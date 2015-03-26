/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
 * Gnu Public License V3 <http://fsf.org/>
 *
 * Entry point file
 *
 */

#include "angharad.h"

/**
 * main function
 * Forks and run the server in the child process
 * if the server crashes, restart it
 * 
 * Note: the fork/waitpd is a temporary solution 
 * until I find a way to bypass the server crash 
 * when the usb resets the arduino,
 * leading to input/output error right after...
 */
int main (int argc, char **argv) {
  pid_t result;
  int status, nb_restart, server_result;
  
  // Config variables
  struct config_elements * config = malloc(sizeof(struct config_elements));
  
  global_handler_variable = ANGHARAD_RUNNING;

  // Catch end signals to make a clean exit
  signal (SIGQUIT, exit_handler);
  signal (SIGINT, exit_handler);
  signal (SIGTERM, exit_handler);
  signal (SIGHUP, exit_handler);

  if (argc>1) {
    log_message(LOG_INFO, "Starting angharad server");
    if (!build_config(argv[1], config)) {
      exit_server(&config, ANGHARAD_ERROR);
    }
  
    if (config->auto_restart) {
      result = fork();
      if(result == 0) {
        server_result = server(config);
        if (server_result) {
          log_message(LOG_INFO, "Error running server");
          exit_server(&config, ANGHARAD_ERROR);
        }
      }
      
      if(result < 0) {
        log_message(LOG_INFO, "Error initial fork");
        exit_server(&config, ANGHARAD_ERROR);
      }
      
      for(nb_restart=1; global_handler_variable == ANGHARAD_RUNNING; nb_restart++) {
        status = 0;
        waitpid(-1, &status, 0);
        if(!WIFEXITED(status)) {
          sleep(5);
          result = fork();
          if(result == 0) {
            log_message(LOG_INFO, "Restart server after unexpected exit (%dth time)", nb_restart);
            server_result = server(config);
            if (server_result) {
              log_message(LOG_INFO, "Error running server");
              exit_server(&config, ANGHARAD_ERROR);
            }
          }
          if(result < 0) {
            log_message(LOG_INFO, "Server crashed and unable to restart");
            exit_server(&config, ANGHARAD_ERROR);
          }
        }
      }
    } else {
      server_result = server(config);
      if (server_result) {
        log_message(LOG_INFO, "Error running server");
        exit_server(&config, ANGHARAD_ERROR);
      }
    }
  } else {
    log_message(LOG_INFO, "No config file specified");
    exit_server(&config, ANGHARAD_ERROR);
  }
  exit_server(&config, ANGHARAD_STOP);
  return 0;
}

/**
 * handles signal catch to exit properly when ^C is used for example
 */
void exit_handler(int signal) {
  log_message(LOG_INFO, "Angharad caught a stop or kill signal (%d), exiting", signal);
  global_handler_variable = ANGHARAD_STOP;
}

/**
 * Exit properly the server by closing opened connections, databases and files
 */
void exit_server(struct config_elements ** config, int exit_value) {
  int i;
  
  if (config != NULL && *config != NULL) {
    // Cleaning data
    for (i=0; i<(*config)->nb_terminal; i++) {
      pthread_mutex_destroy(&(*config)->terminal[i]->lock);
      close_device((*config)->terminal[i]);
      free((*config)->terminal[i]->element);
      (*config)->terminal[i]->element = NULL;
      free((*config)->terminal[i]);
      (*config)->terminal[i] = NULL;
    }
    free((*config)->terminal);
    (*config)->terminal = NULL;
    
    sqlite3_close((*config)->sqlite3_db);
    (*config)->sqlite3_db = NULL;
    
    MHD_stop_daemon ((*config)->daemon);
    (*config)->daemon = NULL;

    free(*config);
    (*config) = NULL;
  }
  exit(exit_value);
}

/**
 * server function
 * initializes the application, run the http server and the scheduler
 */
int server(struct config_elements * config) {
  
  time_t now;
  struct tm ts;
  pthread_t thread_scheduler;
  int thread_ret_scheduler = 0, thread_detach_scheduler = 0, duration = 0, i = 0;
  
  config->daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION, 
                              config->tcp_port, NULL, NULL, &angharad_rest_webservice, (void *)config, 
                              MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, 
                              MHD_OPTION_END);
  
  for (i = 0; i < config->nb_terminal; i++) {
    if (connect_device(config->terminal[i], config->terminal, config->nb_terminal) == -1) {
      log_message(LOG_WARNING, "Error connecting device %s, using uri: %s", config->terminal[i]->name, config->terminal[i]->uri);
    } else {
      log_message(LOG_INFO, "Device %s connected", config->terminal[i]->name);
      if (init_device_status(config->sqlite3_db, config->terminal[i])) {
        log_message(LOG_INFO, "Device %s initialized", config->terminal[i]->name);
      } else {
        log_message(LOG_WARNING, "Error initializing device %s", config->terminal[i]->name);
      }
    }
  }

  if (NULL == config->daemon) {
    log_message(LOG_WARNING, "Error starting http daemon on port %d", config->tcp_port);
    return 1;
  } else {
    log_message(LOG_INFO, "Start listening on port %d", config->tcp_port);
  }

  while (global_handler_variable == ANGHARAD_RUNNING) {
    thread_ret_scheduler = pthread_create(&thread_scheduler, NULL, thread_scheduler_run, (void *)config);
    thread_detach_scheduler = pthread_detach(thread_scheduler);
    if (thread_ret_scheduler || thread_detach_scheduler) {
      log_message(LOG_WARNING, "Error creating or detaching scheduler thread, return code: %d, detach code: %d",
                  thread_ret_scheduler, thread_detach_scheduler);
    }
    time(&now);
    ts = *localtime(&now);
    duration = (unsigned int)(60-ts.tm_sec);
    sleep(duration);
  }
  exit_server(&config, global_handler_variable);
    
  return (0);
}

/**
 * Initialize the application
 * Read the config file, get mandatory variables and devices, fill the output variables
 * Open sqlite file
 */
int build_config(char * config_file, struct config_elements * config) {
  config_t cfg;
  config_setting_t *root, *cfg_devices;
  const char * cur_prefix, * cur_name, * cur_dbpath, * cur_uri, * cur_type, * cur_scriptpath, * cur_config_path, * cur_user_path,
              * cur_command_line, * cur_log_path;
  int count, i, serial_baud, rc;

  pthread_mutexattr_t mutexattr;
  
  config_init(&cfg);
  
  if (!config_read_file(&cfg, config_file)) {
    log_message(LOG_ERR, "\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return 0;
  }
  
  // Get auto_restart parameter
  if (!config_lookup_bool(&cfg, "auto_restart", &config->auto_restart)) {
    log_message(LOG_ERR, "Error config file, auto_restart not found");
    config_destroy(&cfg);
    return 0;
  }
  
  // Get Port number to listen to
  if (!config_lookup_int(&cfg, "port", &(config->tcp_port))) {
    log_message(LOG_ERR, "Error config file, port not found");
    config_destroy(&cfg);
    return 0;
  }
  
  // Get prefix url
  if (!config_lookup_string(&cfg, "prefix", &cur_prefix)) {
    log_message(LOG_ERR, "Error config file, prefix not found");
    config_destroy(&cfg);
    return 0;
  }
  snprintf(config->url_prefix, WORDLENGTH*sizeof(char), "%s", cur_prefix);
  
  // Get sqlite file path and open it
  if (!config_lookup_string(&cfg, "dbpath", &cur_dbpath)) {
    log_message(LOG_ERR, "Error config file, dbpath not found");
    config_destroy(&cfg);
    return 0;
  } else {
    rc = sqlite3_open_v2(cur_dbpath, &config->sqlite3_db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK && config->sqlite3_db != NULL) {
      log_message(LOG_ERR, "Database error: %s\n", sqlite3_errmsg(config->sqlite3_db));
      sqlite3_close(config->sqlite3_db);
      config_destroy(&cfg);
      return 0;
    }
  }
  
  if (!config_lookup_string(&cfg, "dbpath_archive", &cur_dbpath)) {
    log_message(LOG_WARNING, "Warning config file, dbpath_archive not found");
    strcpy(config->db_archive_path, "");
  } else {
    snprintf(config->db_archive_path, MSGLENGTH*sizeof(char), "%s", cur_dbpath);
  }
    
  if (!config_lookup_string(&cfg, "scriptpath", &cur_scriptpath)) {
    log_message(LOG_WARNING, "Warning config file, scriptpath not found");
  } else {
    snprintf(config->script_path, MSGLENGTH*sizeof(char), "%s", cur_scriptpath);
  }
    
  // Get device list
  root = config_root_setting(&cfg);
  cfg_devices = config_setting_get_member(root, "devices");
  config->nb_terminal = 0;
  config->terminal = NULL;
  
  if (cfg_devices != NULL) {
    count = config_setting_length(cfg_devices);
    for (i=0; i < count; i++) {
      config_setting_t * cfg_device = config_setting_get_elem(cfg_devices, i);
      if ((config_setting_lookup_string(cfg_device, "name", &cur_name) &&
          config_setting_lookup_string(cfg_device, "type", &cur_type) &&
          config_setting_lookup_string(cfg_device, "uri", &cur_uri))) {
        if (config->nb_terminal == 0) {
          config->terminal = malloc(sizeof(device *));
        } else {
          config->terminal = realloc(config->terminal, (config->nb_terminal+1)*sizeof(device *));
        }
        
        config->terminal[config->nb_terminal] = malloc(sizeof(device));
        config->terminal[config->nb_terminal]->enabled = 0;
        config->terminal[config->nb_terminal]->display[0] = 0;
        config->terminal[config->nb_terminal]->type = TYPE_NONE;
        config->terminal[config->nb_terminal]->uri[0] = 0;
        snprintf(config->terminal[config->nb_terminal]->name, WORDLENGTH*sizeof(char), "%s", cur_name);

        // Set up mutual exclusion so that this thread has priority
        pthread_mutexattr_init ( &mutexattr );
        pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( &config->terminal[config->nb_terminal]->lock, &mutexattr );
        pthread_mutexattr_destroy( &mutexattr );
        
        if (pthread_mutex_init(&config->terminal[config->nb_terminal]->lock, NULL) != 0) {
          log_message(LOG_ERR, "Impossible to initialize Mutex Lock for %s", config->terminal[config->nb_terminal]->name);
        }
        
        snprintf(config->terminal[config->nb_terminal]->uri, WORDLENGTH*sizeof(char), "%s", cur_uri);

        if (0 == strncmp("serial", cur_type, WORDLENGTH)) {
          config->terminal[config->nb_terminal]->element = malloc(sizeof(struct _arduino_device));
          
          ((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_baud = 0;
          ((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_fd = -1;
          memset(((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_file, '\0', WORDLENGTH*sizeof(char));
          
          config->terminal[config->nb_terminal]->type = TYPE_SERIAL;
          config_setting_lookup_int(cfg_device, "baud", &serial_baud);
          ((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_baud = serial_baud;
          
        } else if (0 == strncmp("zwave", cur_type, WORDLENGTH)) {
          config->terminal[config->nb_terminal]->type = TYPE_ZWAVE;
          
          config->terminal[config->nb_terminal]->element = malloc(sizeof(struct _zwave_device));
          ((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->home_id = UNDEFINED_HOME_ID;
          ((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->init_failed = 0;
          ((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->nodes_list = NULL;
          
          config_setting_lookup_string(cfg_device, "config_path", &cur_config_path);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->config_path, WORDLENGTH*sizeof(char), "%s", cur_config_path);
          
          config_setting_lookup_string(cfg_device, "user_path", &cur_user_path);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->user_path, WORDLENGTH*sizeof(char), "%s", cur_user_path);
          
          config_setting_lookup_string(cfg_device, "command_line", &cur_command_line);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->command_line, WORDLENGTH*sizeof(char), "%s", cur_command_line);
          
          config_setting_lookup_string(cfg_device, "log_path", &cur_log_path);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->log_path, WORDLENGTH*sizeof(char), "%s", cur_log_path);
          
          memset(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->usb_file, '\0', WORDLENGTH*sizeof(char));
        }

        config->nb_terminal++;
      } else {
        log_message(LOG_ERR, "Error reading parameters for device (name='%s', type='%s', uri='%s')", cur_name, cur_type, cur_uri);
      }
    }
  } else {
    log_message(LOG_ERR, "No device specified");
  }
  config_destroy(&cfg);
  return 1;
}
