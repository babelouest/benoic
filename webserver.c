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
 * Commands used and JSON templates associated
 */

// multiple get
#define OVERVIEW        "OVERVIEW"
#define REFRESH         "REFRESH"
static const char json_template_webserver_overview_refresh[]            = "{\"command\":\"%s\",\"result\":\"ok\",\"device\":%s}";
static const char json_template_webserver_overview_refresh_error[]      = "{\"command\":\"%s\",\"result\":\"error\",\"message\":\"Error getting all status\"}";
#define ACTIONS         "ACTIONS"
static const char json_template_webserver_actions[]                     = "{\"command\":\"ACTIONS\",\"result\":\"ok\",\"actions\":[%s]}";
static const char json_template_webserver_actions_error[]               = "{\"command\":\"ACTIONS\",\"result\":\"error\",\"message\":\"Error getting actions\"}";
#define DEVICES         "DEVICES"
static const char json_template_webserver_devices[]                     = "{\"command\":\"DEVICES\",\"result\":\"ok\",\"devices\":[%s]}";
#define SCHEDULES       "SCHEDULES"
static const char json_template_webserver_schedules[]                   = "{\"command\":\"SCHEDULES\",\"result\":\"ok\",\"schedules\":[%s]}";
static const char json_template_webserver_schedules_error[]             = "{\"command\":\"SCHEDULES\",\"result\":\"error\",\"message\":\"Error getting schedules\"}";
#define SCRIPTS         "SCRIPTS"
static const char json_template_webserver_scripts[]                     = "{\"command\":\"SCRIPTS\",\"result\":\"ok\",\"scripts\":[%s]}";
static const char json_template_webserver_scripts_error[]               = "{\"command\":\"SCRIPTS\",\"result\":\"error\",\"message\":\"Error getting scripts\"}";

// single get
#define GETDIMMER       "GETDIMMER"
static const char json_template_webserver_getdimmer[]                   = "{\"command\":\"GETDIMMER\",\"result\":\"ok\",\"dimmer\":{\"device\":\"%s\",\"name\":\"%s\",\"value\":%d}}";
static const char json_template_webserver_getdimmer_error[]             = "{\"command\":\"GETDIMMER\",\"result\":\"error\",\"syntax_error\":{\"message\":\"Error getting dimmer status\"}}";
#define GETHEATER       "GETHEATER"
static const char json_template_webserver_getheater[]                   = "{\"command\":\"GETHEATER\",\"result\":\"ok\",\"heater\":{\"device\":\"%s\",\"name\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}}";
static const char json_template_webserver_getheater_error[]             = "{\"command\":\"GETHEATER\",\"result\":\"error\",\"message\":\"error getting heater status\"}";
static const char json_template_webserver_getheater_error_parsing[]     = "{\"command\":\"GETHEATER\",\"result\":\"error\",\"message\":\"error parsing results\"}";
#define GETSWITCH       "GETSWITCH"
static const char json_template_webserver_getswitch[]                   = "{\"command\":\"GETSWITCH\",\"result\":\"ok\",\"switch\":{\"device\":\"%s\",\"switch\":\"%s\",\"status\":%d}}";
static const char json_template_webserver_getswitch_error[]             = "{\"command\":\"GETSWITCH\",\"result\":\"error\",\"message\":\"Error getting switch status\"}";
static const char json_template_webserver_getswitch_error_noswitch[]    = "{\"command\":\"GETSWITCH\",\"result\":\"error\",\"syntax_error\":{\"message\":\"no switch specified\",\"command\":\"%s\"}}";
#define SENSOR          "SENSOR"
static const char json_template_webserver_sensor[]                      = "{\"command\":\"SENSOR\",\"result\":\"ok\",\"sensor\":{\"device\":\"%s\",\"sensor\":\"%s\",\"value\":%.2f}}";
static const char json_template_webserver_sensor_error[]                = "{\"command\":\"SENSOR\",\"result\":\"error\",\"message\":\"Error getting sensor value\"}";
static const char json_template_webserver_sensor_not_found[]            = "{\"command\":\"SENSOR\",\"result\":\"unknown sensor\",\"sensor\":{\"device\":\"%s\",\"sensor\":\"%s\",\"response\":\"error\"}}";
#define SCRIPT          "SCRIPT"
static const char json_template_webserver_script[]                      = "{\"command\":\"SCRIPT\",\"result\":\"ok\",\"script\":%s}";
static const char json_template_webserver_script_error[]                = "{\"command\":\"SCRIPT\",\"result\":\"error\",\"message\":\"Error getting script\"}";
static const char json_template_webserver_script_error_id[]             = "{\"command\":\"SCRIPT\",\"result\":\"ok\",\"syntax_error\":{\"message\":\"no script id specified\"}}";

// set element
#define SETDIMMER       "SETDIMMER"
static const char json_template_webserver_setdimmer[]                   = "{\"command\":\"SETDIMMER\",\"result\":\"ok\",\"dimmer\":{\"device\":\"%s\",\"dimmer\":\"%s\",\"value\":%d}}";
static const char json_template_webserver_setdimmer_error[]             = "{\"command\":\"SETDIMMER\",\"result\":\"error\",\"message\":\"Error setting dimmer\"}";
#define SETHEATER       "SETHEATER"
static const char json_template_webserver_setheater[]                   = "{\"command\":\"SETHEATER\",\"result\":\"ok\",\"heater\":{\"device\":\"%s\",\"name\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}}";
static const char json_template_webserver_setheater_error[]             = "{\"command\":\"SETHEATER\",\"result\":\"error\",\"message\":\"Error setting heater\"}";
static const char json_template_webserver_setheater_error_parsing[]     = "{\"command\":\"SETHEATER\",\"result\":\"error\",\"message\":\"Error parsing heater results\"}";
#define SETSWITCH       "SETSWITCH"
static const char json_template_webserver_setswitch[]                   = "{\"command\":\"SETSWITCH\",\"result\":\"ok\",\"switch\":{\"device\":\"%s\",\"switch\":\"%s\",\"status\":%d}}";
static const char json_template_webserver_setswitch_error[]             = "{\"command\":\"SETSWITCH\",\"result\":\"error\",\"message\":\"Error setting switch\"}";
#define TOGGLESWITCH    "TOGGLESWITCH"
static const char json_template_webserver_toggleswitch[]                = "{\"command\":\"TOGGLESWITCH\",\"result\":\"ok\",\"switch\":{\"name\":\"%s\",\"response\":%d}}";
static const char json_template_webserver_toggleswitch_error[]          = "{\"command\":\"TOGGLESWITCH\",\"result\":\"error\",\"message\":\"Error toggling weitch\"}";

// add element data
#define ADDACTION       "ADDACTION"
static const char json_template_webserver_addaction[]                   = "{\"command\":\"ADDACTION\",\"result\":\"ok\",\"action\":%s}";
static const char json_template_webserver_addaction_error[]             = "{\"command\":\"ADDACTION\",\"result\":\"error\",\"message\":\"Error adding action\"}";
#define ADDSCHEDULE     "ADDSCHEDULE"
static const char json_template_webserver_addschedule[]                 = "{\"command\":\"ADDSCHEDULE\",\"result\":\"ok\",\"schedule\":%s}";
static const char json_template_webserver_addschedule_error[]           = "{\"command\":\"ADDSCHEDULE\",\"result\":\"error\",\"message\":\"Error adding schedule\"}";
#define ADDSCRIPT       "ADDSCRIPT"
static const char json_template_webserver_addscript[]                   = "{\"command\":\"ADDSCRIPT\",\"result\":\"ok\",\"script\":%s}";
static const char json_template_webserver_addscript_error[]             = "{\"command\":\"ADDSCRIPT\",\"result\":\"error\",\"message\":\"Error adding script\"}";

// modify element data
#define ENABLESCHEDULE  "ENABLESCHEDULE"
static const char json_template_webserver_schedule_enable[]             = "{\"command\":\"ENABLESCHEDULE\",\"result\":\"ok\",\"schedule\":%s}";
static const char json_template_webserver_schedule_enable_error[]       = "{\"command\":\"ENABLESCHEDULE\",\"result\":\"error\",\"message\":\"Error setting schedule\"}";
#define SETACTION       "SETACTION"
static const char json_template_webserver_setaction[]                   = "{\"command\":\"SETACTION\",\"result\":\"ok\",\"action\":%s}";
static const char json_template_webserver_setaction_error[]             = "{\"command\":\"SETACTION\",\"result\":\"error\",\"message\":\"Error setting action\"}";
#define SETDEVICEDATA   "SETDEVICEDATA"
static const char json_template_webserver_setdevicedata[]               = "{\"command\":\"SETDEVICEDATA\",\"result\":\"ok\",\"device\":%s}";
static const char json_template_webserver_setdevicedata_error[]         = "{\"command\":\"SETDEVICEDATA\",\"result\":\"error\",\"message\":\"Error setting device\"}";
#define SETDIMMERDATA   "SETDIMMERDATA"
static const char json_template_webserver_setdimmerdata[]               = "{\"command\":\"SETDIMMERDATA\",\"result\":\"ok\",\"dimmer\":%s}";
static const char json_template_webserver_setdimmerdata_error[]         = "{\"command\":\"SETDIMMERDATA\",\"result\":\"error\",\"message\":\"Error setting dimmer\"}";
#define SETHEATERDATA   "SETHEATERDATA"
static const char json_template_webserver_setheaterdata[]               = "{\"command\":\"SETHEATERDATA\",\"result\":\"ok\",\"heater\":%s}";
static const char json_template_webserver_setheaterdata_error[]         = "{\"command\":\"SETHEATERDATA\",\"result\":\"error\",\"message\":\"Error setting heater\"}";
#define SETSCHEDULE     "SETSCHEDULE"
static const char json_template_webserver_setschedule[]                 = "{\"command\":\"SETSCHEDULE\",\"result\":\"ok\",\"schedule\":%s}";
static const char json_template_webserver_setschedule_error[]           = "{\"command\":\"SETSCHEDULE\",\"result\":\"error\",\"message\":\"Error setting schedule\"}";
#define SETSCRIPT       "SETSCRIPT"
static const char json_template_webserver_setscript[]                   = "{\"command\":\"SETSCRIPT\",\"result\":\"ok\",\"script\":%s}";
static const char json_template_webserver_setscript_error[]             = "{\"command\":\"SETSCRIPT\",\"result\":\"error\",\"message\":\"Error setting script\"}";
#define SETSENSORDATA   "SETSENSORDATA"
static const char json_template_webserver_setsensordata[]               = "{\"command\":\"SETSENSORDATA\",\"result\":\"ok\",\"sensor\":%s}";
static const char json_template_webserver_setsensordata_error[]         = "{\"command\":\"SETSENSORDATA\",\"result\":\"error\",\"message\":\"Error setting sensor\"}";
#define SETSWITCHDATA   "SETSWITCHDATA"
static const char json_template_webserver_setswitchdata[]               = "{\"command\":\"SETSWITCHDATA\",\"result\":\"ok\",\"switch\":%s}";
static const char json_template_webserver_setswitchdata_error[]         = "{\"command\":\"SETSWITCHDATA\",\"result\":\"error\",\"message\":\"Error setting switcher\"}";

// remove element data
#define DELETEACTION    "DELETEACTION"
static const char json_template_webserver_action_delete[]               = "{\"command\":\"DELETEACTION\",\"result\":\"ok\"}";
static const char json_template_webserver_action_delete_error[]         = "{\"command\":\"DELETEACTION\",\"result\":\"error\",\"message\":\"Error deleting action\"}";
#define DELETESCHEDULE  "DELETESCHEDULE"
static const char json_template_webserver_schedule_delete[]             = "{\"command\":\"DELETESCHEDULE\",\"result\":\"ok\"}";
static const char json_template_webserver_schedule_delete_error[]       = "{\"command\":\"DELETESCHEDULE\",\"result\":\"error\",\"message\":\"Error deleting schedule\"}";
#define DELETESCRIPT    "DELETESCRIPT"
static const char json_template_webserver_script_delete[]               = "{\"command\":\"DELETESCRIPT\",\"result\":\"ok\"}";
static const char json_template_webserver_script_delete_error[]         = "{\"command\":\"DELETESCRIPT\",\"result\":\"error\",\"message\":\"Error deleting script\"}";

// misc
#define ARCHIVE         "ARCHIVE"
static const char json_template_webserver_archive[]                     = "{\"command\":\"ARCHIVE\",\"result\":\"%s\",\"archive_from\":%d,\"last_archive\":%d,\"archive_running\":%s}";
#define HEARTBEAT       "HEARTBEAT"
static const char json_template_webserver_heartbeat[]                   = "{\"command\":\"HEARTBEAT\",\"result\":\"ok\",\"response\":\"%s\"}";
#define LASTARCHIVE     "LASTARCHIVE"
static const char json_template_webserver_last_archive[]                = "{\"command\":\"LASTARCHIVE\",\"result\":\"ok\",\"last_archive\":%d,\"archive_running\":%s}";
static const char json_template_webserver_last_archive_error[]          = "{\"command\":\"LASTARCHIVE\",\"result\":\"error\",\"message\":\"No archive database\"}";
#define MONITOR         "MONITOR"
static const char json_template_webserver_monitor[]                     = "{\"command\":\"MONITOR\",\"result\":\"ok\",\"monitor\":%s}";
static const char json_template_webserver_monitor_error[]               = "{\"command\":\"MONITOR\",\"result\":\"error\",\"message\":\"Error getting monitor values\"}";
#define RESET           "RESET"
static const char json_template_webserver_reset_device[]                = "{\"command\":\"RESET\",\"result\":\"ok\",\"device\":\"%s\",\"response\":%s,\"initialization\":%s}";
#define RUNSCRIPT       "RUNSCRIPT"
static const char json_template_webserver_script_run[]                  = "{\"command\":\"RUNSCRIPT\",\"result\":\"ok\"}";
static const char json_template_webserver_script_run_error[]            = "{\"command\":\"RUNSCRIPT\",\"result\":\"error\",\"message\":\"Error running script\"}";
#define LISTCOMMANDS    "LISTCOMMANDS"
static const char json_template_webserver_list_commands[]               = "{\"command\":\"LISTCOMMANDS\",\"description\":\"List of commands available\",\"commands_available\":%s}";
#define RESTARTSERVER	  "RESTARTSERVER"
static const char json_template_webserver_restart_server_ok[]           = "{\"command\":\"RESTARTSERVER\",\"result\":\"Server restarting\"}";
static const char json_template_webserver_restart_server_not_allowed[]  = "{\"command\":\"RESTARTSERVER\",\"result\":\"error\",\"message\":\"Restart is not allowed for the server\"}";

// general errors
static const char json_template_webserver_no_device[]                   = "{\"command\":\"%s\",\"result\":\"error\",\"syntax_error\":{\"message\":\"No device\"}}";
static const char json_template_webserver_command_empty[]               = "{\"command\":\"\",\"result\":\"error\",\"message\":\"Empty command\",\"commands_available\":%s}";
static const char json_template_webserver_device_disabled[]             = "{\"command\":\"%s\",\"result\":\"error\",\"syntax_error\":{\"message\":\"Device disabled\",\"device\":\"%s\"}}";
static const char json_template_webserver_device_not_connected[]        = "{\"command\":\"%s\",\"result\":\"error\",\"syntax_error\":{\"message\":\"Device not connected\",\"device\":\"%s\"}}";
static const char json_template_webserver_device_not_found[]            = "{\"command\":\"%s\",\"result\":\"error\",\"syntax_error\":{\"message\":\"Device not found\",\"device\":\"%s\"}}";
static const char json_template_webserver_unknown_command[]             = "{\"command\":\"%s\",\"result\":\"error\",\"message\":\"Unknown command\",\"command\":\"%s\",\"commands_available\":%s}";
static const char json_template_webserver_wrong_http_method[]           = "{\"result\":\"ok\"\"syntax_error\":{\"message\":\"Wrong http method\"}}";
static const char json_template_webserver_wrong_prefix[]                = "{\"result\":\"syntax_error\":{\"message\":\"Unknown prefix\",\"prefix\":\"%s\",\"available_prefix\":\"%s\"}}";
static const char json_template_webserver_wrong_url[]                   = "{\"result\":\"ok\"\"syntax_error\":{\"message\":\"Can not parse url\",\"url\":\"%s\",\"size\":%zu}}";

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
  char * command = NULL, * device_name = NULL, * sensor_name = NULL, * switcher_name = NULL,
    * status = NULL, * force = NULL, * action = NULL, * script = NULL, *schedule = NULL, 
    * heater_name = NULL, * heat_enabled = NULL, * heat_value = NULL, 
    * dimmer_name = NULL, * dimmer_value = NULL, * to_free = NULL, 
    * start_date = NULL;
  char * page = NULL;
  char * saveptr;
  heater * heat_status;
  struct MHD_Response *response;
  int ret;
  size_t page_len, urllength = strlen(url);
  int result;
  float sensor_value;
  int iforce;
  int i_heat_enabled;
  float f_heat_value;
  int i_dimmer_value;
  struct _device * cur_terminal = NULL;
  struct connection_info_struct *con_info = *con_cls;
  struct sockaddr *so_client = MHD_get_connection_info (connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
  int tf_len;
  struct config_elements * config = (struct config_elements *) cls;
  struct connection_info_struct * con_info_post = NULL;
  char urlcpy[(urllength*2)+1];
  
  // Post data structs
  struct _device * cur_device;
  struct _switcher * cur_switch;
  struct _sensor * cur_sensor;
  struct _heater * cur_heater;
  struct _dimmer * cur_dimmer;
  struct _action * cur_action;
  struct _script * cur_script;
  struct _schedule * cur_schedule;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sanitize_json_string_url(url, urlcpy, ((urllength*2)+1));
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
        memset(((struct _device *)con_info_post->data)->display, 0, WORDLENGTH*sizeof(char));
        memset(((struct _device *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_DEVICE;
      } else if (0 == strncmp(SETSWITCHDATA, command, strlen(SETSWITCHDATA))) {
        con_info_post->data = malloc(sizeof(struct _switcher));
        memset(((struct _switcher *)con_info_post->data)->display, 0, WORDLENGTH*sizeof(char));
        memset(((struct _switcher *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SWITCH;
      } else if (0 == strncmp(SETSENSORDATA, command, strlen(SETSENSORDATA))) {
        con_info_post->data = malloc(sizeof(struct _sensor));
        memset(((struct _sensor *)con_info_post->data)->display, 0, WORDLENGTH*sizeof(char));
        memset(((struct _sensor *)con_info_post->data)->unit, 0, WORDLENGTH*sizeof(char));
        ((struct _sensor *)con_info_post->data)->value_type = VALUE_TYPE_NONE;
        memset(((struct _sensor *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_SENSOR;
      } else if (0 == strncmp(SETHEATERDATA, command, strlen(SETHEATERDATA))) {
        con_info_post->data = malloc(sizeof(struct _heater));
        memset(((struct _heater *)con_info_post->data)->display, 0, WORDLENGTH*sizeof(char));
        memset(((struct _heater *)con_info_post->data)->unit, 0, WORDLENGTH*sizeof(char));
        ((struct _heater *)con_info_post->data)->value_type = VALUE_TYPE_NONE;
        memset(((struct _heater *)con_info_post->data)->tags, 0, MSGLENGTH*sizeof(char));
        con_info_post->data_type = DATA_HEATER;
      } else if (0 == strncmp(SETDIMMERDATA, command, strlen(SETDIMMERDATA))) {
        con_info_post->data = malloc(sizeof(struct _dimmer));
        memset(((struct _dimmer *)con_info_post->data)->display, 0, WORDLENGTH*sizeof(char));
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
        memset(((struct _action *)con_info_post->data)->params, 0, WORDLENGTH*sizeof(char));
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
   * url parsing
   */
  if (prefix == NULL) {
    // wrong url
    tf_len = 2*strlen(url);
    page_len = snprintf(NULL, 0, json_template_webserver_wrong_url, url, urllength);
    page = malloc((page_len+1)*sizeof(char));
    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_wrong_url, url, urllength);
  } else if (0 == strcmp(prefix, config->url_prefix)) {
    /*
     * All GET commands are executed here
     */
    if (0 == strcmp(method, "GET")) {
      command = strtok_r( NULL, delim, &saveptr );
      if (command != NULL) {
        if (0 == strncmp(LISTCOMMANDS, command, strlen(LISTCOMMANDS))) { // List all commands
          to_free = get_json_list_commands();
          tf_len = snprintf(NULL, 0, json_template_webserver_list_commands, to_free);
          page = malloc((tf_len+1)*sizeof(char));
          snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_list_commands, to_free);
          free(to_free);
          to_free = NULL;
        } else if (0 == strncmp(DEVICES, command, strlen(DEVICES))) { // Get all devices
          to_free = get_devices( config->master_db, config->terminal, config->nb_terminal);
          tf_len = snprintf(NULL, 0, json_template_webserver_devices, to_free);
          page = malloc((tf_len+1)*sizeof(char));
          snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_devices, to_free);
          free(to_free);
          to_free = NULL;
        } else if ( 0 == strncmp(ACTIONS, command, strlen(ACTIONS))) { // Get actions
          device_name = strtok_r( NULL, delim, &saveptr );
          to_free = get_actions( config->master_db, device_name);
          if (to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_actions, to_free);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_actions, to_free);
            free(to_free);
            to_free = NULL;
          } else {
            page_len = strlen(json_template_webserver_actions_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_actions_error);
          }
        } else if ( 0 == strncmp(SCRIPTS, command, strlen(SCRIPTS))) { // Get scripts
          device_name = strtok_r( NULL, delim, &saveptr );
          to_free = get_scripts( config->master_db, device_name);
          if (to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_scripts, to_free);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_scripts, to_free);
            free(to_free);
            to_free = NULL;
          } else {
            page_len = strlen(json_template_webserver_scripts_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_scripts_error);
          }
        } else if ( 0 == strncmp(SCRIPT, command, strlen(SCRIPT))) { // Get one script
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            page_len = strlen(json_template_webserver_script_error_id);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_error_id);
          } else {
            to_free = get_script( config->master_db, script, 1);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_script, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_script, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_script_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_error);
            }
          }
        } else if ( 0 == strncmp(RUNSCRIPT, command, strlen(RUNSCRIPT))) { // Run a script
          script = strtok_r( NULL, delim, &saveptr );
          if (script == NULL) {
            page_len = strlen(json_template_webserver_script_error_id);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_error_id);
          } else {
            if (run_script( config->master_db, config->terminal, config->nb_terminal, config->script_path, script)) {
              page_len = strlen(json_template_webserver_script_run);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_run);
            } else {
              page_len = strlen(json_template_webserver_script_run_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_run_error);
            }
          }
        } else if ( 0 == strncmp(SCHEDULES, command, strlen(SCHEDULES))) { // get all schedules
          device_name = strtok_r( NULL, delim, &saveptr );
          to_free = get_schedules( config->master_db, device_name);
          if (to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_schedules, to_free);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_schedules, to_free);
            free(to_free);
            to_free = NULL;
          } else {
            page_len = strlen(json_template_webserver_schedules_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_schedules_error);
          }
        } else if ( 0 == strncmp(ENABLESCHEDULE, command, strlen(ENABLESCHEDULE))) { // Enable or disable a schedule
          schedule = strtok_r( NULL, delim, &saveptr );
          status = strtok_r( NULL, delim, &saveptr );
          to_free = enable_schedule( config->master_db, schedule, status);
          if (schedule != NULL && status != NULL && to_free != NULL) {
            tf_len = snprintf(NULL, 0, json_template_webserver_schedule_enable, to_free);
            page = malloc((tf_len+1)*sizeof(char));
            snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_schedule_enable, to_free);
            free(to_free);
          } else {
            page_len = strlen(json_template_webserver_schedule_enable_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_schedule_enable_error);
          }
        } else if ( 0 == strncmp(DELETEACTION, command, strlen(DELETEACTION)) ) { // Delete an action
          action = strtok_r( NULL, delim, &saveptr );
          if (delete_action( config->master_db, action)) {
            page_len = strlen(json_template_webserver_action_delete);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_action_delete);
          } else {
            page_len = strlen(json_template_webserver_action_delete_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_action_delete_error);
          }
        } else if ( 0 == strncmp(DELETESCRIPT, command, strlen(DELETESCRIPT)) ) { // Delete a script
          script = strtok_r( NULL, delim, &saveptr );
          if (delete_script( config->master_db, script)) {
            page_len = strlen(json_template_webserver_script_delete);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_delete);
          } else {
            page_len = strlen(json_template_webserver_script_delete_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_script_delete_error);
          }
        } else if ( 0 == strncmp(DELETESCHEDULE, command, strlen(DELETESCHEDULE)) ) { // Delete a schedule
          schedule = strtok_r( NULL, delim, &saveptr );
          if (delete_schedule( config->master_db, schedule)) {
            page_len = strlen(json_template_webserver_schedule_delete);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_schedule_delete);
          } else {
            page_len = strlen(json_template_webserver_schedule_delete_error);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_schedule_delete_error);
          }
        } else if ( 0 == strncmp(RESTARTSERVER, command, strlen(RESTARTSERVER)) ) { // Restart the server
          if (config->auto_restart) {
            global_handler_variable = ANGHARAD_RESTART;
            page_len = strlen(json_template_webserver_restart_server_ok);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_restart_server_ok);
          } else {
            // Not allowed to restart server
            page_len = strlen(json_template_webserver_restart_server_not_allowed);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_restart_server_not_allowed);
          }
        } else {
          // The following GET commands need a DEVICE specified
          device_name = strtok_r( NULL, delim, &saveptr );
          if (device_name == NULL) {
            page_len = snprintf(NULL, 0, json_template_webserver_no_device, command);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_no_device, command);
          } else {
            cur_terminal = get_device_from_name(device_name, config->terminal, config->nb_terminal);
            if (cur_terminal == NULL) {
              page_len = snprintf(NULL, 0, json_template_webserver_device_not_found, command, device_name);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_device_not_found, command, device_name);
            } else if ( 0 == strncmp(RESET, command, strlen(RESET)) ) { // send a reset command to reconnect a device
              result = reconnect_device(cur_terminal, config->terminal, config->nb_terminal);
              if (result && init_device_status( config->master_db, cur_terminal)) {
                log_message(LOG_LEVEL_WARNING, "Device %s initialized", cur_terminal->name);
                page_len = snprintf(NULL, 0, json_template_webserver_reset_device, device_name, (result!=-1)?"true":"false", "true");
                page = malloc((page_len+1)*sizeof(char));
                snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_reset_device, device_name, (result!=-1)?"true":"false", "true");
              } else {
                log_message(LOG_LEVEL_WARNING, "Error initializing device %s", cur_terminal->name);
                page_len = snprintf(NULL, 0, json_template_webserver_reset_device, device_name, (result!=-1)?"true":"false", "false");
                page = malloc((page_len+1)*sizeof(char));
                snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_reset_device, device_name, (result!=-1)?"true":"false", "false");
              }
            } else if (!cur_terminal->enabled) { // Error, device is disabled
              page_len = snprintf(NULL, 0, json_template_webserver_device_disabled, command, cur_terminal->name);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_device_disabled, command, cur_terminal->name);
            } else if (is_connected(cur_terminal)) {
              if ( 0 == strncmp(HEARTBEAT, command, strlen(HEARTBEAT)) ) { // Send a heartbeat command
                result = send_heartbeat(cur_terminal);
                page_len = snprintf(NULL, 0, json_template_webserver_heartbeat, result?"true":"false");
                page = malloc((page_len+1)*sizeof(char));
                snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_heartbeat, result?"true":"false");
              } else if ( 0 == strncmp(OVERVIEW, command, strlen(OVERVIEW)) ) { // Get overview: all the device elements are listed with their current state
                page = NULL;
                to_free = get_overview( config->master_db, cur_terminal);
                if (to_free != NULL) {
                  tf_len = snprintf(NULL, 0, json_template_webserver_overview_refresh, command, to_free);
                  page = malloc((tf_len+1)*sizeof(char));
                  snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_overview_refresh, command, to_free);
                  free(to_free);
                } else {
                  page_len = snprintf(NULL, 0, json_template_webserver_overview_refresh_error, command);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_overview_refresh_error, command);
                }
              } else if ( 0 == strncmp(REFRESH, command, strlen(REFRESH)) ) { // Get refresh: refresh all elements of a device and get the OVERVIEW command result
                page = NULL;
                to_free = get_refresh( config->master_db, cur_terminal);
                if (to_free != NULL) {
                  tf_len = snprintf(NULL, 0, json_template_webserver_overview_refresh, command, to_free);
                  page = malloc((tf_len+1)*sizeof(char));
                  snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_overview_refresh, command, to_free);
                  free(to_free);
                } else {
                  page_len = snprintf(NULL, 0, json_template_webserver_overview_refresh_error, command);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_overview_refresh_error, command);
                }
              } else if ( 0 == strncmp(GETSWITCH, command, strlen(GETSWITCH)) ) { // Get a switch state
                switcher_name = strtok_r( NULL, delim, &saveptr );
                force = strtok_r( NULL, delim, &saveptr );
                iforce = (force != NULL && (0 == strcmp("1", force)))?1:0;
                if (switcher_name != NULL) {
                  result = get_switch_state(cur_terminal, switcher_name, iforce);
                  if (result != ERROR_SWITCH) {
                    page_len = snprintf(NULL, 0, json_template_webserver_getswitch, device_name, switcher_name, result);
                    page = malloc((page_len+1)*sizeof(char));
                    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getswitch, device_name, switcher_name, result);
                  } else {
                    page_len = strlen(json_template_webserver_getswitch_error);
                    page = malloc((page_len+1)*sizeof(char));
                    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getswitch_error);
                  }
                } else {
                  page_len = snprintf(NULL, 0, json_template_webserver_getswitch_error_noswitch, command);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getswitch_error_noswitch, command);
                }
              } else if ( 0 == strncmp(SETSWITCH, command, strlen(SETSWITCH)) ) { // Set a switch state
                switcher_name = strtok_r( NULL, delim, &saveptr );
                status = strtok_r( NULL, delim, &saveptr );
                result = set_switch_state(cur_terminal, switcher_name, (status != NULL && (0 == strcmp("1", status))?1:0));
                if (result != ERROR_SWITCH) {
                  page_len = snprintf(NULL, 0, json_template_webserver_setswitch, switcher_name, status, result);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setswitch, switcher_name, status, result);
                  if (!save_startup_switch_status( config->master_db, cur_terminal->name, switcher_name, (status != NULL && (0 == strcmp("1", status))?1:0))) {
                    log_message(LOG_LEVEL_WARNING, "Error saving switcher status in the database");
                  }
                  if (!monitor_store( config->master_db, cur_terminal->name, switcher_name, "", "", "", status)) {
                    log_message(LOG_LEVEL_WARNING, "Error monitoring switcher");
                  }
                } else {
                  page_len = snprintf(NULL, 0, json_template_webserver_setswitch_error);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setswitch_error);
                }
              } else if ( 0 == strncmp(TOGGLESWITCH, command, strlen(TOGGLESWITCH)) ) { // Toggle a switch state
                switcher_name = strtok_r( NULL, delim, &saveptr );
                result = toggle_switch_state(cur_terminal, switcher_name);
                if (result != ERROR_SWITCH) {
                  page_len = snprintf(NULL, 0, json_template_webserver_toggleswitch, switcher_name, result);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_toggleswitch, switcher_name, result);
                  if (!save_startup_switch_status( config->master_db, cur_terminal->name, switcher_name, result)) {
                    log_message(LOG_LEVEL_WARNING, "Error saving switcher status in the database");
                  }
                  snprintf(status, WORDLENGTH, "%d", result);
                  if (!monitor_store( config->master_db, cur_terminal->name, switcher_name, "", "", "", status)) {
                    log_message(LOG_LEVEL_WARNING, "Error monitoring switcher");
                  }
                } else {
                  page_len = strlen(json_template_webserver_toggleswitch_error);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_toggleswitch_error);
                }
              } else if ( 0 == strncmp(SETDIMMER, command, strlen(SETDIMMER)) ) { // Set a dimmer state
                dimmer_name = strtok_r( NULL, delim, &saveptr );
                dimmer_value = strtok_r( NULL, delim, &saveptr );
                if (dimmer_name != NULL && dimmer_value != NULL) {
                  i_dimmer_value = strtol(dimmer_value, NULL, 10);
                  result = set_dimmer_value(cur_terminal, dimmer_name, i_dimmer_value);
                  page_len = snprintf(NULL, 0, json_template_webserver_setdimmer, cur_terminal->name, dimmer_name, result);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setdimmer, cur_terminal->name, dimmer_name, result);
                  if (!save_startup_dimmer_value( config->master_db, cur_terminal->name, dimmer_name, i_dimmer_value)) {
                    log_message(LOG_LEVEL_WARNING, "Error saving switcher status in the database");
                  }
                  if (!monitor_store( config->master_db, cur_terminal->name, "", "", dimmer_name, "", dimmer_value)) {
                    log_message(LOG_LEVEL_WARNING, "Error monitoring dimmer");
                  }
                } else {
                  page_len = strlen(json_template_webserver_setdimmer_error);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setdimmer_error);
                }
              } else if ( 0 == strncmp(GETDIMMER, command, strlen(GETDIMMER)) ) { // Get a dimmer state
                dimmer_name = strtok_r( NULL, delim, &saveptr );
                if (dimmer_name != NULL) {
                  i_dimmer_value = get_dimmer_value(cur_terminal, dimmer_name);
                  page_len = snprintf(NULL, 0, json_template_webserver_getdimmer, cur_terminal->name, dimmer_name, i_dimmer_value);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getdimmer, cur_terminal->name, dimmer_name, i_dimmer_value);
                } else {
                  page_len = strlen(json_template_webserver_getdimmer_error);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getdimmer_error);
                }
              } else if ( 0 == strncmp(SENSOR, command, strlen(SENSOR)) ) { // Get a sensor value
                sensor_name = strtok_r( NULL, delim, &saveptr );
                if (sensor_name != NULL) {
                  force = strtok_r( NULL, delim, &saveptr );
                  iforce=(force != NULL && (0 == strcmp("1", force)))?1:0;
                  sensor_value = get_sensor_value( config->master_db, cur_terminal, sensor_name, iforce);
                  if (sensor_value == ERROR_SENSOR) {
                    page_len = snprintf(NULL, 0, json_template_webserver_sensor_error);
                    page = malloc((page_len+1)*sizeof(char));
                    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_sensor_error);
                  } else {
                    page_len = snprintf(NULL, 0, json_template_webserver_sensor, device_name, sensor_name, sensor_value);
                    page = malloc((page_len+1)*sizeof(char));
                    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_sensor, device_name, sensor_name, sensor_value);
                  }
                } else {
                  page_len = snprintf(NULL, 0, json_template_webserver_sensor_not_found, device_name, sensor_name);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_sensor_not_found, device_name, sensor_name);
                }
              } else if ( 0 == strncmp(GETHEATER, command, strlen(GETHEATER)) ) { // Get the heater value
                heater_name = strtok_r( NULL, delim, &saveptr );
                heat_status = get_heater( config->master_db, cur_terminal, heater_name);
                if (heat_status != NULL) {
                  page_len = snprintf(NULL, 0, json_template_webserver_getheater, 
                            cur_terminal->name, heat_status->name, heat_status->enabled?"true":"false",
                            heat_status->set?"true":"false", heat_status->on?"true":"false", heat_status->heat_max_value, heat_status->unit);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getheater, 
                            cur_terminal->name, heat_status->name, heat_status->enabled?"true":"false",
                            heat_status->set?"true":"false", heat_status->on?"true":"false", heat_status->heat_max_value, heat_status->unit);
                  free(heat_status);
                } else {
                  page_len = strlen(json_template_webserver_getheater_error);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_getheater_error);
                }
              } else if ( 0 == strncmp(SETHEATER, command, strlen(SETHEATER)) ) { // Set the heater value
                heater_name = strtok_r( NULL, delim, &saveptr );
                heat_enabled = strtok_r( NULL, delim, &saveptr );
                heat_value = strtok_r( NULL, delim, &saveptr );
                if (heater_name != NULL && heat_enabled != NULL && (heat_value != NULL || 0==strcmp("0", heat_enabled))) {
                  i_heat_enabled = (0==strcmp("1", heat_enabled)?1:0);
                  f_heat_value = strtof(heat_value, NULL);
                  heat_status = set_heater( config->master_db, cur_terminal, heater_name, i_heat_enabled, f_heat_value);
                  if (heat_status != NULL) {
                    if (!save_startup_heater_status( config->master_db, cur_terminal->name, heater_name, i_heat_enabled, f_heat_value)) {
                      log_message(LOG_LEVEL_WARNING, "Error saving heater status in the database");
                    }
                    if (!monitor_store( config->master_db, cur_terminal->name, "", heater_name, "", "", (i_heat_enabled?"0.0":heat_value))) {
                      log_message(LOG_LEVEL_WARNING, "Error monitoring heater");
                    }
                    page_len = snprintf(NULL, 0, json_template_webserver_setheater,
                              cur_terminal->name, heat_status->name, heat_status->enabled?"true":"false",
                              heat_status->on?"true":"false", heat_status->set?"true":"false", heat_status->heat_max_value, heat_status->unit);
                    page = malloc((page_len+1)*sizeof(char));
                    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setheater,
                              cur_terminal->name, heat_status->name, heat_status->enabled?"true":"false",
                              heat_status->on?"true":"false", heat_status->set?"true":"false", heat_status->heat_max_value, heat_status->unit);
                    free(heat_status);
                  } else {
                    page_len = strlen(json_template_webserver_setheater_error);
                    page = malloc((page_len+1)*sizeof(char));
                    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setheater_error);
                  }
                } else {
                  to_free = get_json_list_commands();
                  page_len = snprintf(NULL, 0, json_template_webserver_unknown_command, command, command, to_free);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_unknown_command, command, command, to_free);
                  free(to_free);
                }
              } else if ( 0 == strncmp(MONITOR, command, strlen(MONITOR)) ) { // Get the monitor value of an element since a specified date
                switcher_name = strsep(&saveptr, delim);
                sensor_name = strsep(&saveptr, delim);
                dimmer_name = strsep(&saveptr, delim);
                heater_name = strsep(&saveptr, delim);
                start_date = strsep(&saveptr, delim);
                to_free = get_monitor( config->master_db, device_name, switcher_name, sensor_name, dimmer_name, heater_name, start_date);
                if (to_free != NULL) {
                  tf_len = snprintf(NULL, 0, json_template_webserver_monitor, to_free);
                  page = malloc((tf_len + 1)*sizeof(char));
                  snprintf(page, (tf_len + 1)*sizeof(char), json_template_webserver_monitor, to_free);
                  free(to_free);
                } else {
                  page_len = strlen(json_template_webserver_monitor_error);
                  page = malloc((page_len+1)*sizeof(char));
                  snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_monitor_error);
                }
              } else {
                to_free = get_json_list_commands();
                page_len = snprintf(NULL, 0, json_template_webserver_unknown_command, device_name, device_name, to_free);
                page = malloc((page_len+1)*sizeof(char));
                snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_unknown_command, device_name, device_name, to_free);
                free(to_free);
              }
            } else {
              page_len = snprintf(NULL, 0, json_template_webserver_device_not_connected, device_name, device_name);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_device_not_connected, device_name, device_name);
            }
          }
        }
      } else {
        to_free = get_json_list_commands();
        page_len = snprintf(NULL, 0, json_template_webserver_command_empty, to_free);
        page = malloc((page_len+1)*sizeof(char));
        snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_command_empty, to_free);
        free(to_free);
      }
    } else if (0 == strcmp(method, "POST")) {
      /*
       * Execute all the POST commands
       * POST commands are only commands to set data values
       * For example change a sensor name, add an action, etc.
       * All data are sanitized on input, never on output,
       * so all data are supposed to be safe in the database
       */
      command = strtok_r( NULL, delim, &saveptr );
      if (command != NULL) {
        if (*upload_data_size != 0) {
          MHD_post_process (con_info->postprocessor, upload_data, *upload_data_size);
          *upload_data_size = 0;
          return MHD_YES;
        } else {
          if (0 == strncmp(SETDEVICEDATA, command, strlen(SETDEVICEDATA))) { // Set device data
            cur_device = (struct _device *)con_info->data;
            cur_terminal = get_device_from_name(cur_device->name, config->terminal, config->nb_terminal);
            to_free = set_device_data( config->master_db, *cur_device);
            if (to_free != NULL) {
              cur_terminal->enabled = cur_device->enabled;
              tf_len = snprintf(NULL, 0, json_template_webserver_setdevicedata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setdevicedata, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setdevicedata_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setdevicedata_error);
            }
          } else if (0 == strncmp(SETSWITCHDATA, command, strlen(SETSWITCHDATA))) { // Set switcher data
            cur_switch = (struct _switcher *)con_info->data;
            to_free = set_switch_data( config->master_db, *cur_switch);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setswitchdata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setswitchdata, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setswitchdata_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setswitchdata_error);
            }
          } else if (0 == strncmp(SETSENSORDATA, command, strlen(SETSENSORDATA))) { // Set sensor data
            cur_sensor = (struct _sensor *)con_info->data;
            to_free = set_sensor_data( config->master_db, *cur_sensor);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setsensordata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setsensordata, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setsensordata_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setsensordata_error);
            }
          } else if (0 == strncmp(SETHEATERDATA, command, strlen(SETHEATERDATA))) { // Set heater data
            cur_heater = (struct _heater *)con_info->data;
            to_free = set_heater_data( config->master_db, *cur_heater);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setheaterdata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setheaterdata, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setheaterdata_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setheaterdata_error);
            }
          } else if (0 == strncmp(SETDIMMERDATA, command, strlen(SETDIMMERDATA))) { // Set dimmer data
            cur_dimmer = (struct _dimmer *)con_info->data;
            to_free = set_dimmer_data( config->master_db, *cur_dimmer);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setdimmerdata, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setdimmerdata, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setdimmerdata_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setdimmerdata_error);
            }
          } else if (0 == strncmp(ADDACTION, command, strlen(ADDACTION))) {
            cur_action = (struct _action *)con_info->data;
            to_free = add_action( config->master_db, * cur_action);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_addaction, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_addaction, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_addaction_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_addaction_error);
            }
          } else if (0 == strncmp(SETACTION, command, strlen(SETACTION))) {
            cur_action = (struct _action *)con_info->data;
            to_free = set_action( config->master_db, * cur_action);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setaction, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setaction, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setaction_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setaction_error);
            }
          } else if (0 == strncmp(ADDSCRIPT, command, strlen(ADDSCRIPT))) {
            cur_script = (struct _script *)con_info->data;
            to_free = add_script( config->master_db, *cur_script);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_addscript, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_addscript, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_addscript_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_addscript_error);
            }
          } else if (0 == strncmp(SETSCRIPT, command, strlen(SETSCRIPT))) {
            cur_script = (struct _script *)con_info->data;
            to_free = set_script( config->master_db, *cur_script);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setscript, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setscript, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setscript_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setscript_error);
            }
          } else if (0 == strncmp(ADDSCHEDULE, command, strlen(ADDSCHEDULE))) {
            cur_schedule = (struct _schedule *)con_info->data;
            to_free = add_schedule( config->master_db, *cur_schedule);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_addschedule, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_addschedule, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_addschedule_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_addschedule_error);
            }
          } else if (0 == strncmp(SETSCHEDULE, command, strlen(SETSCHEDULE))) {
            cur_schedule = (struct _schedule *)con_info->data;
            to_free = set_schedule( config->master_db, *cur_schedule);
            if (to_free != NULL) {
              tf_len = snprintf(NULL, 0, json_template_webserver_setschedule, to_free);
              page = malloc((tf_len+1)*sizeof(char));
              snprintf(page, (tf_len+1)*sizeof(char), json_template_webserver_setschedule, to_free);
              free(to_free);
            } else {
              page_len = strlen(json_template_webserver_setschedule_error);
              page = malloc((page_len+1)*sizeof(char));
              snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_setschedule_error);
            }
          } else {
            to_free = get_json_list_commands();
            page_len = snprintf(NULL, 0, json_template_webserver_unknown_command, command, command, to_free);
            page = malloc((page_len+1)*sizeof(char));
            snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_unknown_command, command, command, to_free);
            free(to_free);
          }
        }
      } else {
        to_free = get_json_list_commands();
        page_len = snprintf(NULL, 0, json_template_webserver_command_empty, to_free);
        page = malloc((page_len+1)*sizeof(char));
        snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_command_empty, to_free);
        free(to_free);
      }
    } else {
      page_len = strlen(json_template_webserver_wrong_http_method);
      page = malloc(page_len+sizeof(char));
      snprintf(page, (page_len+sizeof(char)), json_template_webserver_wrong_http_method);
    }
  } else {
    page_len = snprintf(NULL, 0, json_template_webserver_wrong_prefix, prefix, config->url_prefix);
    page = malloc((page_len+1)*sizeof(char));
    snprintf(page, (page_len+1)*sizeof(char), json_template_webserver_wrong_prefix, prefix, config->url_prefix);
  }
  
  response = MHD_create_response_from_buffer (strlen (page), (void *) page, MHD_RESPMEM_MUST_FREE );
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  log_message(LOG_LEVEL_INFO, "End execution of angharad_rest_webservice, from %s, url: %s", inet_ntoa(((struct sockaddr_in *)so_client)->sin_addr), url);
  if (global_handler_variable == ANGHARAD_RESTART) {
    exit_server(&config, ANGHARAD_STOP);
  }
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

  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  switch (con_info->data_type) {
    case DATA_DEVICE:
      cur_device = (struct _device*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_device->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_device->display, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_device->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_device->tags, MSGLENGTH*sizeof(char));
        }
      }
      break;
    case DATA_SWITCH:
      cur_switch = (struct _switcher *)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_switch->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_switch->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_switch->display, WORDLENGTH*sizeof(char));
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
          sanitize_json_string(data, cur_switch->tags, MSGLENGTH*sizeof(char));
        }
      }
      break;
    case DATA_SENSOR:
      cur_sensor = (struct _sensor*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_sensor->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_sensor->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_sensor->display, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "unit")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_sensor->unit, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "value_type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_sensor->value_type = strtol(data, NULL, 10);
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
          sanitize_json_string(data, cur_sensor->tags, MSGLENGTH*sizeof(char));
        }
      }
    break;
    case DATA_HEATER:
      cur_heater = (struct _heater*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_heater->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_heater->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_heater->display, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "unit")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_heater->unit, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "value_type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_heater->value_type = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_heater->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_heater->monitored=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored_every")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_heater->monitored_every=strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_heater->tags, MSGLENGTH*sizeof(char));
        }
      }
    break;
    case DATA_DIMMER:
      cur_dimmer = (struct _dimmer*)con_info->data;
      if (0 == strcmp (key, "name")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_dimmer->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_dimmer->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "display")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_dimmer->display, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_dimmer->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_dimmer->monitored=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "monitored_every")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_dimmer->monitored_every=strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_dimmer->tags, MSGLENGTH*sizeof(char));
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
          sanitize_json_string(data, cur_action->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_action->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "type")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_action->type = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "switcher")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_action->switcher, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "dimmer")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_action->dimmer, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "heater")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_action->heater, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "params")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_action->params, MSGLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_action->tags, MSGLENGTH*sizeof(char));
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
          sanitize_json_string(data, cur_script->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_script->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_script->enabled=(0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "actions")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_script->actions, MSGLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          sanitize_json_string(data, cur_script->tags, MSGLENGTH*sizeof(char));
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
          sanitize_json_string(data, cur_schedule->name, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "device")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          sanitize_json_string(data, cur_schedule->device, WORDLENGTH*sizeof(char));
        }
      } else if (0 == strcmp (key, "enabled")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->enabled = (0==strcmp("true", data))?1:0;
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
          cur_schedule->remove_after_done = (0==strcmp("true", data))?1:0;
        }
      } else if (0 == strcmp (key, "script")) {
        if ((size > 0) && (size <= WORDLENGTH)) {
          cur_schedule->script = strtol(data, NULL, 10);
        }
      } else if (0 == strcmp (key, "tags")) {
        if ((size > 0) && (size <= MSGLENGTH)) {
          snprintf(cur_schedule->tags, MSGLENGTH*sizeof(char), "%s", data);
        }
      }
      break;
    default:
      log_message(LOG_LEVEL_ERROR, "Error parsing POST data: unknown data type");
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
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
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
