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

// JSON templates
static const char json_template_webserver_wrong_url[] = "{\"syntax_error\":{\"message\":\"can not parse url\",\"url\":\"%s\",\"size\":%d}}";
static const char json_template_webserver_devices[] = "{\"devices\":[%s]}";
static const char json_template_webserver_actions[] = "{\"actions\":[%s]}";
static const char json_template_webserver_actions_error[] = "{\"result\":\"error\",\"message\":\"Error getting actions\"}";
static const char json_template_webserver_scripts[] = "{\"scripts\":[%s]}";
static const char json_template_webserver_scripts_error[] = "{\"result\":\"error\",\"message\":\"Error getting scripts\"}";
static const char json_template_webserver_script[] = "{\"result\":%s}";
static const char json_template_webserver_script_error[] = "{\"result\":\"error\",\"message\":\"Error getting script\"}";
static const char json_template_webserver_script_error_id[] = "{\"syntax_error\":{\"message\":\"no script id specified\"}}";
static const char json_template_webserver_script_run[] = "{\"result\":\"ok\"}";
static const char json_template_webserver_script_run_error[] = "{\"result\":\"error\",\"message\":\"Error running script\"}";
static const char json_template_webserver_schedules[] = "{\"result\":[%s]}";
static const char json_template_webserver_schedules_error[] = "{\"result\":\"error\",\"message\":\"Error getting schedules\"}";
static const char json_template_webserver_schedule_enable[] = "{\"result\":%s}";
static const char json_template_webserver_schedule_enable_error[] = "{\"result\":\"error\",\"message\":\"Error setting schedule\"}";
static const char json_template_webserver_action_delete[] = "{\"result\":\"ok\"}";
static const char json_template_webserver_action_delete_error[] = "{\"result\":\"error\",\"message\":\"Error deleting action\"}";
static const char json_template_webserver_script_delete[] = "{\"result\":\"ok\"}";
static const char json_template_webserver_script_delete_error[] = "{\"result\":\"error\",\"message\":\"Error deleting script\"}";
static const char json_template_webserver_schedule_delete[] = "{\"result\":\"ok\"}";
static const char json_template_webserver_schedule_delete_error[] = "{\"result\":\"error\",\"message\":\"Error deleting schedule\"}";
static const char json_template_webserver_archive[] = "{\"result\":\"%s\",\"archive_from\":%d,\"last_archive\":%d,\"archive_running\":%s}";
static const char json_template_webserver_last_archive[] = "{\"result\":\"ok\",\"last_archive\":%d,\"archive_running\":%s}";
static const char json_template_webserver_last_archive_error[] = "{\"result\":\"error\",\"last_archive\":\"no archive database\"}";
static const char json_template_webserver_no_device[] = "{\"syntax_error\":{\"message\":\"no device\"}}";
static const char json_template_webserver_device_not_found[] = "{\"syntax_error\":{\"message\":\"device not found\",\"device\":\"%s\"}}";
static const char json_template_webserver_reset_device[] = "{\"result\":{\"command\":\"reset\",\"device\":\"%s\",\"response\":%s,\"initialization\":%s}}";
static const char json_template_webserver_device_disabled[] = "{\"syntax_error\":{\"message\":\"device disabled\",\"device\":\"%s\"}}";
static const char json_template_webserver_heartbeat[] = "{\"result\":{\"command\":\"heartbeat\",\"device\":\"%s\",\"response\":\"%s\"}}";
static const char json_template_webserver_device_name[] = "{\"result\":{\"command\":\"name\",\"device\":\"%s\",\"response\":\"%s\"}}";
static const char json_template_webserver_getswitch[] = "{\"result\":{\"command\":\"get_status\",\"device\":\"%s\",\"switcher\":\"%s\",\"response\":%d}}";
static const char json_template_webserver_getswitch_error[] = "{\"syntax_error\":{\"message\":\"no switcher specified\",\"command\":\"%s\"}}";
static const char json_template_webserver_setswitch[] = "{\"result\":{\"command\":\"set_status\",\"device\":\"%s\",\"switcher\":\"%s\",\"status\":\"%s\",\"response\":%d}}";
static const char json_template_webserver_toggleswitch[] = "{\"result\":{\"command\":\"toggle_status\",\"device\":\"%s\",\"switcher\":\"%s\",\"response\":%d}}";
static const char json_template_webserver_setdimmer[] = "{\"result\":{\"command\":\"set_dimmer\",\"device\":\"%s\",\"dimmer\":\"%s\",\"response\":%d}}";
static const char json_template_webserver_setdimmer_error[] = "{\"syntax_error\":{\"message\":\"wrong command\"}}";
static const char json_template_webserver_getdimmer[] = "{\"result\":{\"command\":\"get_dimmer\",\"device\":\"%s\",\"dimmer\":\"%s\",\"response\":%d}}";
static const char json_template_webserver_getdimmer_error[] = "{\"syntax_error\":{\"message\":\"wrong command\"}}";
static const char json_template_webserver_sensor_error[] = "{\"result\":{\"command\":\"sensor\",\"device\":\"%s\",\"response\":\"error\"}}";
static const char json_template_webserver_sensor_not_found[] = "{\"syntax_error\":{\"message\":\"unknown sensor\",\"sensor\":\"%s\"}}";
static const char json_template_webserver_sensor[] = "{\"result\":{\"command\":\"sensor\",\"device\":\"%s\",\"response\":%.2f}}";
static const char json_template_webserver_getheater[] = "{\"result\":{\"command\":\"get_heater\",\"device\":\"%s\",\"response\":{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}}}";
static const char json_template_webserver_getheater_error_parsing[] = "{\"syntax_error\":{\"message\":\"error parsing results\"}}";
static const char json_template_webserver_getheater_error[] = "{\"syntax_error\":{\"message\":\"error getting heater status\"}}";
static const char json_template_webserver_setheater[] = "{\"result\":{\"command\":\"set_heater\",\"device\":\"%s\",\"response\":{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}}}";
static const char json_template_webserver_setheater_error_parsing[] = "{\"syntax_error\":{\"message\":\"error parsing heater results\"}}";
static const char json_template_webserver_setheater_error[] = "{\"syntax_error\":{\"message\":\"error getting results\",\"message\":{\"command\":\"set_heater\",\"device\":\"%s\",\"heater\":\"%s\"}}";
static const char json_template_webserver_monitor_error[] = "{\"result\":\"error\",\"message\":\"Error getting monitor values\"}";
static const char json_template_webserver_unknown_command[] = "{\"syntax_error\":{\"message\":\"unknown command\",\"command\":\"%s\"}}";
static const char json_template_webserver_device_not_connected[] = "{\"syntax_error\":{\"message\":\"device not connected\",\"device\":\"%s\"}}";
static const char json_template_webserver_command_empty[] = "{\"syntax_error\":{\"message\":\"empty command\"}}";
static const char json_template_webserver_setdevicedata[] = "{\"device\":%s}";
static const char json_template_webserver_setdevicedata_error[] = "{\"result\":\"error\",\"message\":\"Error setting device\"}";
static const char json_template_webserver_setswitchdata[] = "{\"switcher\":%s}";
static const char json_template_webserver_setswitchdata_error[] = "{\"result\":\"error\",\"message\":\"Error setting switcher\"}";
static const char json_template_webserver_setsensordata[] = "{\"sensor\":%s}";
static const char json_template_webserver_setsensordata_error[] = "{\"result\":\"error\",\"message\":\"Error setting sensor\"}";
static const char json_template_webserver_setheaterdata[] = "{\"heater\":%s}";
static const char json_template_webserver_setheaterdata_error[] = "{\"result\":\"error\",\"message\":\"Error setting heater\"}";
static const char json_template_webserver_setdimmerdata[] = "{\"dimmer\":%s}";
static const char json_template_webserver_setdimmerdata_error[] = "{\"result\":\"error\",\"message\":\"Error setting dimmer\"}";
static const char json_template_webserver_addaction[] = "{\"action\":%s}";
static const char json_template_webserver_addaction_error[] = "{\"result\":\"error\",\"message\":\"Error adding action\"}";
static const char json_template_webserver_setaction[] = "{\"action\":%s}";
static const char json_template_webserver_setaction_error[] = "{\"result\":\"error\",\"message\":\"Error setting action\"}";
static const char json_template_webserver_addscript[] = "{\"script\":%s}";
static const char json_template_webserver_addscript_error[] = "{\"result\":\"error\",\"message\":\"Error adding script\"}";
static const char json_template_webserver_setscript[] = "{\"script\":%s}";
static const char json_template_webserver_setscript_error[] = "{\"result\":\"error\",\"message\":\"Error setting script\"}";
static const char json_template_webserver_addschedule[] = "{\"schedule\":%s}";
static const char json_template_webserver_addschedule_error[] = "{\"result\":\"error\",\"message\":\"Error adding schedule\"}";
static const char json_template_webserver_setschedule[] = "{\"schedule\":%s}";
static const char json_template_webserver_setschedule_error[] = "{\"result\":\"error\",\"message\":\"Error setting schedule\"}";
static const char json_template_webserver_empty_command[] = "{\"syntax_error\":{\"message\":\"empty command\"}}";
static const char json_template_webserver_wrong_http_method[] = "{\"syntax_error\":{\"message\":\"wrong http method\"}}";
static const char json_template_webserver_wrong_prefix[] = "{\"syntax_error\":{\"message\":\"unknown prefix\",\"prefix\":\"%s\"}}";

/**
 * Commands used
 */
// POST commands
#define SETDEVICEDATA   "SETDEVICEDATA"
#define SETSWITCHDATA   "SETSWITCHDATA"
#define SETSENSORDATA   "SETSENSORDATA"
#define SETHEATERDATA   "SETHEATERDATA"
#define SETDIMMERDATA   "SETDIMMERDATA"
#define SETACTION       "SETACTION"
#define SETSCRIPT       "SETSCRIPT"
#define SETSCHEDULE     "SETSCHEDULE"
#define ADDACTION       "ADDACTION"
#define ADDSCRIPT       "ADDSCRIPT"
#define ADDSCHEDULE     "ADDSCHEDULE"

// GET commands
#define DEVICES         "DEVICES"
#define ACTIONS         "ACTIONS"
#define SCRIPTS         "SCRIPTS"
#define SCRIPT          "SCRIPT"
#define RUNSCRIPT       "RUNSCRIPT"
#define SCHEDULES       "SCHEDULES"
#define ENABLESCHEDULE  "ENABLESCHEDULE"
#define DELETEACTION    "DELETEACTION"
#define DELETESCRIPT    "DELETESCRIPT"
#define DELETESCHEDULE  "DELETESCHEDULE"
#define LASTARCHIVE     "LASTARCHIVE"
#define ARCHIVE         "ARCHIVE"
#define RESET           "RESET"
#define HEARTBEAT       "HEARTBEAT"
#define NAME            "NAME"
#define OVERVIEW        "OVERVIEW"
#define REFRESH         "REFRESH"
#define GETSWITCH       "GETSWITCH"
#define SETSWITCH       "SETSWITCH"
#define TOGGLESWITCH    "TOGGLESWITCH"
#define SETDIMMER       "SETDIMMER"
#define GETDIMMER       "GETDIMMER"
#define SENSOR          "SENSOR"
#define GETHEATER       "GETHEATER"
#define SETHEATER       "SETHEATER"
#define MONITOR         "MONITOR"

/**
 * Main libmicrohttpd answer callback function
 * url format: /PREFIX/COMMAND/DEVICE[/PARAM1[/PARAM2[/1]]]
 * examples:
 * Get indoor temperature on sensor 0 from device DEV1: /PREFIX/SENSOR/DEV1/TEMPINT/0
 * Set switcher 3 to ON on DEV1 : /PREFIX/SETSWITCH/DEV1/3/1
 * Get switcher 2 state on DEV2 : /PREFIX/GETSWITCH/DEV2/2
 * Get forced switcher 2 state on DEV2 : /PREFIX/GETSWITCH/DEV2/2/1
 * etc.
 * This function looks huge but basically it's a finite state machine
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
  
  // archive data
  pthread_t thread_archive;
  int thread_ret_archive = 0, thread_detach_archive = 0;
  struct archive_args config_archive;
  
  snprintf(urlcpy, urllength+1, "%s", url);
  prefix = strtok_r( urlcpy, delim, &saveptr );
  
  /*
   * Initialize POST data shell with empty values
   */
  if (NULL == *con_cls) {
    if (0 == strcmp (method, "POST")) {
      con_info_post = malloc (sizeof (struct connection_info_struct));
      if (NULL == con_info_post) {
        return MHD_NO;
      }
      command = strtok_r( NULL, delim, &saveptr );
      if (0 == strncmp(SETDEVICEDATA, command, strlen(SETDEVICEDATA))) {
        con_info_post->data = malloc(sizeof(struct _device));
        memset(((struct _device *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_DEVICE;
      } else if (0 == strncmp(SETSWITCHDATA, command, strlen(SETSWITCHDATA))) {
        con_info_post->data = malloc(sizeof(struct _switcher));
        memset(((struct _switcher *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SWITCH;
      } else if (0 == strncmp(SETSENSORDATA, command, strlen(SETSENSORDATA))) {
        con_info_post->data = malloc(sizeof(struct _sensor));
        memset(((struct _sensor *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SENSOR;
      } else if (0 == strncmp(SETHEATERDATA, command, strlen(SETHEATERDATA))) {
        con_info_post->data = malloc(sizeof(struct _heater));
        memset(((struct _heater *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_HEATER;
      } else if (0 == strncmp(SETDIMMERDATA, command, strlen(SETDIMMERDATA))) {
        con_info_post->data = malloc(sizeof(struct _dimmer));
        memset(((struct _dimmer *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_DIMMER;
      } else if (0 == strncmp(SETACTION, command, strlen(SETACTION)) || 0 == strncmp(ADDACTION, command, strlen(ADDACTION))) {
        con_info_post->data = malloc(sizeof(struct _action));
        ((struct _action *)con_info_post->data)->id = 0;
        memset(((struct _action *)con_info_post->data)->name, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->device, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->switcher, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->heater, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->dimmer, 0, WORDLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->params, 0, MSGLENGTH*sizeof(char));
        memset(((struct _action *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_ACTION;
      } else if (0 == strncmp(SETSCRIPT, command, strlen(SETSCRIPT)) || 0 == strncmp(ADDSCRIPT, command, strlen(ADDSCRIPT))) {
        con_info_post->data = malloc(sizeof(struct _script));
        ((struct _script *)con_info_post->data)->id = 0;
        strcpy(((struct _script *)con_info_post->data)->name, "");
        strcpy(((struct _script *)con_info_post->data)->device, "");
        ((struct _script *)con_info_post->data)->enabled = 0;
        strcpy(((struct _script *)con_info_post->data)->actions, "");
        memset(((struct _script *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SCRIPT;
      } else if (0 == strncmp(SETSCHEDULE, command, strlen(SETSCHEDULE)) || 0 == strncmp(ADDSCHEDULE, command, strlen(ADDSCHEDULE))) {
        con_info_post->data = malloc(sizeof(struct _schedule));
        ((struct _schedule *)con_info_post->data)->id = 0;
        ((struct _schedule *)con_info_post->data)->next_time = 0;
        ((struct _schedule *)con_info_post->data)->script = 0;
        ((struct _schedule *)con_info_post->data)->repeat_schedule = -1;
        ((struct _schedule *)con_info_post->data)->repeat_schedule_value = 0;
        ((struct _schedule *)con_info_post->data)->remove_after_done = 0;
        strcpy(((struct _schedule *)con_info_post->data)->name, "");
        strcpy(((struct _schedule *)con_info_post->data)->device, "");
        memset(((struct _schedule *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SCHEDULE;
      }
      con_info_post->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post_data, (void *) con_info_post);
      if (NULL == con_info_post->postprocessor) {
        free(con_info_post);
        return MHD_NO;
      }
      con_info_post->connectiontype = HTTPPOST;
      *con_cls = (void *) con_info_post;
      return MHD_YES;
    }
  }
  
  /*
   * Initialize page variable that will feed the http output
   */
  page = malloc((MSGLENGTH*2+1)*sizeof(char));
  
  /*
   * url parsing
   */
  if (prefix == NULL) {
    // wrong url
    tf_len = 2*strlen(url);
    char * sanitize = malloc((tf_len+1)*sizeof(char));
    sanitize_json_string((char *)url, sanitize, WORDLENGTH);
    snprintf(page, MSGLENGTH, json_template_webserver_wrong_url, sanitize, urllength);
    free(sanitize);
  } else if (0 == strcmp(prefix, config->url_prefix)) {
    /*
     * All GET commands are executed here
     */
    if (0 == strcmp(method, "GET")) {
      command = strtok_r( NULL, delim, &saveptr );
      if (command != NULL) {
        if (0 == strncmp(DEVICES, command, strlen(DEVICES))) { // Get all devices
          to_free = get_devices(config->sqlite3_db, config->terminal, config->nb_terminal);
          tf_len = snprintf(NULL, 0, json_template_webserver_devices, to_free);
          free(page);
          page = malloc((tf_len+1)*sizeof(char));
          snprintf(page, tf_len+1, json_template_webserver_devices, to_free);
          free(to_free);
          to_free = NULL;
        } else if ( 0 == strncmp(ACTIONS, command, strlen(ACTIONS))) { // Get actions
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_actions(config->sqlite3_db, device);
          if (to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_actions, to_free);
            free(page);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, tf_len+1, json_template_webserver_actions, to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_actions_error);
          }
        } else if ( 0 == strncmp(SCRIPTS, command, strlen(SCRIPTS))) { // Get scripts
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_scripts(config->sqlite3_db, device);
          if (to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_scripts, to_free);
            free(page);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, tf_len+1, json_template_webserver_scripts, to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_scripts_error);
          }
        } else if ( 0 == strncmp(SCRIPT, command, strlen(SCRIPT))) { // Get one script
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            snprintf(page, MSGLENGTH, json_template_webserver_script_error_id);
          } else {
            to_free = get_script(config->sqlite3_db, script, 1);
            if (to_free != NULL) {
              snprintf(page, MSGLENGTH, json_template_webserver_script, to_free);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_script_error);
            }
            free(to_free);
          }
        } else if ( 0 == strncmp(RUNSCRIPT, command, strlen(RUNSCRIPT))) { // Run a script
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            snprintf(page, MSGLENGTH, json_template_webserver_script_error_id);
          } else {
            if (run_script(config->sqlite3_db, config->terminal, config->nb_terminal, config->script_path, script)) {
              snprintf(page, MSGLENGTH, json_template_webserver_script_run);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_script_run_error);
            }
          }
        } else if ( 0 == strncmp(SCHEDULES, command, strlen(SCHEDULES))) { // get all schedules
          device = strtok_r( NULL, delim, &saveptr );
          to_free = get_schedules(config->sqlite3_db, device);
          if (to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_schedules, to_free);
            free(page);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, tf_len+1, json_template_webserver_schedules, to_free);
            free(to_free);
            to_free = NULL;
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_schedules_error);
          }
        } else if ( 0 == strncmp(ENABLESCHEDULE, command, strlen(ENABLESCHEDULE))) { // Enable or disable a schedule
          schedule = strtok_r( NULL, delim, &saveptr );
          status = strtok_r( NULL, delim, &saveptr );
          if (schedule != NULL && status != NULL && enable_schedule(config->sqlite3_db, schedule, status, buffer)) {
            snprintf(page, MSGLENGTH, json_template_webserver_schedule_enable, buffer);
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_schedule_enable_error);
          }
        } else if ( 0 == strncmp(DELETEACTION, command, strlen(DELETEACTION)) ) { // Delete an action
          action = strtok_r( NULL, delim, &saveptr );
          if (delete_action(config->sqlite3_db, action)) {
            snprintf(page, MSGLENGTH, json_template_webserver_action_delete);
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_action_delete_error);
          }
        } else if ( 0 == strncmp(DELETESCRIPT, command, strlen(DELETESCRIPT)) ) { // Delete a script
          script = strtok_r( NULL, delim, &saveptr );
          if (delete_script(config->sqlite3_db, script)) {
            snprintf(page, MSGLENGTH, json_template_webserver_script_delete);
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_script_delete_error);
          }
        } else if ( 0 == strncmp(DELETESCHEDULE, command, strlen(DELETESCHEDULE)) ) { // Delete a schedule
          schedule = strtok_r( NULL, delim, &saveptr );
          if (delete_schedule(config->sqlite3_db, schedule)) {
            snprintf(page, MSGLENGTH, json_template_webserver_schedule_delete);
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_schedule_delete_error);
          }
        } else if ( 0 == strncmp(ARCHIVE, command, strlen(ARCHIVE)) ) { // Archive data
          epoch_from_str = strtok_r( NULL, delim, &saveptr );
          epoch_from = strtol(epoch_from_str, NULL, 10);
          config_archive.sqlite3_db = config->sqlite3_db;
          strncpy(config_archive.db_archive_path, config->db_archive_path, MSGLENGTH);
          config_archive.epoch_from = epoch_from;
          
          thread_ret_archive = pthread_create(&thread_archive, NULL, thread_archive_run, (void *)&config_archive);
          thread_detach_archive = pthread_detach(thread_archive);
          if (thread_ret_archive || thread_detach_archive) {
            log_message(LOG_WARNING, "Error creating or detaching archive thread, return code: %d, detach code: %d",
                        thread_ret_archive, thread_detach_archive);
            snprintf(page, MSGLENGTH, json_template_webserver_archive, "error", epoch_from, get_last_archive(config->db_archive_path), is_archive_running(config->db_archive_path)?"true":"false");
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_archive, "ok", epoch_from, get_last_archive(config->db_archive_path), is_archive_running(config->db_archive_path)?"true":"false");
          }
        } else if ( 0 == strncmp(LASTARCHIVE, command, strlen(LASTARCHIVE)) ) { // Get Archive informations
          if (strcmp("", config->db_archive_path) != 0) {
            snprintf(page, MSGLENGTH, json_template_webserver_last_archive, get_last_archive(config->db_archive_path), is_archive_running(config->db_archive_path)?"true":"false");
          } else {
            snprintf(page, MSGLENGTH, json_template_webserver_last_archive_error);
          }
        } else {
          // The following GET commands need a DEVICE specified
          device = strtok_r( NULL, delim, &saveptr );
          if (device == NULL) {
            snprintf(page, MSGLENGTH, json_template_webserver_no_device);
          } else {
            cur_terminal = get_device_from_name(device, config->terminal, config->nb_terminal);
            if (cur_terminal == NULL) {
              snprintf(page, MSGLENGTH, json_template_webserver_device_not_found, device);
            } else if ( 0 == strncmp(RESET, command, strlen(RESET)) ) { // send a reset command to reconnect a device
              result = reconnect_device(cur_terminal, config->terminal, config->nb_terminal);
              sanitize_json_string(device, sanitized, WORDLENGTH);
              if (result && init_device_status(config->sqlite3_db, cur_terminal)) {
                log_message(LOG_WARNING, "Device %s initialized", cur_terminal->name);
                snprintf(page, MSGLENGTH, json_template_webserver_reset_device, sanitized, (result!=-1)?"true":"false", "true");
              } else {
                log_message(LOG_WARNING, "Error initializing device %s", cur_terminal->name);
                snprintf(page, MSGLENGTH, json_template_webserver_reset_device, sanitized, (result!=-1)?"true":"false", "false");
              }
            } else if (!cur_terminal->enabled) { // Error, device is disabled
              snprintf(page, MSGLENGTH, json_template_webserver_device_disabled, cur_terminal->name);
            } else if (is_connected(cur_terminal)) {
              if ( 0 == strncmp(HEARTBEAT, command, strlen(HEARTBEAT)) ) { // Send a heartbeat command
                result = send_heartbeat(cur_terminal);
                snprintf(page, MSGLENGTH, json_template_webserver_heartbeat, sanitized, result?"true":"false");
              } else if ( 0 == strncmp(NAME, command, strlen(NAME)) ) { // Get the device name
                get_name(cur_terminal, buffer);
                sanitize_json_string(device, sanitized, WORDLENGTH);
                snprintf(page, MSGLENGTH, json_template_webserver_device_name, sanitized, buffer);
              } else if ( 0 == strncmp(OVERVIEW, command, strlen(OVERVIEW)) ) { // Get overview: all the device elements are listed with their current state
                free(page);
                page = NULL;
                page = get_overview(config->sqlite3_db, cur_terminal);
              } else if ( 0 == strncmp(REFRESH, command, strlen(REFRESH)) ) { // Get refresh: refresh all elements of a device and get the OVERVIEW command result
                free(page);
                page = NULL;
                page = get_refresh(config->sqlite3_db, cur_terminal);
              } else if ( 0 == strncmp(GETSWITCH, command, strlen(GETSWITCH)) ) { // Get a switch state
                switcher = strtok_r( NULL, delim, &saveptr );
                force = strtok_r( NULL, delim, &saveptr );
                iforce=(force != NULL && (0 == strcmp("1", force)))?1:0;
                if (switcher != NULL) {
                  result = get_switch_state(cur_terminal, switcher, iforce);
                  snprintf(page, MSGLENGTH, json_template_webserver_getswitch, device, switcher, result);
                } else {
                  snprintf(page, MSGLENGTH, json_template_webserver_getswitch_error, command);
                }
              } else if ( 0 == strncmp(SETSWITCH, command, strlen(SETSWITCH)) ) { // Set a switch state
                switcher = strtok_r( NULL, delim, &saveptr );
                status = strtok_r( NULL, delim, &saveptr );
                result = set_switch_state(cur_terminal, switcher, (status != NULL && (0 == strcmp("1", status))?1:0));
                snprintf(page, MSGLENGTH, json_template_webserver_setswitch, device, switcher, status, result);
                if (!save_startup_switch_status(config->sqlite3_db, cur_terminal->name, switcher, (status != NULL && (0 == strcmp("1", status))?1:0))) {
                  log_message(LOG_WARNING, "Error saving switcher status in the database");
                }
              } else if ( 0 == strncmp(TOGGLESWITCH, command, strlen(TOGGLESWITCH)) ) { // Toggle a switch state
                switcher = strtok_r( NULL, delim, &saveptr );
                result = toggle_switch_state(cur_terminal, switcher);
                snprintf(page, MSGLENGTH, json_template_webserver_toggleswitch, device, switcher, result);
                if (!save_startup_switch_status(config->sqlite3_db, cur_terminal->name, switcher, result)) {
                  log_message(LOG_WARNING, "Error saving switcher status in the database");
                }
              } else if ( 0 == strncmp(SETDIMMER, command, strlen(SETDIMMER)) ) { // Set a dimmer state
                dimmer_name = strtok_r( NULL, delim, &saveptr );
                dimmer_value = strtok_r( NULL, delim, &saveptr );
                if (dimmer_name != NULL && dimmer_value != NULL) {
                  i_dimmer_value = strtol(dimmer_value, NULL, 10);
                  result = set_dimmer_value(cur_terminal, dimmer_name, i_dimmer_value);
                  snprintf(page, MSGLENGTH, json_template_webserver_setdimmer, cur_terminal->name, dimmer_name, result);
                  if (!save_startup_dimmer_value(config->sqlite3_db, cur_terminal->name, dimmer_name, i_dimmer_value)) {
                    log_message(LOG_WARNING, "Error saving switcher status in the database");
                  }
                } else {
                  snprintf(page, MSGLENGTH, json_template_webserver_setdimmer_error);
                }
              } else if ( 0 == strncmp(GETDIMMER, command, strlen(GETDIMMER)) ) { // Get a dimmer state
                dimmer_name = strtok_r( NULL, delim, &saveptr );
                if (dimmer_name != NULL) {
                  i_dimmer_value = get_dimmer_value(cur_terminal, dimmer_name);
                  snprintf(page, MSGLENGTH, json_template_webserver_getdimmer, cur_terminal->name, dimmer_name, i_dimmer_value);
                } else {
                  snprintf(page, MSGLENGTH, json_template_webserver_getdimmer_error);
                }
              } else if ( 0 == strncmp(SENSOR, command, strlen(SENSOR)) ) { // Get a sensor value
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
                    snprintf(page, MSGLENGTH, json_template_webserver_sensor_error, sanitized);
                  } else {
                    snprintf(page, MSGLENGTH, json_template_webserver_sensor, sanitized, sensor_value);
                  }
                } else {
                  sanitize_json_string(sensor, sanitized, WORDLENGTH);
                  snprintf(page, MSGLENGTH, json_template_webserver_sensor_not_found, sanitized);
                }
              } else if ( 0 == strncmp(GETHEATER, command, strlen(GETHEATER)) ) { // Get the heater command
                heater_name = strtok_r( NULL, delim, &saveptr );
                if (get_heater(cur_terminal, heater_name, buffer)) {
                  if (parse_heater(config->sqlite3_db, cur_terminal->name, heater_name, buffer, &heat_status)) {
                    snprintf(page, MSGLENGTH, json_template_webserver_getheater, 
                              cur_terminal->name, heat_status.name, heat_status.display, heat_status.enabled?"true":"false",
                              heat_status.set?"true":"false", heat_status.heat_max_value, heat_status.unit);
                  } else {
                    snprintf(page, MSGLENGTH, json_template_webserver_getheater_error_parsing);
                  }
                } else {
                  snprintf(page, MSGLENGTH, json_template_webserver_getheater_error);
                }
              } else if ( 0 == strncmp(SETHEATER, command, strlen(SETHEATER)) ) { // Set the heater command
                heater_name = strtok_r( NULL, delim, &saveptr );
                heat_enabled = strtok_r( NULL, delim, &saveptr );
                heat_value = strtok_r( NULL, delim, &saveptr );
                if (heater_name != NULL && heat_enabled != NULL && (heat_value != NULL || 0==strcmp("0", heat_enabled))) {
                  i_heat_enabled = (0==strcmp("1", heat_enabled)?1:0);
                  f_heat_value = strtof(heat_value, NULL);
                  if (set_heater(cur_terminal, heater_name, i_heat_enabled, f_heat_value, buffer)) {
                    if (parse_heater(config->sqlite3_db, cur_terminal->name, heater_name, buffer, &heat_status)) {
                      if (!save_startup_heater_status(config->sqlite3_db, cur_terminal->name, heater_name, i_heat_enabled, f_heat_value)) {
                        log_message(LOG_WARNING, "Error saving heater status in the database");
                      }
                      snprintf(page, MSGLENGTH, json_template_webserver_setheater,
                                cur_terminal->name, heat_status.name, heat_status.display, heat_status.enabled?"true":"false",
                                heat_status.on?"true":"false", heat_status.set?"true":"false", heat_status.heat_max_value, heat_status.unit);
                    } else {
                      snprintf(page, MSGLENGTH, json_template_webserver_setheater_error_parsing);
                    }
                  } else {
                    snprintf(page, MSGLENGTH, json_template_webserver_setheater_error, device, heater_name);
                  }
                } else {
                  snprintf(page, MSGLENGTH, json_template_webserver_unknown_command, command);
                }
              } else if ( 0 == strncmp(MONITOR, command, strlen(MONITOR)) ) { // Get the monitor value of an element since a specified date
                switcher = strtok_r( NULL, delim, &saveptr );
                sensor = strtok_r( NULL, delim, &saveptr );
                start_date = strtok_r( NULL, delim, &saveptr );
                to_free = get_monitor(config->sqlite3_db, device, switcher, sensor, start_date);
                // TODO: Add monitor result shell
                if (to_free != NULL) {
                  tf_len = strlen(to_free);
                  free(page);
                  page = malloc((tf_len + 1) * sizeof(char));
                  strcpy(page, to_free);
                  free(to_free);
                } else {
                  snprintf(page, MSGLENGTH, json_template_webserver_monitor_error);
                }
              } else {
                sanitize_json_string(command, sanitized, WORDLENGTH);
                snprintf(page, MSGLENGTH, json_template_webserver_unknown_command, sanitized);
              }
            } else {
              sanitize_json_string(device, sanitized, WORDLENGTH);
              snprintf(page, MSGLENGTH, json_template_webserver_device_not_connected, sanitized);
            }
          }
        }
      } else {
        snprintf(page, MSGLENGTH, json_template_webserver_command_empty);
      }
    } else if (0 == strcmp(method, "POST")) {
      /*
       * Execute all the POST commands
       * POST commands are only commands to set data values
       * For example change a sensor name, add an action, etc.
       */
      command = strtok_r( NULL, delim, &saveptr );
      if (command != NULL) {
        if (*upload_data_size != 0) {
          MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
          *upload_data_size = 0;
          free(page);
          return MHD_YES;
        } else {
          if (0 == strncmp(SETDEVICEDATA, command, strlen(SETDEVICEDATA))) { // Set device data
            cur_device = (struct _device *)con_info->data;
            cur_terminal = get_device_from_name(cur_device->name, config->terminal, config->nb_terminal);
            to_free = set_device_data(config->sqlite3_db, *cur_device);
            if (to_free != NULL) {
              cur_terminal->enabled = cur_device->enabled;
              free(page);
              tf_len = snprintf(NULL, 0, json_template_webserver_setdevicedata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1), json_template_webserver_setdevicedata, to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setdevicedata_error);
            }
          } else if (0 == strncmp(SETSWITCHDATA, command, strlen(SETSWITCHDATA))) { // Set switcher data
            cur_switch = (struct _switcher *)con_info->data;
            to_free = set_switch_data(config->sqlite3_db, *cur_switch);
            if (to_free != NULL) {
              free(page);
              tf_len = snprintf(NULL, 0, json_template_webserver_setswitchdata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1), json_template_webserver_setswitchdata, to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setswitchdata_error);
            }
          } else if (0 == strncmp(SETSENSORDATA, command, strlen(SETSENSORDATA))) { // Set sensor data
            cur_sensor = (struct _sensor *)con_info->data;
            to_free = set_sensor_data(config->sqlite3_db, *cur_sensor);
            if (to_free != NULL) {
              free(page);
              tf_len = snprintf(NULL, 0, json_template_webserver_setsensordata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1), json_template_webserver_setsensordata, to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setsensordata_error);
            }
          } else if (0 == strncmp(SETHEATERDATA, command, strlen(SETHEATERDATA))) { // Set heater data
            cur_heater = (struct _heater *)con_info->data;
            to_free = set_heater_data(config->sqlite3_db, *cur_heater);
            if (to_free != NULL) {
              free(page);
              tf_len = snprintf(NULL, 0, json_template_webserver_setheaterdata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1), json_template_webserver_setheaterdata, to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setheaterdata_error);
            }
          } else if (0 == strncmp(SETDIMMERDATA, command, strlen(SETDIMMERDATA))) { // Set dimmer data
            cur_dimmer = (struct _dimmer *)con_info->data;
            to_free = set_dimmer_data(config->sqlite3_db, *cur_dimmer);
            if (to_free != NULL) {
              free(page);
              tf_len = snprintf(NULL, 0, json_template_webserver_setdimmerdata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1), json_template_webserver_setdimmerdata, to_free);
              free(to_free);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setdimmerdata_error);
            }
          } else if (0 == strncmp(ADDACTION, command, strlen(ADDACTION))) {
            cur_action = (struct _action *)con_info->data;
            if (add_action(config->sqlite3_db, *cur_action, buffer)) {
              snprintf(page, MSGLENGTH*2, json_template_webserver_addaction, buffer);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_addaction_error);
            }
          } else if (0 == strncmp(SETACTION, command, strlen(SETACTION))) {
            cur_action = (struct _action *)con_info->data;
            if (set_action(config->sqlite3_db, *cur_action, buffer)) {
              snprintf(page, MSGLENGTH, json_template_webserver_setaction, buffer);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setaction_error);
            }
          } else if (0 == strncmp(ADDSCRIPT, command, strlen(ADDSCRIPT))) {
            cur_script = (struct _script *)con_info->data;
            if (add_script(config->sqlite3_db, *cur_script, buffer)) {
              snprintf(page, MSGLENGTH*2, json_template_webserver_addscript, buffer);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_addscript_error);
            }
          } else if (0 == strncmp(SETSCRIPT, command, strlen(SETSCRIPT))) {
            cur_script = (struct _script *)con_info->data;
            if (set_script(config->sqlite3_db, *cur_script, buffer)) {
              snprintf(page, MSGLENGTH*2, json_template_webserver_setscript, buffer);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setscript_error);
            }
          } else if (0 == strncmp(ADDSCHEDULE, command, strlen(ADDSCHEDULE))) {
            cur_schedule = (struct _schedule *)con_info->data;
            if (add_schedule(config->sqlite3_db, *cur_schedule, buffer)) {
              snprintf(page, MSGLENGTH*2, json_template_webserver_addschedule, buffer);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_addschedule_error);
            }
          } else if (0 == strncmp(SETSCHEDULE, command, strlen(SETSCHEDULE))) {
            cur_schedule = (struct _schedule *)con_info->data;
            if (set_schedule(config->sqlite3_db, *cur_schedule, buffer)) {
              snprintf(page, MSGLENGTH*2, json_template_webserver_setschedule, buffer);
            } else {
              snprintf(page, MSGLENGTH, json_template_webserver_setschedule_error);
            }
          } else {
            sanitize_json_string(command, sanitized, WORDLENGTH);
            snprintf(page, MSGLENGTH, json_template_webserver_unknown_command, sanitized);
          }
        }
      } else {
        snprintf(page, MSGLENGTH, json_template_webserver_empty_command);
      }
    } else {
      snprintf(page, MSGLENGTH, json_template_webserver_wrong_http_method);
    }
  } else {
    sanitize_json_string(prefix, sanitized, WORDLENGTH);
    snprintf(page, MSGLENGTH, json_template_webserver_wrong_prefix, sanitized);
  }
  
  journal(config->sqlite3_db, inet_ntoa(((struct sockaddr_in *)so_client)->sin_addr), url, page);
  response = MHD_create_response_from_buffer (strlen (page), (void *) page, MHD_RESPMEM_MUST_FREE );
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  log_message(LOG_INFO, "End execution of angharad_rest_webservice, from %s, url: %s", inet_ntoa(((struct sockaddr_in *)so_client)->sin_addr), url);
  return ret;
}

/**
 * Parse the POST data
 */
int iterate_post_data (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size) {
  struct connection_info_struct *con_info = coninfo_cls;
  device    * cur_device;
  switcher  * cur_switch;
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
      log_message(LOG_WARNING, "Error parsing POST data: unknown data type");
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
