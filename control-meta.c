/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * Entry point for devices calls
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

static const char json_template_control_meta_getdevices[] = "{\"name\":\"%s\",\"display\":\"%s\",\"connected\":%s,\"enabled\":%s,\"tags\":%s}";
static const char json_template_control_meta_overview_final[] = "{\"name\":\"%s\",\"switches\":%s,\"sensors\":%s,\"heaters\":%s,\"dimmers\":%s}";
static const char json_template_control_meta_overview_switcher[] = "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"type\":%d,\"status\":%d,\"monitored\":%s,\"monitored_every\":%d,\"monitored_next\":%ld,\"tags\":%s}";
static const char json_template_control_meta_overview_sensor[] = "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"value\":%s,\"unit\":\"%s\",\"value_type\":%d,\"monitored\":%s,\"monitored_every\":%d,\"monitored_next\":%ld,\"tags\":%s}";
static const char json_template_control_meta_overview_heater[] = "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\",\"value_type\":%d,\"monitored\":%s,\"monitored_every\":%d,\"monitored_next\":%ld,\"tags\":%s}";
static const char json_template_control_meta_overview_dimmer[] = "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"value\":%d,\"monitored\":%s,\"monitored_every\":%d,\"monitored_next\":%ld,\"tags\":%s}";
static const char json_template_control_meta_empty[] = "{}";

/**
 * Connect the specified device
 */
int connect_device(device * terminal, device ** terminals, unsigned int nb_terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return connect_device_arduino(terminal, terminals, nb_terminal);
      break;
    case TYPE_ZWAVE:
      return connect_device_zwave(terminal, terminals, nb_terminal);
      break;
    case TYPE_NET:
      return connect_device_net(terminal, terminals, nb_terminal);
      break;
    case TYPE_NONE:
    default:
      return -1;
      break;
  }
}

/**
 * Reconnect the specified device
 */
int reconnect_device(device * terminal, device ** terminals, unsigned int nb_terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return reconnect_device_arduino(terminal, terminals, nb_terminal);
      break;
    case TYPE_ZWAVE:
      return reconnect_device_zwave(terminal, terminals, nb_terminal);
      break;
    case TYPE_NET:
      return reconnect_device_net(terminal, terminals, nb_terminal);
      break;
    case TYPE_NONE:
    default:
      return -1;
      break;
  }
}

/**
 * Close the specified device
 */
int close_device(device * terminal) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return close_device_arduino(terminal);
        break;
      case TYPE_ZWAVE:
        return close_device_zwave(terminal);
        break;
      case TYPE_NET:
        return close_device_net(terminal);
        break;
      case TYPE_NONE:
      default:
        return 0;
        break;
    }
  }
  return 0;
}

/**
 * Check if the specified device is connected
 */
int is_connected(device * terminal) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return is_connected_arduino(terminal);
        break;
      case TYPE_ZWAVE:
        return is_connected_zwave(terminal);
        break;
      case TYPE_NET:
        return is_connected_net(terminal);
        break;
      case TYPE_NONE:
      default:
        return 0;
        break;
    }
  }
  return 0;
}

/**
 * get the first device that has the given name
 */
device * get_device_from_name(char * device_name, device ** terminal, unsigned int nb_terminal) {
  int i;
  for (i=0; i<nb_terminal; i++) {
    if (terminal[i] != NULL && 0 == strncmp(terminal[i]->name, device_name, WORDLENGTH)) {
      return terminal[i];
    }
  }
  return NULL;
}

/**
 * Get the list of all devices
 */
char * get_devices(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal) {
  int i;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
	char * sql_query = NULL;
  char cur_name[WORDLENGTH+1] = {0}, cur_display[WORDLENGTH+1] = {0}, cur_active[WORDLENGTH+1] = {0};
	char * output = malloc(sizeof(char)), * one_item = NULL, * tags = NULL, ** tags_array = NULL;
  int str_len;

  strcpy(output, "");
  for (i=0; i<nb_terminal; i++) {
    strcpy(cur_name, "");
    sql_query = sqlite3_mprintf("SELECT de_display, de_active FROM an_device WHERE de_name = '%q'", terminal[i]->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_LEVEL_INFO, "Error preparing sql query (get_devices)");
      one_item = malloc(3*sizeof(char));
      strcpy(one_item, json_template_control_meta_empty);
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result != SQLITE_ROW) {
        // Creating default value
        sql_query = sqlite3_mprintf("INSERT INTO an_device (de_name, de_display, de_active) VALUES ('%q', '%q', 1)",
                        terminal[i]->name, terminal[i]->name);
        if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          log_message(LOG_LEVEL_INFO, "Error inserting an_device %s", sql_query);
        }
        sqlite3_free(sql_query);
        strncpy(cur_name, terminal[i]->name, WORDLENGTH);
        strncpy(cur_display, terminal[i]->name, WORDLENGTH);
        strcpy(cur_active, "true");
      } else {
        strncpy(cur_name, terminal[i]->name, WORDLENGTH);
        strncpy(cur_display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
        strcpy(cur_active, sqlite3_column_int(stmt, 1)==1?"true":"false");
      }
      
      tags_array = get_tags(sqlite3_db, NULL, DATA_DEVICE, terminal[i]->name);
      tags = build_json_tags(tags_array);
      
      str_len = snprintf(NULL, 0, json_template_control_meta_getdevices, cur_name, cur_display, 
                        (is_connected(terminal[i]))?"true":"false", cur_active, tags);

      one_item = malloc((str_len+1)*sizeof(char));
      
      snprintf(one_item, (str_len+1)*sizeof(char), json_template_control_meta_getdevices, 
                cur_name, cur_display, (is_connected(terminal[i]))?"true":"false", cur_active, tags);
      
      free(tags);
      free_tags(tags_array);
    }
    sqlite3_finalize(stmt);
    if (strlen(output) > 0) {
      output = realloc(output, strlen(output)+strlen(one_item)+2);
      strcat(output, ",");
      strcat(output, one_item);
    } else {
      output = realloc(output, strlen(output)+strlen(one_item)+1);
      strcat(output, one_item);
    }
    free(one_item);
  }
  return output;
}

/**
 * Check if the device is alive by sending a command that expects a specific answer
 */
int send_heartbeat(device * terminal) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return send_heartbeat_arduino(terminal);
        break;
      case TYPE_ZWAVE:
        return send_heartbeat_zwave(terminal);
        break;
      case TYPE_NET:
        return send_heartbeat_net(terminal);
        break;
      case TYPE_NONE:
      default:
        return 0;
        break;
    }
  }
  return 0;
}

/**
 * set the status of a switch
 */
int set_switch_state(device * terminal, char * switcher, int status) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return set_switch_state_arduino(terminal, switcher, status);
        break;
      case TYPE_ZWAVE:
        return set_switch_state_zwave(terminal, switcher, status);
        break;
      case TYPE_NET:
        return set_switch_state_net(terminal, switcher, status);
        break;
      case TYPE_NONE:
      default:
        return ERROR_SWITCH;
        break;
    }
  }
  return ERROR_SWITCH;
}

/**
 * get the status of a switch
 */
int get_switch_state(device * terminal, char * switcher, int force) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return get_switch_state_arduino(terminal, switcher, force);
        break;
      case TYPE_ZWAVE:
        return get_switch_state_zwave(terminal, switcher, force);
        break;
      case TYPE_NET:
        return get_switch_state_net(terminal, switcher, force);
        break;
      case TYPE_NONE:
      default:
        return ERROR_SWITCH;
        break;
    }
  }
  return ERROR_SWITCH;
}

/**
 * toggle the status of a switch
 */
int toggle_switch_state(device * terminal, char * switcher) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return toggle_switch_state_arduino(terminal, switcher);
        break;
      case TYPE_ZWAVE:
        return toggle_switch_state_zwave(terminal, switcher);
        break;
      case TYPE_NET:
        return toggle_switch_state_net(terminal, switcher);
        break;
      case TYPE_NONE:
      default:
        return ERROR_SWITCH;
        break;
    }
  }
  return ERROR_SWITCH;
}

/**
 * Return the value of a sensor
 */
float get_sensor_value(sqlite3 * sqlite3_db, device * terminal, char * sensor, int force) {
  int is_fahrenheit = 0;
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;

  if (terminal != NULL && terminal->enabled) {
    
    sql_query = sqlite3_mprintf("SELECT se_value_type FROM an_sensor\
                                  WHERE se_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", sensor, terminal->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_LEVEL_WARNING, "Error preparing sql query get_sensor_value");
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        is_fahrenheit = (sqlite3_column_int(stmt, 0) == VALUE_TYPE_FAHRENHEIT);
      }
    }
    sqlite3_finalize(stmt);
    switch (terminal->type) {
      case TYPE_SERIAL:
        return (is_fahrenheit?fahrenheit_to_celsius(get_sensor_value_arduino(terminal, sensor, force)):get_sensor_value_arduino(terminal, sensor, force));
        break;
      case TYPE_ZWAVE:
        return (is_fahrenheit?fahrenheit_to_celsius(get_sensor_value_zwave(terminal, sensor, force)):get_sensor_value_zwave(terminal, sensor, force));
        break;
      case TYPE_NET:
        return (is_fahrenheit?fahrenheit_to_celsius(get_sensor_value_net(terminal, sensor, force)):get_sensor_value_net(terminal, sensor, force));
        break;
      case TYPE_NONE:
      default:
        return ERROR_SENSOR;
        break;
    }
  }
  return ERROR_SENSOR;
}

/**
 * Returns an overview of all zwave devices connected and their last status
 */
char * get_overview(sqlite3 * sqlite3_db, device * terminal) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return get_overview_arduino(sqlite3_db, terminal);
        break;
      case TYPE_ZWAVE:
        return get_overview_zwave(sqlite3_db, terminal);
        break;
      case TYPE_NET:
        return get_overview_net(sqlite3_db, terminal);
        break;
      case TYPE_NONE:
      default:
        return NULL;
        break;
    }
  }
  return NULL;
}

/**
 * Refresh the zwave devices values
 */
char * get_refresh(sqlite3 * sqlite3_db, device * terminal) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return get_refresh_arduino(sqlite3_db, terminal);
        break;
      case TYPE_ZWAVE:
        return get_refresh_zwave(sqlite3_db, terminal);
        break;
      case TYPE_NET:
        return get_refresh_net(sqlite3_db, terminal);
        break;
      case TYPE_NONE:
      default:
        return NULL;
        break;
    }
  }
  return NULL;
}

/**
 * Builds the overview output based on all the elements given in parameter
 */
char * build_overview_output(sqlite3 * sqlite3_db, char * device_name, switcher * switchers, int nb_switchers, sensor * sensors, int nb_sensors, heater * heaters, int nb_heaters, dimmer * dimmers, int nb_dimmers) {
  char * str_switches = NULL, * str_sensors = NULL, * str_heaters = NULL, * str_dimmers = NULL, * one_element = NULL;
  char * tags = NULL, ** tags_array = NULL;
  char * output = NULL;

  int i, output_len, str_len;
  
  // Arranging the results of an overview
  
  // Build switchers string
  str_switches = malloc(2*sizeof(char));
  strcpy(str_switches, "[");
  for (i=0; i<nb_switchers; i++) {
    tags_array = get_tags(sqlite3_db, device_name, DATA_SWITCH, switchers[i].name);
    tags = build_json_tags(tags_array);

    str_len = snprintf(NULL, 0, json_template_control_meta_overview_switcher, 
                        i>0?",":"", switchers[i].name, switchers[i].display, switchers[i].enabled?"true":"false", switchers[i].type, switchers[i].status,
                        switchers[i].monitored?"true":"false", switchers[i].monitored_every, switchers[i].monitored_next, tags);
    
    one_element = malloc((str_len+1)*sizeof(char));
    snprintf(one_element, (str_len+1)*sizeof(char), json_template_control_meta_overview_switcher,
                        i>0?",":"", switchers[i].name, switchers[i].display, switchers[i].enabled?"true":"false", switchers[i].type, switchers[i].status,
                        switchers[i].monitored?"true":"false", switchers[i].monitored_every, switchers[i].monitored_next, tags);
    
    str_switches = realloc(str_switches, strlen(str_switches)+strlen(one_element)+1);
    strcat(str_switches, one_element);
    free(tags);
    free_tags(tags_array);
    free(one_element);
  }
  str_switches = realloc(str_switches, strlen(str_switches)+2);
  strcat(str_switches, "]");
  
  // Build sensors string
  str_sensors = malloc(2*sizeof(char));
  strcpy(str_sensors, "[");
  for (i=0; i<nb_sensors; i++) {
    tags_array = get_tags(sqlite3_db, device_name, DATA_SENSOR, sensors[i].name);
    tags = build_json_tags(tags_array);

    str_len = snprintf(NULL, 0, json_template_control_meta_overview_sensor, i>0?",":"", sensors[i].name, sensors[i].display, 
                        sensors[i].enabled?"true":"false", sensors[i].value, sensors[i].unit, sensors[i].value_type,
                        sensors[i].monitored?"true":"false", sensors[i].monitored_every, sensors[i].monitored_next, tags);
    
    one_element = malloc((str_len+1)*sizeof(char));
    snprintf(one_element, (str_len+1)*sizeof(char), json_template_control_meta_overview_sensor,i>0?",":"", sensors[i].name, 
                        sensors[i].display, sensors[i].enabled?"true":"false", sensors[i].value, sensors[i].unit, sensors[i].value_type,
                        sensors[i].monitored?"true":"false", sensors[i].monitored_every, sensors[i].monitored_next, tags);
    
    str_sensors = realloc(str_sensors, strlen(str_sensors)+strlen(one_element)+1);
    strcat(str_sensors, one_element);
    free(tags);
    free_tags(tags_array);
    free(one_element);
  }
  str_sensors = realloc(str_sensors, strlen(str_sensors)+2);
  strcat(str_sensors, "]");
  
  // Build heaters string
  str_heaters = malloc(2*sizeof(char));
  strcpy(str_heaters, "[");
  for (i=0; i<nb_heaters; i++) {
    tags_array = get_tags(sqlite3_db, device_name, DATA_HEATER, heaters[i].name);
    tags = build_json_tags(tags_array);
    
    str_len = snprintf(NULL, 0, json_template_control_meta_overview_heater, 
                        i>0?",":"", heaters[i].name, heaters[i].display, heaters[i].enabled?"true":"false", heaters[i].set?"true":"false",
                        heaters[i].on?"true":"false", heaters[i].heat_max_value, heaters[i].unit, heaters[i].value_type,
                        heaters[i].monitored?"true":"false", heaters[i].monitored_every, heaters[i].monitored_next, tags);
    
    one_element = malloc((str_len+1)*sizeof(char));
    snprintf(one_element, (str_len+1)*sizeof(char), json_template_control_meta_overview_heater,
              i>0?",":"", heaters[i].name, heaters[i].display, heaters[i].enabled?"true":"false", heaters[i].set?"true":"false",
              heaters[i].on?"true":"false", heaters[i].heat_max_value, heaters[i].unit, heaters[i].value_type,
              heaters[i].monitored?"true":"false", heaters[i].monitored_every, heaters[i].monitored_next, tags);
    
    str_heaters = realloc(str_heaters, strlen(str_heaters)+strlen(one_element)+1);
    strcat(str_heaters, one_element);
    free(tags);
    free_tags(tags_array);
    free(one_element);
  }
  str_heaters = realloc(str_heaters, strlen(str_heaters)+2);
  strcat(str_heaters, "]");
  
  // Build dimmers string
  str_dimmers = malloc(2*sizeof(char));
  strcpy(str_dimmers, "[");
  for (i=0; i<nb_dimmers; i++) {
    tags_array = get_tags(sqlite3_db, device_name, DATA_DIMMER, dimmers[i].name);
    tags = build_json_tags(tags_array);

    str_len = snprintf(NULL, 0, json_template_control_meta_overview_dimmer,
                        i>0?",":"", dimmers[i].name, dimmers[i].display, dimmers[i].enabled?"true":"false", dimmers[i].value, 
                        dimmers[i].monitored?"true":"false", dimmers[i].monitored_every, dimmers[i].monitored_next, tags);
    
    one_element = malloc((str_len+1)*sizeof(char));
    snprintf(one_element, (str_len+1)*sizeof(char), json_template_control_meta_overview_dimmer,
                        i>0?",":"", dimmers[i].name, dimmers[i].display, dimmers[i].enabled?"true":"false", dimmers[i].value, 
                        dimmers[i].monitored?"true":"false", dimmers[i].monitored_every, dimmers[i].monitored_next, tags);
    
    str_dimmers = realloc(str_dimmers, strlen(str_dimmers)+strlen(one_element)+1);
    strcat(str_dimmers, one_element);
    free(tags);
    free_tags(tags_array);
    free(one_element);
  }
  str_dimmers = realloc(str_dimmers, strlen(str_dimmers)+2);
  strcat(str_dimmers, "]");
  
  output_len = snprintf(NULL, 0, json_template_control_meta_overview_final, device_name, str_switches, str_sensors, str_heaters, str_dimmers);
  output = malloc((output_len+1)*sizeof(char));
  snprintf(output, (output_len+1)*sizeof(char), json_template_control_meta_overview_final, device_name, str_switches, str_sensors, str_heaters, str_dimmers);
  
  // Free all allocated pointers before return
  free(str_switches);
  str_switches = NULL;
  free(str_sensors);
  str_sensors = NULL;
  free(str_heaters);
  str_heaters = NULL;
  free(str_dimmers);
  str_dimmers = NULL;
  return output;
}

/**
 * Get the current heater command
 */
heater * get_heater(sqlite3 * sqlite3_db, device * terminal, char * heat_id) {

  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return get_heater_arduino(sqlite3_db, terminal, heat_id);
        break;
      case TYPE_ZWAVE:
        return get_heater_zwave(sqlite3_db, terminal, heat_id);
        break;
      case TYPE_NET:
        return get_heater_net(sqlite3_db, terminal, heat_id);
        break;
      case TYPE_NONE:
      default:
        return NULL;
        break;
    }
  }
  return NULL;
}

/**
 * Change the heater command
 */
heater * set_heater(sqlite3 * sqlite3_db, device * terminal, char * heat_id, int heat_enabled, float max_heat_value) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return set_heater_arduino(sqlite3_db, terminal, heat_id, heat_enabled, max_heat_value);
        break;
      case TYPE_ZWAVE:
        return set_heater_zwave(sqlite3_db, terminal, heat_id, heat_enabled, max_heat_value);
        break;
      case TYPE_NET:
        return set_heater_net(sqlite3_db, terminal, heat_id, heat_enabled, max_heat_value);
        break;
      case TYPE_NONE:
      default:
        return NULL;
        break;
    }
  }
  return NULL;
}

/**
 * Get the dimmer value (obviously)
 */
int get_dimmer_value(device * terminal, char * dimmer) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return get_dimmer_value_arduino(terminal, dimmer);
        break;
      case TYPE_ZWAVE:
        return get_dimmer_value_zwave(terminal, dimmer);
        break;
      case TYPE_NET:
        return get_dimmer_value_net(terminal, dimmer);
        break;
      case TYPE_NONE:
      default:
        return 0;
        break;
    }
  }
  return 0;
}

/**
 * Change the dimmer value
 */
int set_dimmer_value(device * terminal, char * dimmer, int value) {
  if (terminal != NULL && terminal->enabled) {
    switch (terminal->type) {
      case TYPE_SERIAL:
        return set_dimmer_value_arduino(terminal, dimmer, value);
        break;
      case TYPE_ZWAVE:
        return set_dimmer_value_zwave(terminal, dimmer, value);
        break;
      case TYPE_NET:
        return set_dimmer_value_net(terminal, dimmer, value);
        break;
      case TYPE_NONE:
      default:
        return 0;
        break;
    }
  }
  return 0;
}

