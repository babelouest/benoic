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
  char message[WORDLENGTH+1];
  
  // Config variables
  struct config_elements * config = malloc(sizeof(struct config_elements));
  
  global_handler_variable = ANGHARAD_RUNNING;

  signal (SIGQUIT, exit_handler);
  signal (SIGINT, exit_handler);
  signal (SIGTERM, exit_handler);
  signal (SIGHUP, exit_handler);

  if (argc>1) {
    log_message(LOG_INFO, "Starting angharad server");
    if (!initialize(argv[1], message, config)) {
      log_message(LOG_INFO, message);
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
            snprintf(message, WORDLENGTH, "Restart server after unexpected exit (%dth time)", nb_restart);
            log_message(LOG_INFO, message);
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
  
  char message[MSGLENGTH+1];
  time_t now;
  struct tm ts;
  pthread_t thread_scheduler;
  int thread_ret_scheduler = 0, thread_detach_scheduler = 0, duration = 0;
  
  config->daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION, 
                              config->tcp_port, NULL, NULL, &angharad_rest_webservice, (void *)config, 
                              MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, 
                              MHD_OPTION_END);
  
  if (NULL == config->daemon) {
    snprintf(message, MSGLENGTH, "Error starting http daemon on port %d", config->tcp_port);
    log_message(LOG_INFO, message);
    return 1;
  } else {
    snprintf(message, MSGLENGTH, "Start listening on port %d", config->tcp_port);
    log_message(LOG_INFO, message);
  }

  while (global_handler_variable == ANGHARAD_RUNNING) {
    thread_ret_scheduler = pthread_create(&thread_scheduler, NULL, thread_scheduler_run, (void *)config);
    thread_detach_scheduler = pthread_detach(thread_scheduler);
    if (thread_ret_scheduler || thread_detach_scheduler) {
      snprintf(message, MSGLENGTH, "Error creating or detaching scheduler thread, return code: %d, detach code: %d", thread_ret_scheduler, thread_detach_scheduler);
      log_message(LOG_INFO, message);
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
int initialize(char * config_file, char * message, struct config_elements * config) {
  config_t cfg;
  config_setting_t *root, *cfg_devices;
  const char * cur_prefix, * cur_name, * cur_dbpath, * cur_uri, * cur_type, * cur_scriptpath, * cur_config_path, * cur_user_path, * cur_command_line, * cur_log_path;
  int count, i, serial_baud, rc;

  pthread_mutexattr_t mutexattr;
  
  config_init(&cfg);
  
  if (!config_read_file(&cfg, config_file)) {
    snprintf(message, MSGLENGTH, "\n%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return 0;
  }
  
  // Get auto_restart parameter
  if (!config_lookup_bool(&cfg, "auto_restart", &config->auto_restart)) {
    snprintf(message, MSGLENGTH, "Error config file, auto_restart not found\n");
    config_destroy(&cfg);
    return 0;
  }
  
  // Get Port number to listen to
  if (!config_lookup_int(&cfg, "port", &(config->tcp_port))) {
    snprintf(message, MSGLENGTH, "Error config file, port not found\n");
    config_destroy(&cfg);
    return 0;
  }
  
  // Get prefix url
  if (!config_lookup_string(&cfg, "prefix", &cur_prefix)) {
    snprintf(message, MSGLENGTH, "Error config file, prefix not found\n");
    config_destroy(&cfg);
    return 0;
  }
  snprintf(config->url_prefix, WORDLENGTH, "%s", cur_prefix);
  
  // Get sqlite file path and open it
  if (!config_lookup_string(&cfg, "dbpath", &cur_dbpath)) {
    snprintf(message, MSGLENGTH, "Error config file, dbpath not found\n");
    config_destroy(&cfg);
    return 0;
  } else {
    rc = sqlite3_open_v2(cur_dbpath, &config->sqlite3_db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK && config->sqlite3_db != NULL) {
      snprintf(message, MSGLENGTH, "Database error: %s\n", sqlite3_errmsg(config->sqlite3_db));
      sqlite3_close(config->sqlite3_db);
      config_destroy(&cfg);
      return 0;
    }
  }
  
  if (!config_lookup_string(&cfg, "dbpath_archive", &cur_dbpath)) {
    snprintf(message, MSGLENGTH, "Warning config file, dbpath_archive not found\n");
  } else {
    snprintf(config->db_archive_path, MSGLENGTH, "%s", cur_dbpath);
  }
    
  if (!config_lookup_string(&cfg, "scriptpath", &cur_scriptpath)) {
    snprintf(message, MSGLENGTH, "Warning config file, scriptpath not found\n");
  } else {
    snprintf(config->script_path, MSGLENGTH, "%s", cur_scriptpath);
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
        snprintf(config->terminal[config->nb_terminal]->name, WORDLENGTH, "%s", cur_name);

        // Set up mutual exclusion so that this thread has priority
        pthread_mutexattr_init ( &mutexattr );
        pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
        pthread_mutex_init( &config->terminal[config->nb_terminal]->lock, &mutexattr );
        pthread_mutexattr_destroy( &mutexattr );
        
        if (pthread_mutex_init(&config->terminal[config->nb_terminal]->lock, NULL) != 0) {
          snprintf(message, MSGLENGTH, "Impossible to initialize Mutex Lock for %s", config->terminal[config->nb_terminal]->name);
          log_message(LOG_INFO, message);
        }
        
        snprintf(config->terminal[config->nb_terminal]->uri, WORDLENGTH, "%s", cur_uri);

        if (0 == strncmp("serial", cur_type, WORDLENGTH)) {
          config->terminal[config->nb_terminal]->element = malloc(sizeof(struct _arduino_device));
          
          ((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_baud = 0;
          ((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_fd = -1;
          memset(((struct _arduino_device *) config->terminal[config->nb_terminal]->element)->serial_file, '\0', WORDLENGTH+1);
          
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
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->config_path, WORDLENGTH, "%s", cur_config_path);
          
          config_setting_lookup_string(cfg_device, "user_path", &cur_user_path);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->user_path, WORDLENGTH, "%s", cur_user_path);
          
          config_setting_lookup_string(cfg_device, "command_line", &cur_command_line);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->command_line, WORDLENGTH, "%s", cur_command_line);
          
          config_setting_lookup_string(cfg_device, "log_path", &cur_log_path);
          snprintf(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->log_path, WORDLENGTH, "%s", cur_log_path);
          
          memset(((struct _zwave_device *) config->terminal[config->nb_terminal]->element)->usb_file, '\0', WORDLENGTH+1);
        }

        if (connect_device(config->terminal[config->nb_terminal], config->terminal, config->nb_terminal) == -1) {
          snprintf(message, MSGLENGTH, "Error connecting device %s, using uri: %s", config->terminal[config->nb_terminal]->name, config->terminal[config->nb_terminal]->uri);
          log_message(LOG_INFO, message);
        } else {
          snprintf(message, MSGLENGTH, "Device %s connected", config->terminal[config->nb_terminal]->name);
          log_message(LOG_INFO, message);
          if (init_device_status(config->sqlite3_db, config->terminal[config->nb_terminal])) {
            snprintf(message, MSGLENGTH, "Device %s initialized", config->terminal[config->nb_terminal]->name);
            log_message(LOG_INFO, message);
          } else {
            snprintf(message, MSGLENGTH, "Error initializing device %s", config->terminal[config->nb_terminal]->name);
            log_message(LOG_INFO, message);
          }
        }
        config->nb_terminal++;
      } else {
        snprintf(message, MSGLENGTH, "Error reading parameters for device (name='%s', type='%s', uri='%s')", cur_name, cur_type, cur_uri);
        log_message(LOG_INFO, message);
      }
    }
  } else {
    snprintf(message, MSGLENGTH, "No device specified\n");
  }
  config_destroy(&cfg);
  return 1;
}

/**
 * Main libmicrohttpd answer callback function
 * url format: /PREFIX/COMMAND/DEVICE[/PARAM1[/PARAM2[/1]]]
 * examples:
 * Get indoor temperature on sensor 0 from device DEV1: /PREFIX/SENSOR/DEV1/TEMPINT/0
 * Set switcher 3 to ON on DEV1 : /PREFIX/SETSWITCH/DEV1/3/1
 * Get switcher 2 state on DEV2 : /PREFIX/GETSWITCH/DEV2/2
 * Get forced switcher 2 state on DEV2 : /PREFIX/GETSWITCH/DEV2/2/1
 * etc.
 */
int angharad_rest_webservice (void *cls, struct MHD_Connection *connection,
                  const char *url, const char *method,
                  const char *version, const char *upload_data,
                  size_t *upload_data_size, void **con_cls) {
  
  char delim[] = "/";
  char * prefix = NULL;
  char * command = NULL, * device = NULL, * sensor = NULL, * switcher = NULL, 
    * status = NULL, * force = NULL, * action = NULL, * script = NULL, *schedule = NULL, 
    * heater_name = NULL, * heat_enabled = NULL, * heat_value = NULL, 
    * dimmer_name = NULL, * dimmer_value = NULL, * to_free = NULL, 
    * start_date = NULL, * epoch_from_str = NULL;
  char * page = NULL, buffer[2*MSGLENGTH+1];
  char * saveptr;
  heater heat_status;
  char sanitized[WORDLENGTH+1];
  struct MHD_Response *response;
  int ret, urllength = strlen(url);
  int result;
  float sensor_value;
  int iforce;
  int i_heat_enabled;
  int epoch_from;
  float f_heat_value;
  int i_dimmer_value;
  struct _device * cur_terminal = NULL;
  struct connection_info_struct *con_info = *con_cls;
  struct sockaddr *so_client = MHD_get_connection_info (connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
  int tf_len;
  struct config_elements * config = (struct config_elements *) cls;
  struct connection_info_struct * con_info_post = NULL;
  char urlcpy[urllength+1];
  
  // Post data structs
  struct _device * cur_device;
  struct _switcher * cur_switch;
  struct _sensor * cur_sensor;
  struct _heater * cur_heater;
  struct _dimmer * cur_dimmer;
  struct _action * cur_action;
  struct _script * cur_script;
  struct _schedule * cur_schedule;
  
  snprintf(urlcpy, urllength+1, "%s", url);
  prefix = strtok_r( urlcpy, delim, &saveptr );
  
  if (NULL == *con_cls) {
    
    if (0 == strcmp (method, "POST")) {
      con_info_post = malloc (sizeof (struct connection_info_struct));
      if (NULL == con_info_post) {
        return MHD_NO;
      }
      command = strtok_r( NULL, delim, &saveptr );
      if (0 == strcmp("SETDEVICEDATA", command)) {
        con_info_post->data = malloc(sizeof(struct _device));
        memset(((struct _device *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_DEVICE;
      } else if (0 == strcmp("SETSWITCHDATA", command)) {
        con_info_post->data = malloc(sizeof(struct _switcher));
        memset(((struct _switcher *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SWITCH;
      } else if (0 == strcmp("SETSENSORDATA", command)) {
        con_info_post->data = malloc(sizeof(struct _sensor));
        memset(((struct _sensor *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SENSOR;
      } else if (0 == strcmp("SETHEATERDATA", command)) {
        con_info_post->data = malloc(sizeof(struct _heater));
        memset(((struct _heater *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_HEATER;
      } else if (0 == strcmp("SETDIMMERDATA", command)) {
        con_info_post->data = malloc(sizeof(struct _dimmer));
        memset(((struct _dimmer *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_DIMMER;
      } else if (0 == strcmp("SETACTION", command) || 0 == strcmp("ADDACTION", command)) {
        con_info_post->data = malloc(sizeof(struct _action));
        ((struct _action *)con_info_post->data)->id = 0;
        memset(((struct _action *)con_info_post->data)->name, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->device, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->switcher, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->heater, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->dimmer, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->params, 0, MSGLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_ACTION;
      } else if (0 == strcmp("SETSCRIPT", command) || 0 == strcmp("ADDSCRIPT", command)) {
        con_info_post->data = malloc(sizeof(struct _script));
        ((struct _script *)con_info_post->data)->id = 0;
        strcpy(((struct _script *)con_info_post->data)->name, "");
        strcpy(((struct _script *)con_info_post->data)->device, "");
        ((struct _script *)con_info_post->data)->enabled = 0;
        strcpy(((struct _script *)con_info_post->data)->actions, "");
        memset(((struct _script *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SCRIPT;
      } else if (0 == strcmp("SETSCHEDULE", command) || 0 == strcmp("ADDSCHEDULE", command)) {
        con_info_post->data = malloc(sizeof(struct _schedule));
        ((struct _schedule *)con_info_post->data)->id = 0;
        ((struct _schedule *)con_info_post->data)->next_time = 0;
        ((struct _schedule *)con_info_post->data)->script = 0;
        ((struct _schedule *)con_info_post->data)->repeat_schedule = -1;
        ((struct _schedule *)con_info_post->data)->repeat_schedule_value = 0;
        ((struct _schedule *)con_info_post->data)->remove_after_done = 0;
        strcpy(((struct _schedule *)con_info_post->data)->name, "");
        strcpy(((struct _schedule *)con_info_post->data)->device, "");
        memset(((struct _schedule *)con_info_post->data)->tags, 0, WORDLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SCHEDULE;
      }
      con_info_post->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post, (void *) con_info_post);
      if (NULL == con_info_post->postprocessor) {
        free(con_info_post);
        return MHD_NO;
      }
      con_info_post->connectiontype = HTTPPOST;
      *con_cls = (void *) con_info_post;
      return MHD_YES;
    }
  }
  
  page = malloc((MSGLENGTH*2+1)*sizeof(char));
  // url parsing
  if (prefix == NULL) {
    // wrong url
    char * sanitize = malloc((2*strlen(url)+1)*sizeof(char));
    sanitize_json_string((char *)url, sanitize, MSGLENGTH);
    snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"can not parse url\",\"url\":\"%s\",\"size\":%d}}", sanitize, urllength);
    free(sanitize);
  } else if (0 == strcmp(prefix, config->url_prefix)) {
    if (0 == strcmp(method, "GET")) {
      command = strtok_r( NULL, delim, &saveptr );
      
      if (command != NULL) {
        if (0 == strcmp(command, "DEVICES")) {
          to_free = get_devices(config->sqlite3_db, config->terminal, config->nb_terminal);
          tf_len = (16+strlen(to_free))*sizeof(char);
          free(page);
          page = malloc(tf_len);
          snprintf(page, tf_len, "{\"devices\":[%s]}", to_free);
          free(to_free);
          to_free = NULL;
        } else if ( 0 == strcmp(command, "ACTIONS")) {
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_actions(config->sqlite3_db, device);
          if (to_free != NULL) {
            tf_len = (16+strlen(to_free))*sizeof(char);
            free(page);
            page = malloc(tf_len);
            snprintf(page, tf_len, "{\"actions\":[%s]}", to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting actions\"}");
          }
        } else if ( 0 == strcmp(command, "SCRIPTS")) {
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_scripts(config->sqlite3_db, device);
          if (to_free != NULL) {
            tf_len = (15+strlen(to_free))*sizeof(char);
            free(page);
            page = malloc(tf_len);
            snprintf(page, tf_len, "{\"scripts\":[%s]}", to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting scripts\"}");
          }
        } else if ( 0 == strcmp(command, "SCRIPT")) {
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no script id specified\"}}");
          } else {
            to_free = get_script(config->sqlite3_db, script, 1);
            if (to_free != NULL) {
              snprintf(page, MSGLENGTH, "{\"result\":%s}", to_free);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting script\"}");
            }
            free(to_free);
          }
        } else if ( 0 == strcmp(command, "RUNSCRIPT")) {
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no script id specified\"}}");
          } else {
            if (run_script(config->sqlite3_db, config->terminal, config->nb_terminal, config->script_path, script)) {
              snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error running script\"}");
            }
          }
        } else if ( 0 == strcmp(command, "SCHEDULES")) {
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_schedules(config->sqlite3_db, device);
          if (to_free != NULL) {
            tf_len = (15+strlen(to_free))*sizeof(char);
            free(page);
            page = malloc(tf_len);
            snprintf(page, tf_len, "{\"result\":[%s]}", to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting schedules\"}");
          }
        } else if ( 0 == strcmp(command, "ENABLESCHEDULE")) {
          schedule = strtok_r( NULL, delim, &saveptr );
          status = strtok_r( NULL, delim, &saveptr );
          if (schedule != NULL && status != NULL && enable_schedule(config->sqlite3_db, schedule, status, buffer)) {
            snprintf(page, MSGLENGTH, "{\"result\":%s}", buffer);
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting schedule\"}");
          }
        } else if ( 0 == strcmp("DELETEACTION", command) ) {
          action = strtok_r( NULL, delim, &saveptr );
          if (delete_action(config->sqlite3_db, action)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error deleting action\"}");
          }
        } else if ( 0 == strcmp("DELETESCRIPT", command) ) {
          script = strtok_r( NULL, delim, &saveptr );
          if (delete_script(config->sqlite3_db, script)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error deleting script\"}");
          }
        } else if ( 0 == strcmp("DELETESCHEDULE", command) ) {
          schedule = strtok_r( NULL, delim, &saveptr );
          if (delete_schedule(config->sqlite3_db, schedule)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error deleting schedule\"}");
          }
        } else if ( 0 == strcmp("ARCHIVE", command) ) {
          epoch_from_str = strtok_r( NULL, delim, &saveptr );
          epoch_from = strtol(epoch_from_str, NULL, 10);
          if (archive(config->sqlite3_db, config->db_archive_path, epoch_from)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\",\"archive_from\":%d}", epoch_from);
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"archive_from\":%d}", epoch_from);
          }
        } else {
          device = strtok_r( NULL, delim, &saveptr );
          if (device == NULL) {
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no device\"}}");
          } else {
            cur_terminal = get_device_from_name(device, config->terminal, config->nb_terminal);
            if (cur_terminal == NULL) {
              snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"device not found\",\"device\":\"%s\"}}", device);
            } else if ( 0 == strcmp("RESET", command) ) {
              result = reconnect_device(cur_terminal, config->terminal, config->nb_terminal);
              sanitize_json_string(device, sanitized, WORDLENGTH);
              if (result && init_device_status(config->sqlite3_db, cur_terminal)) {
                snprintf(buffer, MSGLENGTH, "Device %s initialized", cur_terminal->name);
                log_message(LOG_INFO, buffer);
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"reset\",\"device\":\"%s\",\"response\":%s,\"initialization\":true}}", sanitized, (result!=-1)?"true":"false");
              } else {
                snprintf(buffer, MSGLENGTH, "Error initializing device %s", cur_terminal->name);
                log_message(LOG_INFO, buffer);
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"reset\",\"device\":\"%s\",\"response\":%s,\"initialization\":false}}", sanitized, (result!=-1)?"true":"false");
              }
            } else if (!cur_terminal->enabled) {
              snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"device disabled\",\"device\":\"%s\"}}", cur_terminal->name);
            } else if (is_connected(cur_terminal)) {
              if ( 0 == strcmp("MARCO", command) ) {
                result = send_heartbeat(cur_terminal);
                if (result) {
                  sanitize_json_string(device, sanitized, WORDLENGTH);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"heartbeat\",\"device\":\"%s\",\"response\":\"POLO\"}}", sanitized);
                } else {
                  sanitize_json_string(device, sanitized, WORDLENGTH);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"heartbeat\",\"device\":\"%s\",\"response\":\"ERROR\"}}", sanitized);
                }
              } else if ( 0 == strcmp("NAME", command) ) {
                get_name(cur_terminal, buffer);
                sanitize_json_string(device, sanitized, WORDLENGTH);
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"name\",\"device\":\"%s\",\"response\":\"%s\"}}", sanitized, buffer);
              } else if ( 0 == strcmp("OVERVIEW", command) ) {
                free(page);
                page = NULL;
                page = get_overview(config->sqlite3_db, cur_terminal);
              } else if ( 0 == strcmp("REFRESH", command) ) {
                free(page);
                page = NULL;
                page = get_refresh(config->sqlite3_db, cur_terminal);
              } else if ( 0 == strcmp("GETSWITCH", command) ) {
                switcher = strtok_r( NULL, delim, &saveptr );
                force = strtok_r( NULL, delim, &saveptr );
                iforce=(force != NULL && (0 == strcmp("1", force)))?1:0;
                if (switcher != NULL) {
                  result = get_switch_state(cur_terminal, switcher, iforce);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"get_status\",\"device\":\"%s\",\"switcher\":\"%s\",\"response\":%d}}", device, switcher, result);
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no switcher specified\",\"command\":\"%s\"}}", command);
                }
              } else if ( 0 == strcmp("SETSWITCH", command) ) {
                switcher = strtok_r( NULL, delim, &saveptr );
                status = strtok_r( NULL, delim, &saveptr );
                result = set_switch_state(cur_terminal, switcher, (status != NULL && (0 == strcmp("1", status))?1:0));
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"set_status\",\"device\":\"%s\",\"switcher\":\"%s\",\"status\":\"%s\",\"response\":%d}}", device, switcher, status, result);
                if (!save_startup_switch_status(config->sqlite3_db, cur_terminal->name, switcher, (status != NULL && (0 == strcmp("1", status))?1:0))) {
                  log_message(LOG_INFO, "Error saving switcher status in the database");
                }
              } else if ( 0 == strcmp("TOGGLESWITCH", command) ) {
                switcher = strtok_r( NULL, delim, &saveptr );
                result = toggle_switch_state(cur_terminal, switcher);
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"toggle_status\",\"device\":\"%s\",\"switcher\":\"%s\",\"response\":%d}}", device, switcher, result);
                if (!save_startup_switch_status(config->sqlite3_db, cur_terminal->name, switcher, result)) {
                  log_message(LOG_INFO, "Error saving switcher status in the database");
                }
              } else if ( 0 == strncmp("SETDIMMER", command, strlen("SETDIMMER")) ) {
                dimmer_name = strtok_r( NULL, delim, &saveptr );
                dimmer_value = strtok_r( NULL, delim, &saveptr );
                if (dimmer_name != NULL && dimmer_value != NULL) {
                  i_dimmer_value = strtol(dimmer_value, NULL, 10);
                  result = set_dimmer_value(cur_terminal, dimmer_name, i_dimmer_value);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"set_dimmer\",\"device\":\"%s\",\"response\":%d}}", cur_terminal->name, result);
                  if (!save_startup_dimmer_value(config->sqlite3_db, cur_terminal->name, dimmer_name, i_dimmer_value)) {
                    log_message(LOG_INFO, "Error saving switcher status in the database");
                  }
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"wrong command\"}}");
                }
              } else if ( 0 == strncmp("GETDIMMER", command, strlen("GETDIMMER")) ) {
                dimmer_name = strtok_r( NULL, delim, &saveptr );
                if (dimmer_name != NULL) {
                  i_dimmer_value = get_dimmer_value(cur_terminal, dimmer_name);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"get_dimmer\",\"device\":\"%s\",\"response\":%d}}", cur_terminal->name, i_dimmer_value);
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"wrong command\"}}");
                }
              } else if ( 0 == strcmp("SENSOR", command) ) {
                sensor = strtok_r( NULL, delim, &saveptr );
                if (sensor != NULL && 
                  ((0 == strncmp(TEMPEXT, sensor, strlen(TEMPEXT))) || 
                  (0 == strncmp(TEMPINT, sensor, strlen(TEMPINT))) || 
                  (0 == strncmp(HUMINT, sensor, strlen(HUMINT))))) {
                  force = strtok_r( NULL, delim, &saveptr );
                  iforce=(force != NULL && (0 == strcmp("1", force)))?1:0;
                  sensor_value = get_sensor_value(cur_terminal, sensor, iforce);
                  sanitize_json_string(device, sanitized, WORDLENGTH);
                  if (sensor_value == ERROR_SENSOR) {
                    snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"sensor\",\"device\":\"%s\",\"response\":\"error\"}}", sanitized);
                  } else {
                    snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"sensor\",\"device\":\"%s\",\"response\":%.2f}}", sanitized, sensor_value);
                  }
                } else {
                  sanitize_json_string(sensor, sanitized, WORDLENGTH);
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"unknown sensor\",\"sensor\":\"%s\"}}", sanitized);
                }
              } else if ( 0 == strncmp("GETHEATER", command, strlen("GETHEATER")) ) {
                heater_name = strtok_r( NULL, delim, &saveptr );
                if (get_heater(cur_terminal, heater_name, buffer)) {
                  if (parse_heater(config->sqlite3_db, cur_terminal->name, heater_name, buffer, &heat_status)) {
                    snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"get_heater\",\"device\":\"%s\",\"response\":{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}}}", cur_terminal->name, heat_status.name, heat_status.display, heat_status.enabled?"true":"false", heat_status.on?"true":"false", heat_status.set?"true":"false", heat_status.heat_max_value, heat_status.unit);
                  } else {
                    snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"error parsing results\"}}");
                  }
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"error getting heater status\"}}");
                }
              } else if ( 0 == strncmp("SETHEATER", command, strlen("SETHEATER")) ) {
                heater_name = strtok_r( NULL, delim, &saveptr );
                heat_enabled = strtok_r( NULL, delim, &saveptr );
                heat_value = strtok_r( NULL, delim, &saveptr );
                if (heater_name != NULL && heat_enabled != NULL && (heat_value != NULL || 0==strcmp("0",heat_enabled))) {
                  i_heat_enabled = 0==strcmp("1",heat_enabled)?1:0;
                  f_heat_value = strtof(heat_value, NULL);
                  if (set_heater(cur_terminal, heater_name, i_heat_enabled, f_heat_value, buffer)) {
                    if (parse_heater(config->sqlite3_db, cur_terminal->name, heater_name, buffer, &heat_status)) {
                      if (!save_startup_heater_status(config->sqlite3_db, cur_terminal->name, heater_name, i_heat_enabled, f_heat_value)) {
                        log_message(LOG_INFO, "Error saving heater status in the database");
                      }
                      snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"set_heater\",\"device\":\"%s\",\"response\":{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}}}", cur_terminal->name, heat_status.name, heat_status.display, heat_status.enabled?"true":"false", heat_status.on?"true":"false", heat_status.set?"true":"false", heat_status.heat_max_value, heat_status.unit);
                    } else {
                      snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"error parsing heater results\"}}");
                    }
                  } else {
                    snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"error getting results\",\"message\":{\"command\":\"set_heater\",\"device\":\"%s\",\"heater\":\"%s\"}}", device, heater_name);
                  }
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"wrong command\"}}");
                }
              } else if ( 0 == strncmp("MONITOR", command, strlen("MONITOR")) ) {
                switcher = strtok_r( NULL, delim, &saveptr );
                sensor = strtok_r( NULL, delim, &saveptr );
                start_date = strtok_r( NULL, delim, &saveptr );
                to_free = get_monitor(config->sqlite3_db, device, switcher, sensor, start_date);
                if (to_free != NULL) {
                  tf_len = strlen(to_free);
                  free(page);
                  page = malloc((tf_len + 1) * sizeof(char));
                  strcpy(page, to_free);
                  free(to_free);
                } else {
                  snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting monitor values\"}");
                }
              } else {
                sanitize_json_string(command, sanitized, WORDLENGTH);
                snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"unknown command\",\"command\":\"%s\"}}", sanitized);
              }
            } else {
              sanitize_json_string(device, sanitized, WORDLENGTH);
              snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"device not connected\",\"device\":\"%s\"}}", sanitized);
            }
          }
        }
      } else {
        snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"empty command\"}}");
      }
    } else if (0 == strcmp(method, "POST")) {
      command = strtok_r( NULL, delim, &saveptr );
      if (command != NULL) {
        if (*upload_data_size != 0) {
          MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
          *upload_data_size = 0;
          free(page);
          return MHD_YES;
        } else {
          if (0 == strcmp("SETDEVICEDATA", command)) {
            cur_device = (struct _device *)con_info->data;
            cur_terminal = get_device_from_name(cur_device->name, config->terminal, config->nb_terminal);
            to_free = set_device_data(config->sqlite3_db, *cur_device);
            if (to_free != NULL) {
              cur_terminal->enabled = cur_device->enabled;
              free(page);
              page = malloc((12+strlen(to_free))*sizeof(char));
              snprintf(page, 12+strlen(to_free), "{\"device\":%s}", to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting device\"}");
            }
          } else if (0 == strcmp("SETSWITCHDATA", command)) {
            cur_switch = (struct _switcher *)con_info->data;
            to_free = set_switch_data(config->sqlite3_db, *cur_switch);
            if (to_free != NULL) {
              free(page);
              page = malloc((14+strlen(to_free))*sizeof(char));
              snprintf(page, 14+strlen(to_free), "{\"switcher\":%s}", to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting switcher\"}");
            }
          } else if (0 == strcmp("SETSENSORDATA", command)) {
            cur_sensor = (struct _sensor *)con_info->data;
            to_free = set_sensor_data(config->sqlite3_db, *cur_sensor);
            if (to_free != NULL) {
              free(page);
              page = malloc((12+strlen(to_free))*sizeof(char));
              snprintf(page, 12+strlen(to_free), "{\"sensor\":%s}", to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting sensor\"}");
            }
          } else if (0 == strcmp("SETHEATERDATA", command)) {
            cur_heater = (struct _heater *)con_info->data;
            to_free = set_heater_data(config->sqlite3_db, *cur_heater);
            if (to_free != NULL) {
              free(page);
              page = malloc((12+strlen(to_free))*sizeof(char));
              snprintf(page, 12+strlen(to_free), "{\"heater\":%s}", to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting heater\"}");
            }
          } else if (0 == strcmp("SETDIMMERDATA", command)) {
            cur_dimmer = (struct _dimmer *)con_info->data;
            to_free = set_dimmer_data(config->sqlite3_db, *cur_dimmer);
            if (to_free != NULL) {
              free(page);
              page = malloc((12+strlen(to_free))*sizeof(char));
              snprintf(page, 12+strlen(to_free), "{\"dimmer\":%s}", to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting dimmer\"}");
            }
          } else if (0 == strcmp("ADDACTION", command)) {
            cur_action = (struct _action *)con_info->data;
            if (add_action(config->sqlite3_db, *cur_action, buffer)) {
              snprintf(page, MSGLENGTH*2, "{\"action\":%s}", buffer);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error adding action\"}");
            }
          } else if (0 == strcmp("SETACTION", command)) {
            cur_action = (struct _action *)con_info->data;
            if (set_action(config->sqlite3_db, *cur_action, buffer)) {
              snprintf(page, MSGLENGTH, "{\"action\":%s}", buffer);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting action\"}");
            }
          } else if (0 == strcmp("ADDSCRIPT", command)) {
            cur_script = (struct _script *)con_info->data;
            if (add_script(config->sqlite3_db, *cur_script, buffer)) {
              snprintf(page, MSGLENGTH*2, "{\"script\":%s}", buffer);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error adding script\"}");
            }
          } else if (0 == strcmp("SETSCRIPT", command)) {
            cur_script = (struct _script *)con_info->data;
            if (set_script(config->sqlite3_db, *cur_script, buffer)) {
              snprintf(page, MSGLENGTH*2, "{\"script\":%s}", buffer);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting script\"}");
            }
          } else if (0 == strcmp("ADDSCHEDULE", command)) {
            cur_schedule = (struct _schedule *)con_info->data;
            if (add_schedule(config->sqlite3_db, *cur_schedule, buffer)) {
              snprintf(page, MSGLENGTH*2, "{\"schedule\":%s}", buffer);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error adding schedule\"}");
            }
          } else if (0 == strcmp("SETSCHEDULE", command)) {
            cur_schedule = (struct _schedule *)con_info->data;
            if (set_schedule(config->sqlite3_db, *cur_schedule, buffer)) {
              snprintf(page, MSGLENGTH*2, "{\"schedule\":%s}", buffer);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting schedule\"}");
            }
          } else {
            sanitize_json_string(command, sanitized, WORDLENGTH);
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"unknown command\",\"command\":\"%s\"}}", sanitized);
          }
        }
      } else {
        snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"empty command\"}}");
      }
    } else {
      snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"wrong http method\"}}");
    }
  } else {
    sanitize_json_string(prefix, sanitized, WORDLENGTH);
    snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"unknown prefix\",\"prefix\":\"%s\"}}", sanitized);
  }
  
  journal(config->sqlite3_db, inet_ntoa(((struct sockaddr_in *)so_client)->sin_addr), url, page);
  response = MHD_create_response_from_buffer (strlen (page), (void *) page, MHD_RESPMEM_MUST_FREE );
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  snprintf(buffer, MSGLENGTH, "End execution of angharad_rest_webservice, from %s, url: %s", inet_ntoa(((struct sockaddr_in *)so_client)->sin_addr), url);
  log_message(LOG_INFO, buffer);
  return ret;
}

/**
 * Send a message to syslog
 * and prints the message to stdout if DEBUG mode
 */
void log_message(int type, const char * message, ...) {
	va_list argp;
  #ifdef DEBUG
  char * out = NULL;
  int out_len = 0;
  #endif
  
  if (message != NULL) {
    va_start(argp, message);
    openlog("Angharad", LOG_PID|LOG_CONS, LOG_USER);
    vsyslog( type, message, argp );
    closelog();
  #ifdef DEBUG
    out_len = strlen(message)+1;
    out = malloc(out_len+1);
    snprintf(out, out_len+1, "%s\n", message);
    vfprintf(stdout, out, argp);
    free(out);
  #endif
    va_end(argp);
  }
}

/**
 * Safe string replace function
 * Based on Laird Shaw's replace_str function (http://creativeandcritical.net/str-replace-c/)
 */
int str_replace(char * source, char * target, size_t len, char * old, char * new) {
  char *r;
  const char *p, *q;
  size_t oldlen = strlen(old);
  size_t count, retlen, newlen = strlen(new);
  
  if (source == NULL || target == NULL || old == NULL) {
    return 0;
  }

  if (oldlen != newlen) {
    for (count = 0, p = source; (q = strstr(p, old)) != NULL; p = q + oldlen)
      count++;
    /* this is undefined if p - source > PTRDIFF_MAX */
    retlen = p - source + strlen(p) + count * (newlen - oldlen);
  } else {
    retlen = strlen(source);
  }

  if (retlen + 1 > len) {
    return 0;
  }

  for (r = target, p = source; (q = strstr(p, old)) != NULL; p = q + oldlen) {
    /* this is undefined if q - p > PTRDIFF_MAX */
    ptrdiff_t l = q - p;
    memcpy(r, p, l);
    r += l;
    memcpy(r, new, newlen);
    r += newlen;
  }
  strcpy(r, p);

  return 1;
}

/**
 * Sanitize special characters for json output
 */
int sanitize_json_string(char * source, char * target, size_t len) {
  char tmp1[len], tmp2[len];
  unsigned int tab_size = 8, i;
  char *old[] = {"\\", "\b", "\f", "\n", "\r", "\t", "\v", "\""};
  char *new[] = {"\\\\", "\\b", "\\f", "\\n", "\\r", "\\t", "\\v", "\\\""};
  
  snprintf(tmp1, len, "%s", source);
  for (i = 0; i < tab_size; i++) {
    if (str_replace(tmp1, tmp2, len, old[i], new[i])) {
      snprintf(tmp1, len, "%s", tmp2);
    } else {
      return 0;
    }
  }
  snprintf(target, len, "%s", tmp1);
  return 1;
}

/**
 * Parse the POST data
 */
int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size) {
  struct connection_info_struct *con_info = coninfo_cls;
  device    * cur_device;
  switcher       * cur_switch;
  sensor    * cur_sensor;
  heater    * cur_heater;
  dimmer    * cur_dimmer;
  action    * cur_action;
  script    * cur_script;
  schedule  * cur_schedule;

  switch (con_info->data_type) {
    case DATA_DEVICE:
      cur_device = (struct _device*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_device->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_device->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_device->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_device->tags, MSGLENGTH, "%s", data);
        }
      }
      break;
    case DATA_SWITCH:
      cur_switch = (struct _switcher *)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_switch->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_switch->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_switch->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_switch->type=strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_switch->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_switch->monitored=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored_every")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_switch->monitored_every=strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_switch->tags, MSGLENGTH, "%s", data);
        }
      }
      break;
    case DATA_SENSOR:
      cur_sensor = (struct _sensor*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_sensor->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_sensor->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_sensor->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "unit")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_sensor->unit, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_sensor->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_sensor->monitored=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored_every")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_sensor->monitored_every=strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_sensor->tags, MSGLENGTH, "%s", data);
        }
      }
    break;
    case DATA_HEATER:
      cur_heater = (struct _heater*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_heater->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_heater->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_heater->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_heater->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "unit")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_heater->unit, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_heater->tags, MSGLENGTH, "%s", data);
        }
      }
    break;
    case DATA_DIMMER:
      cur_dimmer = (struct _dimmer*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_dimmer->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_dimmer->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_dimmer->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_dimmer->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_dimmer->tags, MSGLENGTH, "%s", data);
        }
      }
    break;
    case DATA_ACTION:
      cur_action = (struct _action*)con_info->data;
      if (0 == strcmp (key, "id")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->id = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->type = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "switcher")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->switcher, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "dimmer")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->dimmer, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "heater")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->heater, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "params")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_action->params, MSGLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_action->tags, MSGLENGTH, "%s", data);
        }
      }
      break;
    case DATA_SCRIPT:
      cur_script = (struct _script*)con_info->data;
      if (0 == strcmp (key, "id")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_script->id = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_script->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_script->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_script->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "actions")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_script->actions, MSGLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_script->tags, MSGLENGTH, "%s", data);
        }
      }
      break;
    case DATA_SCHEDULE:
      cur_schedule = (struct _schedule*)con_info->data;
      if (0 == strcmp (key, "id")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->id = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_schedule->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "next_time")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->next_time = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "repeat_schedule")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->repeat_schedule = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "repeat_schedule_value")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->repeat_schedule_value = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "remove_after_done")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->remove_after_done = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "script")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->script = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_schedule->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_schedule->tags, MSGLENGTH, "%s", data);
        }
      }
      break;
    default:
      log_message(LOG_INFO, "Unknown data type");
      break;
  }
  return MHD_YES;
}

/**
 * Mark the request completed so angharad_rest_webservice can keep going
 */
void request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe) {
  struct connection_info_struct *con_info = *con_cls;
  if (NULL == con_info) {
    return;
  }
  if (con_info->connectiontype == HTTPPOST) {
    MHD_destroy_post_processor (con_info->postprocessor);
  }
  free(con_info->data);
  free(con_info);
  *con_cls = NULL;
}

/*
 * Logs the commands, its origin (remote ip address or internal schedule) and its result into the database journal table
 */
int journal(sqlite3 * sqlite3_db, const char * origin, const char * command, const char * result) {
  static char sql_query[MSGLENGTH+1];
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_journal (jo_date, jo_origin, jo_command, jo_result) VALUES (strftime('%%s', 'now'), '%q', '%q', '%q')", origin, command, result);
  return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
}

/**
 * Counts the number of digits of a integer
 */
int num_digits (int n) {
  if (n == 0) return 1;
  return floor (log10 (abs (n))) + 1;
}

/**
 * Counts the number of digits of a long
 */
int num_digits_l (long n) {
  if (n == 0) return 1;
  return floor (log10 (abs (n))) + 1;
}
