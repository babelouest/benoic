#include "angharad.h"

// TODO: remove global variables
device ** terminal = NULL;
unsigned int nb_terminal=0;

int cfg_port;
char cfg_prefix[WORDLENGTH+1];

sqlite3 * sqlite3_db;
// END Global variables

static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind,
                         const char *key, const char *filename,
                         const char *content_type,
                         const char *transfer_encoding,
                         const char *data,
                         uint64_t off,
                         size_t size);
static int angharad_rest_webservice (void *cls, struct MHD_Connection *connection,
                                     const char *url, const char *method,
                                     const char *version, const char *upload_data,
                                     size_t *upload_data_size, void **con_cls);
static void request_completed (void *cls, struct MHD_Connection *connection,
                               void **con_cls, enum MHD_RequestTerminationCode toe);

/**
 * main function
 * initializes the application, run the http server and the scheduler
 */
int main(int argc, char* argv[]) {
  
  struct MHD_Daemon *daemon;
  char message[MSGLENGTH+1];
  int i;
  time_t now;
  struct tm ts;
  unsigned int duration;
  
  log_message(LOG_INFO, "Starting angharad server");
  if (argc>1) {
    if (!initialize(argv[1], message)) {
      log_message(LOG_INFO, message);
      for (i=0; i<nb_terminal; i++) {
        free(terminal[i]);
      }
      
      free(terminal);
      exit(-1);
    }
  } else {
    log_message(LOG_INFO, "No config file specified\n");
    exit(-1);
  }
  
  daemon = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_SELECT_INTERNALLY, 
                              cfg_port, NULL, NULL, &angharad_rest_webservice, NULL, 
                              MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, 
                              MHD_OPTION_END);
  
  if (NULL == daemon) {
    return 1;
  } else {
    snprintf(message, MSGLENGTH, "Start listening on port %d", cfg_port);
    log_message(LOG_INFO, message);
  }
  while (1) {
    run_scheduler(sqlite3_db, terminal, nb_terminal);
    time(&now);
    ts = *localtime(&now);
    duration = (unsigned int)(60-ts.tm_sec);
    sleep(duration);
  }
  MHD_stop_daemon (daemon);
  
  for (i=0; i<nb_terminal; i++) {
    close_device(terminal[i]);
    free(terminal[i]);
  }
  sqlite3_close(sqlite3_db);
  free(terminal);
  
  return (0);
}

/**
 * Initialize the application
 * Read the config file, get mandatory variables and devices
 * Open sqlite file
 */
int initialize(char * config_file, char * message) {
  config_t cfg;
  config_setting_t *root, *cfg_devices;
  config_init(&cfg);
  
  const char * cur_prefix, * cur_name, * cur_dbpath, * cur_uri, * cur_type;
  
  int count, i, serial_baud, rc;
  
  if (!config_read_file(&cfg, config_file)) {
    snprintf(message, MSGLENGTH, "\n%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return 0;
  }
  
  // Get Port number to listen to
  if (!config_lookup_int(&cfg, "port", &cfg_port)) {
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
  snprintf(cfg_prefix, WORDLENGTH, "%s", cur_prefix);
  
  // Get sqlite file path and open it
  if (!config_lookup_string(&cfg, "dbpath", &cur_dbpath)) {
    snprintf(message, MSGLENGTH, "Error config file, dbpath not found\n");
    config_destroy(&cfg);
    return 0;
  } else {
    rc = sqlite3_open_v2(cur_dbpath, &sqlite3_db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK && sqlite3_db != NULL) {
      snprintf(message, MSGLENGTH, "Database error: %s\n", sqlite3_errmsg(sqlite3_db));
      sqlite3_close(sqlite3_db);
      config_destroy(&cfg);
      return 0;
    }
  }
  
  // Get device list
  root = config_root_setting(&cfg);
  cfg_devices = config_setting_get_member(root, "devices");
  if (cfg_devices != NULL) {
    count = config_setting_length(cfg_devices);
    for (i=0; i < count; i++) {
      config_setting_t * cfg_device = config_setting_get_elem(cfg_devices, i);
      if ((config_setting_lookup_string(cfg_device, "name", &cur_name) &&
          config_setting_lookup_string(cfg_device, "type", &cur_type) &&
          config_setting_lookup_string(cfg_device, "uri", &cur_uri))) {
        if (nb_terminal==0) {
          terminal = malloc(sizeof(device *));
        } else {
          terminal = realloc(terminal, sizeof(device *));
        }
        terminal[nb_terminal] = malloc(sizeof(device));
        snprintf(terminal[nb_terminal]->name, WORDLENGTH, "%s", cur_name);
        if (0 == strncmp("serial", cur_type, WORDLENGTH)) {
          terminal[nb_terminal]->type = TYPE_SERIAL;
          config_setting_lookup_int(cfg_device, "baud", &serial_baud);
          terminal[nb_terminal]->serial_baud = serial_baud;
        }
        snprintf(terminal[nb_terminal]->uri, WORDLENGTH, "%s", cur_uri);
        if (connect_device(terminal[nb_terminal]) == -1) {
          snprintf(message, MSGLENGTH, "Error connecting device %s, using uri: %s", terminal[nb_terminal]->name, terminal[nb_terminal]->uri);
          log_message(LOG_INFO, message);
        } else {
          snprintf(message, MSGLENGTH, "Device %s connected", terminal[nb_terminal]->name);
          log_message(LOG_INFO, message);
          if (init_device_status(sqlite3_db, terminal[nb_terminal])) {
            snprintf(message, MSGLENGTH, "Device %s initialized", terminal[nb_terminal]->name);
            log_message(LOG_INFO, message);
          } else {
            snprintf(message, MSGLENGTH, "Error initializing device %s", terminal[nb_terminal]->name);
            log_message(LOG_INFO, message);
          }
          if (pthread_mutex_init(&terminal[nb_terminal]->lock, NULL) != 0) {
            snprintf(message, MSGLENGTH, "Impossible to initialize Mutex Lock for %s", terminal[nb_terminal]->name);
            log_message(LOG_INFO, message);
          }
        }
        nb_terminal++;
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
 * Get indoor temperature from device DEV1: /PREFIX/SENSOR/DEV1/TEMPINT
 * Set pin 3 to ON on DEV1 : /PREFIX/SETPIN/DEV1/3/1
 * Get pin 2 state on DEV2 : /PREFIX/GETPIN/DEV2/2
 * Get forced pin 2 state on DEV2 : /PREFIX/GETPIN/DEV2/2/1
 */
static int angharad_rest_webservice (void *cls, struct MHD_Connection *connection,
                  const char *url, const char *method,
                  const char *version, const char *upload_data,
                  size_t *upload_data_size, void **con_cls) {
  
  char delim[] = "/";
  char * prefix = NULL;
  char * command = NULL, * device = NULL, * sensor = NULL, * pin = NULL, * status = NULL, * force = NULL, * action = NULL, * script = NULL, *schedule = NULL, * heater_name = NULL, * heat_enabled = NULL, * heat_value = NULL, * light_name = NULL, * light_on = NULL, * to_free = NULL;
  char * page = malloc((MSGLENGTH+1)*sizeof(char)), * urlcpy = malloc((MSGLENGTH+1)*sizeof(char)), * command_result = malloc((MSGLENGTH+1)*sizeof(char));
  char * source, * saveptr;
  heater heat_status;
  char sanitized[WORDLENGTH+1];
  struct MHD_Response *response;
  int ret, urllength = strlen(url);
  int result;
  float sensor_value;
  int iforce;
  int i_heat_enabled;
  float f_heat_value;
  struct _device * cur_terminal = NULL;
  struct connection_info_struct *con_info = *con_cls;
  struct sockaddr *so_client = MHD_get_connection_info (connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
    
  char * debug = malloc((MSGLENGTH+1)*sizeof(char));
  
  // Post data structs
  struct _device * cur_device;
  struct _pin * cur_pin;
  struct _sensor * cur_sensor;
  struct _light * cur_light;
  struct _heater * cur_heater;
  struct _action * cur_action;
  struct _script * cur_script;
  struct _schedule * cur_schedule;
  
  snprintf(urlcpy, MSGLENGTH, "%s", url);
  source = urlcpy;
  
  prefix = strtok_r( source, delim, &saveptr );
  
  if (NULL == *con_cls) {
    struct connection_info_struct *con_info;
    con_info = malloc (sizeof (struct connection_info_struct));
    if (NULL == con_info) {
      return MHD_NO;
    }
    
    if (0 == strcmp (method, "POST")) {
      command = strtok_r( NULL, delim, &saveptr );
      if (0 == strcmp("SETDEVICEDATA", command)) {
        con_info->data = malloc(sizeof(struct _device));
        con_info->data_type = DATA_DEVICE;
      } else if (0 == strcmp("SETPINDATA", command)) {
        con_info->data = malloc(sizeof(struct _pin));
        con_info->data_type = DATA_PIN;
      } else if (0 == strcmp("SETSENSORDATA", command)) {
        con_info->data = malloc(sizeof(struct _sensor));
        con_info->data_type = DATA_SENSOR;
      } else if (0 == strcmp("SETLIGHTDATA", command)) {
        con_info->data = malloc(sizeof(struct _light));
        con_info->data_type = DATA_LIGHT;
      } else if (0 == strcmp("SETHEATERDATA", command)) {
        con_info->data = malloc(sizeof(struct _heater));
        con_info->data_type = DATA_HEATER;
      } else if (0 == strcmp("SETACTION", command) || 0 == strcmp("ADDACTION", command)) {
        con_info->data = malloc(sizeof(struct _action));
        ((struct _action *)con_info->data)->id = 0;
        memset(((struct _action *)con_info->data)->name, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info->data)->device, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info->data)->pin, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info->data)->sensor, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info->data)->heater, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info->data)->params, 0, MSGLENGTH*sizeof(char));
        con_info->data_type = DATA_ACTION;
      } else if (0 == strcmp("SETSCRIPT", command) || 0 == strcmp("ADDSCRIPT", command)) {
        con_info->data = malloc(sizeof(struct _script));
        ((struct _script *)con_info->data)->id = 0;
        strcpy(((struct _script *)con_info->data)->name, "");
        strcpy(((struct _script *)con_info->data)->device, "");
        ((struct _script *)con_info->data)->enabled = 0;
        strcpy(((struct _script *)con_info->data)->actions, "");
        con_info->data_type = DATA_SCRIPT;
      } else if (0 == strcmp("SETSCHEDULE", command) || 0 == strcmp("ADDSCHEDULE", command)) {
        con_info->data = malloc(sizeof(struct _schedule));
        ((struct _schedule *)con_info->data)->id = 0;
        ((struct _schedule *)con_info->data)->next_time = 0;
        ((struct _schedule *)con_info->data)->script = 0;
        ((struct _schedule *)con_info->data)->repeat_schedule = -1;
        ((struct _schedule *)con_info->data)->repeat_schedule_value = 0;
        strcpy(((struct _schedule *)con_info->data)->name, "");
        strcpy(((struct _schedule *)con_info->data)->device, "");
        con_info->data_type = DATA_SCHEDULE;
      }
      con_info->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post, (void *) con_info);
      if (NULL == con_info->postprocessor) {
        return MHD_NO;
      }
      con_info->connectiontype = HTTPPOST;
      *con_cls = (void *) con_info;
      return MHD_YES;
    }
  }
  // url parsing
  if (prefix == NULL) {
    // wrong url
    sanitize_json_string(urlcpy, urlcpy, MSGLENGTH);
    snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"can not parse url\",\"url\":\"%s\",\"size\":%d}}", urlcpy, urllength);
  } else if (0 == strcmp(prefix, cfg_prefix)) {
    if (0 == strcmp(method, "GET")) {
      command = strtok_r( NULL, delim, &saveptr );
      
      if (command != NULL) {
        if (0 == strcmp(command, "DEVICES")) {
          to_free = get_devices(sqlite3_db, terminal, nb_terminal);
          page = realloc(page, (16+strlen(to_free))*sizeof(char));
          snprintf(page, MSGLENGTH, "{\"devices\":[%s]}", to_free);
          free(to_free);
          to_free = NULL;
        } else if ( 0 == strcmp(command, "ACTIONS")) {
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_actions(sqlite3_db, device);
          if (to_free != NULL) {
            page = realloc(page, (16+strlen(to_free))*sizeof(char));
            snprintf(page, MSGLENGTH, "{\"actions\":[%s]}", to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting actions\"}");
          }
        } else if ( 0 == strcmp(command, "SCRIPTS")) {
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_scripts(sqlite3_db, device);
          if (to_free != NULL) {
            page = realloc(page, (15+strlen(to_free))*sizeof(char));
            snprintf(page, MSGLENGTH, "{\"scripts\":[%s]}", to_free);
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
            if (get_script(sqlite3_db, script, command_result)) {
              snprintf(page, MSGLENGTH, "{\"result\":%s", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting script\"}");
            }
          }
        } else if ( 0 == strcmp(command, "RUNSCRIPT")) {
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no script id specified\"}}");
          } else {
            if (run_script(sqlite3_db, terminal, nb_terminal, script)) {
              snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error running script\"}");
            }
          }
        } else if ( 0 == strcmp(command, "SCHEDULES")) {
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_schedules(sqlite3_db, device);
          if (to_free != NULL) {
            page = realloc(page, (15+strlen(to_free))*sizeof(char));
            snprintf(page, MSGLENGTH, "{\"result\":[%s]}", to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error getting schedules\"}");
          }
        } else if ( 0 == strcmp(command, "ENABLESCHEDULE")) {
          schedule = strtok_r( NULL, delim, &saveptr );
          status = strtok_r( NULL, delim, &saveptr );
          if (schedule != NULL && status != NULL && enable_schedule(sqlite3_db, schedule, status, command_result)) {
            snprintf(page, MSGLENGTH, "{\"result\":%s}", command_result);
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting schedule\"}");
          }
        } else if ( 0 == strcmp("DELETEACTION", command) ) {
          action = strtok_r( NULL, delim, &saveptr );
          if (delete_action(sqlite3_db, action)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error deleting action\"}");
          }
        } else if ( 0 == strcmp("DELETESCRIPT", command) ) {
          script = strtok_r( NULL, delim, &saveptr );
          if (delete_script(sqlite3_db, script)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error deleting script\"}");
          }
        } else if ( 0 == strcmp("DELETESCHEDULE", command) ) {
          schedule = strtok_r( NULL, delim, &saveptr );
          if (delete_schedule(sqlite3_db, schedule)) {
            snprintf(page, MSGLENGTH, "{\"result\":\"ok\"}");
          } else {
            snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error deleting schedule\"}");
          }
        } else {
          device = strtok_r( NULL, delim, &saveptr );
          if (device == NULL) {
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no device\"}}");
          } else {
            cur_terminal = get_device_from_name(device, terminal, nb_terminal);
            if (cur_terminal == NULL) {
              snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"device not found\",\"device\":\"%s\"}}", device);
            } else if ( 0 == strcmp("RESET", command) ) {
              result = reconnect_device(cur_terminal);
              sanitize_json_string(device, sanitized, WORDLENGTH);
              if (result && init_device_status(sqlite3_db, terminal[nb_terminal])) {
                snprintf(debug, MSGLENGTH, "Device %s initialized", terminal[nb_terminal]->name);
                log_message(LOG_INFO, debug);
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"reset\",\"device\":\"%s\",\"response\":%s,\"initialization\":true}}", sanitized, (result!=-1)?"true":"false");
              } else {
                snprintf(debug, MSGLENGTH, "Error initializing device %s", terminal[nb_terminal]->name);
                log_message(LOG_INFO, debug);
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
                get_name(cur_terminal, command_result);
                sanitize_json_string(device, sanitized, WORDLENGTH);
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"name\",\"device\":\"%s\",\"response\":\"%s\"}}", sanitized, command_result);
              } else if ( 0 == strcmp("OVERVIEW", command) ) {
                get_overview(cur_terminal, command_result);
                free(page);
                page = NULL;
                page = parse_overview(sqlite3_db, command_result);
              } else if ( 0 == strcmp("REFRESH", command) ) {
                get_refresh(cur_terminal, command_result);
                free(page);
                page = NULL;
                page = parse_overview(sqlite3_db, command_result);
              } else if ( 0 == strcmp("GETPIN", command) ) {
                pin = strtok_r( NULL, delim, &saveptr );
                force = strtok_r( NULL, delim, &saveptr );
                iforce=(force != NULL && (0 == strcmp("1", force)))?1:0;
                if (pin != NULL) {
                  result = get_switch_state(cur_terminal, pin, iforce);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"get_status\",\"device\":\"%s\",\"pin\":\"%s\",\"response\":%d}}", device, pin, result);
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"no pin specified\",\"command\":\"%s\"}}", command);
                }
              } else if ( 0 == strcmp("SETPIN", command) ) {
                pin = strtok_r( NULL, delim, &saveptr );
                status = strtok_r( NULL, delim, &saveptr );
                result = set_switch_state(cur_terminal, pin, (status != NULL && (0 == strcmp("1", status))?1:0));
                snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"set_status\",\"device\":\"%s\",\"pin\":\"%s\",\"status\":\"%s\",\"response\":%d}}", device, pin, status, result);
                if (!set_startup_pin_status(sqlite3_db, cur_terminal->name, pin, (status != NULL && (0 == strcmp("1", status))?1:0))) {
                  log_message(LOG_INFO, "Error saving pin status in the database");
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
                        snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"sensor\",\"device\":\"%s\",\"response\":%.2f}}", sanitized, sensor_value);
                } else {
                  sanitize_json_string(sensor, sanitized, WORDLENGTH);
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"unknown sensor\",\"sensor\":\"%s\"}}", sanitized);
                }
              } else if ( 0 == strncmp("GETHEATER", command, strlen("GETHEATER")) ) {
                heater_name = strtok_r( NULL, delim, &saveptr );
                if (get_heater(cur_terminal, heater_name, command_result)) {
                  if (parse_heater(sqlite3_db, cur_terminal->name, heater_name, command_result, &heat_status)) {
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
                  f_heat_value = i_heat_enabled?strtof(heat_value, NULL):0.0;
                  if (set_heater(cur_terminal, heater_name, i_heat_enabled, f_heat_value, command_result)) {
                    if (parse_heater(sqlite3_db, cur_terminal->name, heater_name, command_result, &heat_status)) {
                      if (!set_startup_heater_status(sqlite3_db, cur_terminal->name, heater_name, i_heat_enabled, f_heat_value)) {
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
              } else if ( 0 == strncmp("SETLIGHT", command, strlen("SETLIGHT")) ) {
                light_name = strtok_r( NULL, delim, &saveptr );
                light_on = strtok_r( NULL, delim, &saveptr );
                if (light_name != NULL && light_on != NULL && (light_on[0] == '0' || light_on[0] == '1')) {
                  result = set_light(cur_terminal, light_name, (light_on[0] == '1'));
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"set_light\",\"device\":\"%s\",\"response\":%d}}", cur_terminal->name, result);
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"wrong command\"}}");
                }
              } else if ( 0 == strncmp("GETLIGHT", command, strlen("GETLIGHT")) ) {
                light_name = strtok_r( NULL, delim, &saveptr );
                if (light_name != NULL) {
                  result = get_light(cur_terminal, light_name);
                  snprintf(page, MSGLENGTH, "{\"result\":{\"command\":\"get_light\",\"device\":\"%s\",\"response\":%d}}", cur_terminal->name, result);
                } else {
                  snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"wrong command\"}}");
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
          MHD_post_process (con_info->postprocessor, upload_data,
                            *upload_data_size);
          *upload_data_size = 0;
          return MHD_YES;
        } else {
          if (0 == strcmp("SETDEVICEDATA", command)) {
            cur_device = (struct _device *)con_info->data;
            cur_terminal = get_device_from_name(cur_device->name, terminal, nb_terminal);
            if (set_device_data(sqlite3_db, *cur_device, command_result)) {
              cur_terminal->enabled = cur_device->enabled;
              snprintf(page, MSGLENGTH, "{\"device\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting device\"}");
            }
          } else if (0 == strcmp("SETPINDATA", command)) {
            cur_pin = (struct _pin *)con_info->data;
            if (set_pin_data(sqlite3_db, *cur_pin, command_result)) {
               snprintf(page, MSGLENGTH, "{\"pin\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting pin\"}");
            }
          } else if (0 == strcmp("SETSENSORDATA", command)) {
            cur_sensor = (struct _sensor *)con_info->data;
            if (set_sensor_data(sqlite3_db, *cur_sensor, command_result)) {
              snprintf(page, MSGLENGTH, "{\"sensor\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting sensor\"}");
            }
          } else if (0 == strcmp("SETLIGHTDATA", command)) {
            cur_light = (struct _light *)con_info->data;
            if (set_light_data(sqlite3_db, *cur_light, command_result)) {
              snprintf(page, MSGLENGTH, "{\"light\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting light\"}");
            }
          } else if (0 == strcmp("SETHEATERDATA", command)) {
            cur_heater = (struct _heater *)con_info->data;
            if (set_heater_data(sqlite3_db, *cur_heater, command_result)) {
              snprintf(page, MSGLENGTH, "{\"heater\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting heater\"}");
            }
          } else if (0 == strcmp("ADDACTION", command)) {
            cur_action = (struct _action *)con_info->data;
            if (add_action(sqlite3_db, *cur_action, command_result)) {
              snprintf(page, MSGLENGTH, "{\"action\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error adding action\"}");
            }
          } else if (0 == strcmp("SETACTION", command)) {
            cur_action = (struct _action *)con_info->data;
            if (set_action(sqlite3_db, *cur_action, command_result)) {
              snprintf(page, MSGLENGTH, "{\"action\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting action\"}");
            }
          } else if (0 == strcmp("ADDSCRIPT", command)) {
            cur_script = (struct _script *)con_info->data;
            if (add_script(sqlite3_db, *cur_script, command_result)) {
              snprintf(page, MSGLENGTH, "{\"script\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error adding script\"}");
            }
          } else if (0 == strcmp("SETSCRIPT", command)) {
            cur_script = (struct _script *)con_info->data;
            if (set_script(sqlite3_db, *cur_script, command_result)) {
              snprintf(page, MSGLENGTH, "{\"script\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting script\"}");
            }
          } else if (0 == strcmp("ADDSCHEDULE", command)) {
            cur_schedule = (struct _schedule *)con_info->data;
            if (add_schedule(sqlite3_db, *cur_schedule, command_result)) {
              snprintf(page, MSGLENGTH, "{\"schedule\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error adding schedule\"}");
            }
          } else if (0 == strcmp("SETSCHEDULE", command)) {
            cur_schedule = (struct _schedule *)con_info->data;
            if (set_schedule(sqlite3_db, *cur_schedule, command_result)) {
              snprintf(page, MSGLENGTH, "{\"schedule\":%s}", command_result);
            } else {
              snprintf(page, MSGLENGTH, "{\"result\":\"error\",\"message\":\"Error setting schedule\"}");
            }
          } else {
            sanitize_json_string(command, sanitized, WORDLENGTH);
            snprintf(page, MSGLENGTH, "{\"syntax_error\":{\"message\":\"unknown command\",\"command\":\"%s\"}}", sanitized);
          }
          free(con_info->data);
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
  
  response = MHD_create_response_from_buffer (strlen (page), (void *) page, MHD_RESPMEM_MUST_COPY );
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  snprintf(debug, MSGLENGTH, "finishing answer_to_connection, url: %s", url);
  log_message(LOG_INFO, debug);
  journal(sqlite3_db, inet_ntoa(((struct sockaddr_in *)so_client)->sin_addr), url, page);
  free(page);
  free(urlcpy);
  free(command_result);
  free(debug);
  return ret;
}

/**
 * Send a message to syslog
 * and prints the message to stdout if DEBUG mode
 */
void log_message(int type, const char * message) {
  openlog("Angharad", LOG_PID|LOG_CONS, LOG_USER);
  syslog( type, message );
  closelog();
#ifdef DEBUG
  fprintf(stdout, "%s\n", message);
#endif
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
static int
iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size) {
  struct connection_info_struct *con_info = coninfo_cls;
  device    * cur_device;
  pin       * cur_pin;
  sensor    * cur_sensor;
  light     * cur_light;
  heater    * cur_heater;
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
      }
      break;
    case DATA_PIN:
      cur_pin = (struct _pin*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_pin->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_pin->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_pin->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_pin->type=strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_pin->enabled=(0==strcmp("true", data))?1:0;
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
      }
    break;
    case DATA_LIGHT:
      cur_light = (struct _light*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_light->name, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_light->device, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_light->display, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_light->enabled=(0==strcmp("true", data))?1:0;
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
      } else if (0 == strcmp (key, "pin")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->pin, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "sensor")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->sensor, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "heater")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->heater, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "params")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_action->params, MSGLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "result_type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->result_value.type = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "result_s")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->result_value.s_value, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "result_i")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->result_value.i_value = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "result_f")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->result_value.f_value = strtof(data, NULL);
        }
      } else if (0 == strcmp (key, "condition_result")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->condition_result = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "condition_type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->condition_value.type = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "condition_s")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_action->condition_value.s_value, WORDLENGTH, "%s", data);
        }
      } else if (0 == strcmp (key, "condition_i")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->condition_value.i_value = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "condition_f")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->condition_value.f_value = strtof(data, NULL);
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
      } else if (0 == strcmp (key, "script")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->script = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          snprintf(cur_schedule->device, WORDLENGTH, "%s", data);
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
static void
request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe) {
  struct connection_info_struct *con_info = *con_cls;
  if (NULL == con_info) {
    return;
  }
  if (con_info->connectiontype == HTTPPOST) {
    MHD_destroy_post_processor (con_info->postprocessor);
  }
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
