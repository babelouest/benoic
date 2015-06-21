/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * Entry point file
 *
 * Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU GENERAL PUBLIC LICENSE
 * License as published by the Free Software Foundation;
 * version 3 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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
  
  // Initialize configuration variables
  struct config_elements * config = malloc(sizeof(struct config_elements));
  config->config_file = NULL;
  config->tcp_port = -1;
  config->bind_address = NULL;
  config->url_prefix = NULL;
  config->log_mode = LOG_MODE_NONE;
  config->log_level = LOG_LEVEL_NONE;
  config->log_file = NULL;
  config->script_path = NULL;
  config->auto_restart = -1;
  config->master_db_path = NULL;
  config->master_db = NULL;
  config->archive_db_path = NULL;
  config->archive_db = NULL;
  
  config->terminal = NULL;
  config->nb_terminal = 0;
  config->daemon = NULL;

  global_handler_variable = ANGHARAD_RUNNING;
  // Catch end signals to make a clean exit
  signal (SIGQUIT, exit_handler);
  signal (SIGINT, exit_handler);
  signal (SIGTERM, exit_handler);
  signal (SIGHUP, exit_handler);

  // First we parse command line arguments
  if (!build_config_from_args(argc, argv, config)) {
    fprintf(stderr, "Error reading command-line parameters\n");
    print_help(stderr);
    exit_server(&config, ANGHARAD_ERROR);
  }
  
  // Then we parse configuration file
  // They have lower priority than command line parameters
  if (!build_config_from_file(config)) {
    fprintf(stderr, "Error reading config file\n");
    exit_server(&config, ANGHARAD_ERROR);
  }
  
  // Check if all mandatory configuration variables are present and correctly typed
  if (!check_config(config)) {
    fprintf(stderr, "Error initializing configuration\n");
    exit_server(&config, ANGHARAD_ERROR);
  }

  if (config->auto_restart) {
    result = fork();
    if(result == 0) {
      server_result = server(config);
      if (server_result) {
        log_message(LOG_LEVEL_ERROR, "Error running server");
        exit_server(&config, ANGHARAD_ERROR);
      }
    }
    
    if(result < 0) {
      log_message(LOG_LEVEL_ERROR, "Error initial fork");
      exit_server(&config, ANGHARAD_ERROR);
    }
    
    for(nb_restart=1; global_handler_variable == ANGHARAD_RUNNING || global_handler_variable == ANGHARAD_RESTART; nb_restart++) {
      status = 0;
      waitpid(-1, &status, 0);
      if(!WIFEXITED(status)) {
        sleep(5);
        result = fork();
        if(result == 0) {
          log_message(LOG_LEVEL_INFO, "Restart server after unexpected exit (%dth time)", nb_restart);
          server_result = server(config);
          if (server_result) {
            log_message(LOG_LEVEL_ERROR, "Error running server");
            exit_server(&config, ANGHARAD_ERROR);
          }
        }
        if(result < 0) {
          log_message(LOG_LEVEL_ERROR, "Server crashed and unable to restart");
          exit_server(&config, ANGHARAD_ERROR);
        }
      }
    }
  } else {
    server_result = server(config);
    if (server_result) {
      log_message(LOG_LEVEL_ERROR, "Error running server");
      exit_server(&config, ANGHARAD_ERROR);
    }
  }

  exit_server(&config, ANGHARAD_STOP);
  return 0;
}

/**
 * handles signal catch to exit properly when ^C is used for example
 */
void exit_handler(int signal) {
  log_message(LOG_LEVEL_INFO, "Angharad caught a stop or kill signal (%d), exiting", signal);
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
    
    sqlite3_close((*config)->master_db);
    (*config)->master_db = NULL;

		free((*config)->config_file);
		free((*config)->bind_address);
		free((*config)->url_prefix);
		free((*config)->log_file);
		free((*config)->script_path);
		free((*config)->master_db_path);
		free((*config)->archive_db_path);
    
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
  pthread_t thread_scheduler;
  time_t now;
  struct tm ts;
  
  int thread_ret_scheduler = 0, thread_detach_scheduler = 0;
  int duration = 0;
  struct sockaddr_in bind_address;
  
  if (!init_server(config)) {
    log_message(LOG_LEVEL_ERROR, "Error initializing configuration");
    return 1;
  }
  
  if (global_handler_variable == ANGHARAD_RESTART) {
    // Server has been restarted by user, reset global_handler_variable to normal mode
    global_handler_variable = ANGHARAD_RUNNING;
  }
  
  bind_address.sin_family = AF_INET;
  bind_address.sin_port = htons(config->tcp_port);
  
  if (config->bind_address == NULL || 0 == strcasecmp(config->bind_address, CONFIG_BIND_ANY)) {
    bind_address.sin_addr.s_addr = htonl (INADDR_ANY);
  } else {
    inet_pton(AF_INET, config->bind_address, &(bind_address.sin_addr));
  }
  
  config->daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION, 
                              config->tcp_port, NULL, NULL, &angharad_rest_webservice, (void *)config, 
                              MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL,
                              MHD_OPTION_SOCK_ADDR, &bind_address,
                              MHD_OPTION_END);
  
  if (NULL == config->daemon) {
    log_message(LOG_LEVEL_ERROR, "Error starting http daemon on port %d", config->tcp_port);
    return 1;
  } else {
    log_message(LOG_LEVEL_INFO, "Start listening on port %d", config->tcp_port);
  }

  while (global_handler_variable == ANGHARAD_RUNNING) {
    thread_ret_scheduler = pthread_create(&thread_scheduler, NULL, thread_scheduler_run, (void *)config);
    thread_detach_scheduler = pthread_detach(thread_scheduler);
    if (thread_ret_scheduler || thread_detach_scheduler) {
      log_message(LOG_LEVEL_ERROR, "Error creating or detaching scheduler thread, return code: %d, detach code: %d",
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
 * Initialize the application configuration based on the config file content
 * Read the config file, get mandatory variables and devices
 */
int build_config_from_file(struct config_elements * config) {
  
  config_t cfg;
  config_setting_t *root, *cfg_devices;
  const char * cur_prefix, * cur_bind_address, * cur_name, * cur_dbpath_master, * cur_dbpath_archive, * cur_uri, * cur_type, * cur_scriptpath, 
              * cur_config_path, * cur_user_path, * cur_command_line, * cur_log_path, * cur_log_mode, * cur_log_level, * cur_log_file, * one_log_mode;
  int count, i, serial_baud;

  config_init(&cfg);
  
  if (!config_read_file(&cfg, config->config_file)) {
    fprintf(stderr, "Error parsing log file %s\nOn line %d error: %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return 0;
  }
  
  if (config->tcp_port == -1) {
    // Get Port number to listen to
    config_lookup_int(&cfg, "port", &(config->tcp_port));
  }
  
  if (config->bind_address == NULL) {
    // Get bind_address parameter
    if (config_lookup_string(&cfg, "bind_address", &cur_bind_address)) {
      config->bind_address = malloc(strlen(cur_bind_address)+1);
      strcpy(config->bind_address, cur_bind_address);
    }
  }
  
  if (config->url_prefix == NULL) {
    // Get prefix url
    if (config_lookup_string(&cfg, "url_prefix", &cur_prefix)) {
      config->url_prefix = malloc(strlen(cur_prefix)+1);
      strcpy(config->url_prefix, cur_prefix);
    }
  }

  if (config->master_db_path == NULL) {
    // Get sqlite file path for master database
    if (config_lookup_string(&cfg, "dbpath_master", &cur_dbpath_master)) {
      config->master_db_path = malloc(strlen(cur_dbpath_master)+1);
      strcpy(config->master_db_path, cur_dbpath_master);
    }
  }
  
  if (config->archive_db_path == NULL) {
    // Get sqlite file path for archive database
    if (config_lookup_string(&cfg, "dbpath_archive", &cur_dbpath_archive)) {
      config->archive_db_path = malloc(strlen(cur_dbpath_archive)+1);
      strcpy(config->archive_db_path, cur_dbpath_master);
    }
  }
  
  if (config->script_path == NULL) {
    // Get absolute path for scripts folder
    if (config_lookup_string(&cfg, "scriptpath", &cur_scriptpath)) {
      config->script_path = malloc(strlen(cur_scriptpath)+1);
      strcpy(config->script_path, cur_scriptpath);
    }
  }
  
  if (config->log_mode == LOG_MODE_NONE) {
    // Get log mode
    if (config_lookup_string(&cfg, "log_mode", &cur_log_mode)) {
      one_log_mode = strtok((char *)cur_log_mode, ",");
      while (one_log_mode != NULL) {
        if (0 == strncmp("console", one_log_mode, strlen("console"))) {
          config->log_mode |= LOG_MODE_CONSOLE;
        } else if (0 == strncmp("syslog", one_log_mode, strlen("syslog"))) {
          config->log_mode |= LOG_MODE_SYSLOG;
        } else if (0 == strncmp("file", one_log_mode, strlen("file"))) {
          config->log_mode |= LOG_MODE_FILE;
          // Get log file path
          if (config->log_file == NULL) {
            if (config_lookup_string(&cfg, "log_file", &cur_log_file)) {
              config->log_file = malloc(strlen(cur_log_file)+sizeof(char));
              strncpy(config->log_file, cur_log_file, strlen(cur_log_file));
            }
          }
        }
        one_log_mode = strtok(NULL, ",");
      }
    }
  }
  
  if (config->log_level == LOG_LEVEL_NONE) {
    // Get log level
    if (config_lookup_string(&cfg, "log_level", &cur_log_level)) {
      if (0 == strncmp("NONE", cur_log_level, strlen("NONE"))) {
        config->log_level = LOG_LEVEL_NONE;
      } else if (0 == strncmp("ERROR", cur_log_level, strlen("ERROR"))) {
        config->log_level = LOG_LEVEL_ERROR;
      } else if (0 == strncmp("WARNING", cur_log_level, strlen("WARNING"))) {
        config->log_level = LOG_LEVEL_WARNING;
      } else if (0 == strncmp("INFO", cur_log_level, strlen("INFO"))) {
        config->log_level = LOG_LEVEL_INFO;
      } else if (0 == strncmp("DEBUG", cur_log_level, strlen("DEBUG"))) {
        config->log_level = LOG_LEVEL_DEBUG;
      }
    }
  }
  
  if (config->auto_restart == -1) {
    // Get auto_restart parameter
    config_lookup_bool(&cfg, "auto_restart", &config->auto_restart);
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
          config->terminal = malloc(sizeof(struct _device *));
        } else {
          config->terminal = realloc(config->terminal, (config->nb_terminal+1)*sizeof(struct _device *));
        }
        
        config->terminal[config->nb_terminal] = NULL;
        config->terminal[config->nb_terminal] = malloc(sizeof(struct _device));
        config->terminal[config->nb_terminal]->enabled = 0;
        config->terminal[config->nb_terminal]->display[0] = 0;
        config->terminal[config->nb_terminal]->type = TYPE_NONE;
        config->terminal[config->nb_terminal]->uri[0] = 0;
        
        snprintf(config->terminal[config->nb_terminal]->name, WORDLENGTH*sizeof(char), "%s", cur_name);
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
      }
    }
  }
  config_destroy(&cfg);
  return 1;
}

/**
 * Initialize the application configuration based on the command line parameters
 */
int build_config_from_args(int argc, char ** argv, struct config_elements * config) {
  int next_option, str_len;
  const char * short_options = "c::p::b::u::d::a::s::m::l::f::r::h::";
  char * tmp = NULL, * to_free = NULL, * one_log_mode = NULL;
  static const struct option long_options[]= {
    {"config-file", optional_argument,NULL, 'c'},
    {"port", optional_argument,NULL, 'p'},
    {"bind-address", optional_argument,NULL, 'b'},
    {"url-prefix", optional_argument,NULL, 'u'},
    {"master-db-path", optional_argument,NULL, 'd'},
    {"archive-db-path", optional_argument,NULL, 'a'},
    {"script-path", optional_argument,NULL, 's'},
    {"log-mode", optional_argument,NULL, 'm'},
    {"log-level", optional_argument,NULL, 'l'},
    {"log-file", optional_argument,NULL, 'f'},
    {"auto-restart", optional_argument,NULL, 'r'},
    {"help", optional_argument,NULL, 'h'},
    {NULL, 0, NULL, 0}
  };
  
  if (config != NULL) {
    do {
      next_option = getopt_long(argc, argv, short_options, long_options, NULL);
      
      switch (next_option) {
        case 'c':
          if (optarg != NULL) {
            config->config_file = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo config file specified\n");
            return 0;
          }
          break;
        case 'p':
          if (optarg != NULL) {
            config->tcp_port = strtol(optarg, NULL, 10);
            if (config->tcp_port <= 0 || config->tcp_port > 65535) {
              fprintf(stderr, "Error!\nInvalid TCP Port number\n\tPlease specify an integer value between 1 and 65535");
              return 0;
            }
          } else {
            fprintf(stderr, "Error!\nNo TCP Port number specified\n");
            return 0;
          }
          break;
        case 'b':
          if (optarg != NULL) {
            config->bind_address = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo address specified\n");
            return 0;
          }
          break;
        case 'u':
          if (optarg != NULL) {
            config->url_prefix = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo URL prefix specified\n");
            return 0;
          }
          break;
        case 'd':
          if (optarg != NULL) {
            config->master_db_path = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo master database path specified\n");
            return 0;
          }
          break;
        case 'a':
          if (optarg != NULL) {
            config->archive_db_path = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo archive db path specified\n");
            return 0;
          }
          break;
        case 's':
          if (optarg != NULL) {
            config->script_path = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo script path specified\n");
            return 0;
          }
          break;
        case 'm':
          if (optarg != NULL) {
            str_len = strlen(optarg);
            tmp = malloc(str_len+sizeof(char));
            memset(tmp, 0, str_len+sizeof(char));
            to_free = tmp;
            strncpy(tmp, optarg, str_len);
            one_log_mode = strtok(tmp, ",");
            while (one_log_mode != NULL) {
              if (0 == strncmp("console", one_log_mode, strlen("console"))) {
                config->log_mode |= LOG_MODE_CONSOLE;
              } else if (0 == strncmp("syslog", one_log_mode, strlen("syslog"))) {
                config->log_mode |= LOG_MODE_SYSLOG;
              } else if (0 == strncmp("file", one_log_mode, strlen("file"))) {
                config->log_mode |= LOG_MODE_FILE;
              }
              one_log_mode = strtok(NULL, ",");
            }
            free(to_free);
          } else {
            fprintf(stderr, "Error!\nNo mode specified\n");
            return 0;
          }
          break;
        case 'l':
          if (optarg != NULL) {
            str_len = strlen(optarg);
            to_free = malloc(str_len+sizeof(char));
            strncpy(to_free, optarg, str_len);
            if (0 == strncmp("NONE", to_free, strlen("NONE"))) {
              config->log_level = LOG_LEVEL_NONE;
            } else if (0 == strncmp("ERROR", to_free, strlen("ERROR"))) {
              config->log_level = LOG_LEVEL_ERROR;
            } else if (0 == strncmp("WARNING", to_free, strlen("WARNING"))) {
              config->log_level = LOG_LEVEL_WARNING;
            } else if (0 == strncmp("INFO", to_free, strlen("INFO"))) {
              config->log_level = LOG_LEVEL_INFO;
            } else if (0 == strncmp("DEBUG", to_free, strlen("DEBUG"))) {
              config->log_level = LOG_LEVEL_DEBUG;
            }
            free(to_free);
          } else {
            fprintf(stderr, "Error!\nNo log level specified\n");
            return 0;
          }
          break;
        case 'f':
          if (optarg != NULL) {
            config->log_file = strdup(optarg);
          } else {
            fprintf(stderr, "Error!\nNo log file specified\n");
            return 0;
          }
          break;
        case 'r':
          config->auto_restart = 1;
          break;
        case 'h':
          print_help(stdout);
          exit_server(&config, ANGHARAD_STOP);
          break;
      }
      
    } while (next_option != -1);
    
    // If no config file specified, look for config file in $HOME/ANGHARAD_CONFIG_FILE_HOME, then ANGHARAD_CONFIG_FILE_ETC
    // If none exists, exit failure
    if (config->config_file == NULL) {
      str_len = snprintf(NULL, 0, "%s/%s", getenv("HOME"), ANGHARAD_CONFIG_FILE_HOME);
      tmp = malloc(str_len+sizeof(char));
      snprintf(tmp, (str_len+1), "%s/%s", getenv("HOME"), ANGHARAD_CONFIG_FILE_HOME);
      if (access(tmp, R_OK) == -1) {
        free(tmp);
        // File non existent or non accessible in $HOME/.angharad.conf, try ANGHARAD_CONFIG_FILE_ETC
        if ((access(ANGHARAD_CONFIG_FILE_ETC, R_OK) != -1)) {
          config->config_file = strdup(ANGHARAD_CONFIG_FILE_ETC);
        } else {
          fprintf(stderr, "No configuration file found, please specify a configuration file path\n");
          print_help(stderr);
          exit_server(&config, ANGHARAD_ERROR);
        }
      } else {
        config->config_file = tmp;
      }
    }
    
    return 1;
  } else {
    return 0;
  }
  
}

/**
 * Check if all mandatory configuration parameters are present and correct
 * Initialize some parameters with default value if not set
 */
int check_config(struct config_elements * config) {

  if (config->tcp_port == -1) {
    fprintf(stderr, "Error, you must specify a port number\n");
    print_help(stderr);
    return 0;
  }
  
  if (config->bind_address == NULL) {
    config->bind_address = strdup(CONFIG_BIND_ANY);
  }
  
  if (config->url_prefix == NULL) {
    config->url_prefix = strdup("");
  }
  
  if (config->log_mode == LOG_MODE_NONE) {
    config->log_mode = LOG_MODE_CONSOLE;
  }
  
  if (config->log_level == LOG_LEVEL_NONE) {
    config->log_level = LOG_LEVEL_ERROR;
  }
  
  if (config->log_mode == LOG_MODE_FILE && config->log_file == NULL) {
    fprintf(stderr, "Error, you must specify a log file if log mode is set to file\n");
    print_help(stderr);
    return 0;
  }
  
  if (config->master_db_path == NULL) {
    fprintf(stderr, "Error, you must specify a file path for master database\n");
    print_help(stderr);
    return 0;
  }
  
  return 1;
}

int init_server(struct config_elements * config) {
  int i = 0;
  pthread_mutexattr_t mutexattr;

  // Initialize Log with configuration variables
  write_log(config->log_mode, config->log_level, config->log_file, LOG_LEVEL_INFO, "Starting Angharad Server");
  log_message(LOG_LEVEL_INFO, "Initializing configuration, using confuration file %s", config->config_file);
  
  // Open master Database
  if (sqlite3_open_v2(config->master_db_path, &config->master_db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
    log_message(LOG_LEVEL_ERROR, "Database error: %s", sqlite3_errmsg(config->master_db));
    sqlite3_close(config->master_db);
    return 0;
  }
  
  // Connect each device
  for (i = 0; i < config->nb_terminal; i++) {

    // Initialize MUTEX for each device
    pthread_mutexattr_init ( &mutexattr );
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
    if (pthread_mutex_init(&(config->terminal[i]->lock), &mutexattr) != 0) {
      log_message(LOG_LEVEL_ERROR, "Impossible to initialize Mutex Lock for %s", config->terminal[config->nb_terminal]->name);
    }
    pthread_mutexattr_destroy( &mutexattr );
    
    if (connect_device(config->terminal[i], config->terminal, config->nb_terminal) == -1) {
      log_message(LOG_LEVEL_WARNING, "Error connecting device %s, using uri: %s", config->terminal[i]->name, config->terminal[i]->uri);
      return 0;
    } else {
      log_message(LOG_LEVEL_INFO, "Device %s connected", config->terminal[i]->name);
      if (init_device_status(config->master_db, config->terminal[i])) {
        log_message(LOG_LEVEL_INFO, "Device %s initialized", config->terminal[i]->name);
      } else {
        log_message(LOG_LEVEL_WARNING, "Error initializing device %s", config->terminal[i]->name);
        return 0;
      }
    }
  }
  return 1;
}

/**
 * Print help message to output file specified
 */
void print_help(FILE * output) {
  fprintf(output, "\nAngharad REST Webserver\n");
  fprintf(output, "\n");
  fprintf(output, "Commands house automation devices using a JSON/REST interface\n");
  fprintf(output, "\n");
  fprintf(output, "-c --config-file=PATH\n");
  fprintf(output, "\tConfiguration file\n");
  fprintf(output, "\tIf no configuration is file specified, look for $HOME/%s, then %s\n", ANGHARAD_CONFIG_FILE_HOME, ANGHARAD_CONFIG_FILE_ETC);
  fprintf(output, "-p --port=PORT\n");
  fprintf(output, "\tSpecify port to listen to\n");
  fprintf(output, "-b --bind-address=ADDRESS\n");
  fprintf(output, "\tIP Address to listen to\n");
  fprintf(output, "\tCan be a specific address, a range, or \"any\"\n");
  fprintf(output, "-u --url-prefix=PREFIX\n");
  fprintf(output, "\tURL prefix\n");
  fprintf(output, "-d --master-databse-path=PATH\n");
  fprintf(output, "\tMaster database location\n");
  fprintf(output, "-a --archive-database-path=PATH\n");
  fprintf(output, "\tArchive database location (optionnal)\n");
  fprintf(output, "-m --log-mode=MODE\n");
  fprintf(output, "\tLog Mode\n");
  fprintf(output, "\tconsole, syslog or file\n");
  fprintf(output, "\tIf you want multiple modes, separate them with a comma \",\"\n");
  fprintf(output, "\tdefault: console\n");
  fprintf(output, "-l --log-level=LEVEL\n");
  fprintf(output, "\tLog level\n");
  fprintf(output, "\tNONE, ERROR, WARNING, INFO, DEBUG\n");
  fprintf(output, "\tdefault: ERROR\n");
  fprintf(output, "-f --log-file=PATH\n");
  fprintf(output, "\tPath for log file if log mode file is specified\n");
  fprintf(output, "-s --script-path=PATH\n");
  fprintf(output, "\tPath for script files folder, where SYSTEM actions script are located\n");
  fprintf(output, "-r --auto-restart\n");
  fprintf(output, "\tResatart automatically the server if a crash occurs\n");
  fprintf(output, "-h --help\n");
  fprintf(output, "\tPrint this message\n\n");
}
