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
 * Entry point for devices calls
 *
 */

#include "angharad.h"

int connect_device(device * terminal, device ** terminals, unsigned int nb_terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return connect_device_arduino(terminal, terminals, nb_terminal);
      break;
    case TYPE_ZWAVE:
      return connect_device_zwave(terminal, terminals, nb_terminal);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int reconnect_device(device * terminal, device ** terminals, unsigned int nb_terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return reconnect_device_arduino(terminal, terminals, nb_terminal);
      break;
    case TYPE_ZWAVE:
      return reconnect_device_zwave(terminal, terminals, nb_terminal);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int close_device(device * terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return close_device_arduino(terminal);
      break;
    case TYPE_ZWAVE:
      return close_device_zwave(terminal);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int is_connected(device * terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return is_connected_arduino(terminal);
      break;
    case TYPE_ZWAVE:
      return is_connected_zwave(terminal);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
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
	char sql_query[MSGLENGTH+1];
  char cur_name[WORDLENGTH+1] = {0}, sanitized[WORDLENGTH+1] = {0};
	char * output = malloc(2*sizeof(char)), one_item[2*MSGLENGTH+1], * tags = NULL, ** tags_array = NULL;

  strcpy(output, "");
  for (i=0; i<nb_terminal; i++) {
    strcpy(cur_name, "");
    strcpy(one_item, "");
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT de_display, de_active FROM an_device WHERE de_name = '%q'", terminal[i]->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query");
      sqlite3_finalize(stmt);
      free(output);
      return NULL;
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        snprintf(cur_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
        if (strlen(output) > 0) {
          strncat(one_item, ",", MSGLENGTH);
        }
        tags_array = get_tags(sqlite3_db, NULL, DATA_DEVICE, terminal[i]->name);
        tags = build_json_tags(tags_array);
        strncat(one_item, "{\"name\":\"", MSGLENGTH);
        sanitize_json_string(terminal[i]->name, sanitized, WORDLENGTH);
        strncat(one_item, sanitized, MSGLENGTH);
        strncat(one_item, "\",\"display\":\"", MSGLENGTH);
        sanitize_json_string(cur_name, sanitized, WORDLENGTH);
        strncat(one_item, sanitized, MSGLENGTH);
        strncat(one_item, "\",\"connected\":", MSGLENGTH);
        strncat(one_item, (is_connected(terminal[i]))?"true":"false", MSGLENGTH);
        strncat(one_item, ",\"enabled\":", MSGLENGTH);
        strncat(one_item, sqlite3_column_int(stmt, 1)==1?"true":"false", MSGLENGTH);
        strncat(one_item, ",\"tags\":", MSGLENGTH);
        strncat(one_item, tags, MSGLENGTH);
        strncat(one_item, "}", MSGLENGTH);
        free(tags);
        free_tags(tags_array);
      } else {
        if (strlen(output) > 0) {
          strncat(one_item, ",", MSGLENGTH);
        }
        strncat(one_item, "{\"name\":\"", MSGLENGTH);
        sanitize_json_string(terminal[i]->name, sanitized, WORDLENGTH);
        strncat(one_item, sanitized, MSGLENGTH);
        strncat(one_item, "\",\"display\":\"", MSGLENGTH);
        strncat(one_item, sanitized, MSGLENGTH);
        strncat(one_item, "\",\"enabled\":", MSGLENGTH);
        strncat(one_item, (is_connected(terminal[i]))?"true":"false", MSGLENGTH);
        strncat(one_item, "}", MSGLENGTH);
        
        // Creating default value
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_device (de_name, de_display, de_active) VALUES ('%q', '%q', 1)", terminal[i]->name, terminal[i]->name);
        if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          log_message(LOG_INFO, "Error inserting an_device %s", sql_query);
        }
      }
    }
    sqlite3_finalize(stmt);
    output = realloc(output, strlen(output)+strlen(one_item)+1);
    strcat(output, one_item);
  }
  return output;
}

int send_heartbeat(device * terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return send_heartbeat_arduino(terminal);
      break;
    case TYPE_ZWAVE:
      return send_heartbeat_zwave(terminal);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int set_switch_state(device * terminal, char * switcher, int status) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return set_switch_state_arduino(terminal, switcher, status);
      break;
    case TYPE_ZWAVE:
      return set_switch_state_zwave(terminal, switcher, status);
      break;
    case TYPE_NONE:
    default:
      return ERROR_SWITCH;
      break;
  }
}

int get_switch_state(device * terminal, char * switcher, int force) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_switch_state_arduino(terminal, switcher, force);
      break;
    case TYPE_ZWAVE:
      return get_switch_state_zwave(terminal, switcher, force);
      break;
    case TYPE_NONE:
    default:
      return ERROR_SWITCH;
      break;
  }
}

int toggle_switch_state(device * terminal, char * switcher) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return toggle_switch_state_arduino(terminal, switcher);
      break;
    case TYPE_ZWAVE:
      return toggle_switch_state_zwave(terminal, switcher);
      break;
    case TYPE_NONE:
    default:
      return ERROR_SWITCH;
      break;
  }
}

float get_sensor_value(device * terminal, char * sensor, int force) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_sensor_value_arduino(terminal, sensor, force);
      break;
    case TYPE_ZWAVE:
      return get_sensor_value_zwave(terminal, sensor, force);
      break;
    case TYPE_NONE:
    default:
      return ERROR_SENSOR;
      break;
  }
}

char * get_overview(sqlite3 * sqlite3_db, device * terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_overview_arduino(sqlite3_db, terminal);
      break;
    case TYPE_ZWAVE:
      return get_overview_zwave(sqlite3_db, terminal);
      break;
    case TYPE_NONE:
    default:
      return NULL;
      break;
  }
}

char * get_refresh(sqlite3 * sqlite3_db, device * terminal) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_refresh_arduino(sqlite3_db, terminal);
      break;
    case TYPE_ZWAVE:
      return get_refresh_zwave(sqlite3_db, terminal);
      break;
    case TYPE_NONE:
    default:
      return NULL;
      break;
  }
}

int get_name(device * terminal, char * output) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_name_arduino(terminal, output);
      break;
    case TYPE_ZWAVE:
      return get_name_zwave(terminal, output);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int get_heater(device * terminal, char * heat_id, char * buffer) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_heater_arduino(terminal, heat_id, buffer);
      break;
    case TYPE_ZWAVE:
      return get_heater_zwave(terminal, heat_id, buffer);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int set_heater(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * buffer) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return set_heater_arduino(terminal, heat_id, heat_enabled, max_heat_value, buffer);
      break;
    case TYPE_ZWAVE:
      return set_heater_zwave(terminal, heat_id, heat_enabled, max_heat_value, buffer);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int get_dimmer_value(device * terminal, char * dimmer) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return get_dimmer_value_arduino(terminal, dimmer);
      break;
    case TYPE_ZWAVE:
      return get_dimmer_value_zwave(terminal, dimmer);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

int set_dimmer_value(device * terminal, char * dimmer, int value) {
  switch (terminal->type) {
    case TYPE_SERIAL:
      return set_dimmer_value_arduino(terminal, dimmer, value);
      break;
    case TYPE_ZWAVE:
      return set_dimmer_value_zwave(terminal, dimmer, value);
      break;
    case TYPE_NONE:
    default:
      return 0;
      break;
  }
}

