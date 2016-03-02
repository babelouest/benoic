/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * All functions that are not related to hardware calls
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

static const char json_template_tools_get_monitor[] = "{\"device\":\"%s\",\"switcher\":\"%s\",\"sensor\":\"%s\",\"dimmer\":\"%s\",\"heater\":\"%s\",\"start_date\":\"%s\",\"values\":[%s]}";
static const char json_template_tools_monitor_one_value[] = "{\"date_time\":\"%s\",\"value\":\"%s\"}";
/**
 * Save the heat status in the database for startup init
 */
int save_startup_heater_status(sqlite3 * sqlite3_db, char * device, char * heater_name, int heat_enabled, float max_heat_value) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, he_id;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query = sqlite3_mprintf("SELECT he_id FROM an_heater WHERE he_name = '%q'\
                    AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", heater_name, device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (save_startup_heater_status)");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      he_id = sqlite3_column_int(stmt, 0);
      sql_query = sqlite3_mprintf("UPDATE an_heater SET he_set='%d', he_max_heat_value='%.2f' WHERE he_id='%d'", heat_enabled, max_heat_value, he_id);
    } else {
      sql_query = sqlite3_mprintf("INSERT INTO an_heater (he_id, he_name, de_id, he_set, he_max_heat_value)\
                        VALUES ((SELECT he_id FROM an_heater WHERE he_name = '%q'\
                        AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q')),\
                        '%q', (SELECT de_id FROM an_device WHERE de_name='%q'), '%d', '%2f')",
                        heater_name, device, heater_name, device, heat_enabled, max_heat_value);
    }
    sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
    sqlite3_free(sql_query);
    sqlite3_finalize(stmt);
    return ( sql_result == SQLITE_OK );
  }
}

/**
 * Save the switch status in the database for startup init
 */
int save_startup_switch_status(sqlite3 * sqlite3_db, char * device, char * switcher, int status) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query = sqlite3_mprintf("SELECT sw_id FROM an_switch WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q') AND sw_name = '%q'", device, switcher);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (save_startup_switch_status)");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sql_query = sqlite3_mprintf("UPDATE an_switch SET sw_status='%d' WHERE sw_id='%d'", status, sqlite3_column_int(stmt, 0));
      sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
      sqlite3_free(sql_query);
      sqlite3_finalize(stmt);
      return ( sql_result == SQLITE_OK );
    } else {
      sqlite3_finalize(stmt);
      return 0;
    }
  }
}

/**
 * Save the dimmer value in the database for startup init
 */
int save_startup_dimmer_value(sqlite3 * sqlite3_db, char * device, char * dimmer, int value) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query = sqlite3_mprintf("SELECT di_id FROM an_dimmer\
                    WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q') AND di_name = '%q'", device, dimmer);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (save_startup_dimmer_value)");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sql_query = sqlite3_mprintf("UPDATE an_dimmer SET di_value='%d' WHERE di_id='%d'", value, sqlite3_column_int(stmt, 0));
      sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
      sqlite3_free(sql_query);
      sqlite3_finalize(stmt);
      return ( sql_result == SQLITE_OK );
    } else {
      sqlite3_finalize(stmt);
      return 0;
    }
  }
}

/**
 * Set all the switch to on if their status is on in the database
 */
int set_startup_all_switch(sqlite3 * sqlite3_db, device * cur_device) {
  char * sql_query = NULL, switch_name[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, state_result = 1;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  // Do not initiate elements on device connect, because it takes a few seconds to pair elements
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    sql_query = sqlite3_mprintf("SELECT sw_name, sw_status FROM an_switch\
                      WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q') AND sw_status = 1", cur_device->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_LEVEL_WARNING, "Error preparing sql query (set_startup_all_switch)");
      state_result = 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        snprintf(switch_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 0));
        if (set_switch_state(cur_device, switch_name, sqlite3_column_int(stmt, 1)) == ERROR_SWITCH) {
          log_message(LOG_LEVEL_WARNING, "Error setting switcher %s on device %s", switch_name, cur_device->name);
          state_result = 0;
        }
        row_result = sqlite3_step(stmt);
      }
    }
    sqlite3_finalize(stmt);
  }
  return state_result;
}

/**
 * Set all the dimmers values if their status is on in the database
 */
int set_startup_all_dimmer_value(sqlite3 * sqlite3_db, device * cur_device) {
  char * sql_query = NULL, dimmer_name[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, result = 1;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  // Do not initiate elements on device connect, because it takes a few seconds to pair elements
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    sql_query = sqlite3_mprintf("SELECT di_name, di_value FROM an_dimmer WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", cur_device->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_LEVEL_WARNING, "Error preparing sql query set_startup_all_dimmer");
      result = 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        snprintf(dimmer_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 0));
        if (set_dimmer_value(cur_device, dimmer_name, sqlite3_column_int(stmt, 1)) == ERROR_DIMMER) {
          log_message(LOG_LEVEL_WARNING, "Error setting dimmer %s on device %s", dimmer_name, cur_device->name);
          result = 0;
        }
        row_result = sqlite3_step(stmt);
      }
    }
    sqlite3_finalize(stmt);
  }
  return result;
}

/**
 * Get the heat status in the database for startup init
 */ 
heater * get_startup_heater_status(sqlite3 * sqlite3_db, char * device) {
  char * sql_query = NULL;
  heater * heaters = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, nb_heaters=0;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query = sqlite3_mprintf("SELECT he.he_id, he.he_name, de.de_name, he.he_enabled, he.he_set, he.he_max_heat_value\
                    FROM an_heater he LEFT OUTER JOIN an_device de on de.de_id = he.de_id\
                    WHERE he.de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (get_startup_heater_status)");
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
      heaters[nb_heaters].id = sqlite3_column_int(stmt, 0);
      snprintf(heaters[nb_heaters].name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(heaters[nb_heaters].device, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 2));
      heaters[nb_heaters].enabled = sqlite3_column_int(stmt, 3);
      heaters[nb_heaters].set = sqlite3_column_int(stmt, 4);
      heaters[nb_heaters].heat_max_value = (float)sqlite3_column_double(stmt, 5);
      row_result = sqlite3_step(stmt);
      nb_heaters++;
    }
    heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
    heaters[nb_heaters].id = -1;
  }
  sqlite3_finalize(stmt);
  return heaters;
}

/**
 * Initialize the device with the stored init values (heater and switches)
 */
int init_device_status(sqlite3 * sqlite3_db, device * cur_device) {
  heater * heaters, * cur_heater;
  int heat_status = 1, switch_status = 1, dimmer_status = 1, i=0;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    switch_status = set_startup_all_switch(sqlite3_db, cur_device);
    dimmer_status = set_startup_all_dimmer_value(sqlite3_db, cur_device);
    heaters = get_startup_heater_status(sqlite3_db, cur_device->name);
    for (i=0; heaters[i].id != -1; i++) {
      cur_heater = set_heater(sqlite3_db, cur_device, heaters[i].name, heaters[i].set, heaters[i].heat_max_value);
      if (cur_heater == NULL) {
        heat_status = 0;
      }
      free(cur_heater);
    }
    free(heaters);
    return (heat_status && switch_status && dimmer_status);
  } else {
    return 1;
  }
}

/**
 * Gets the monitored value using the given filters
 */
char * get_monitor(sqlite3 * sqlite3_db, const char * device_name, const char * switcher_name, const char * sensor_name, const char * dimmer_name, const char * heater_name, const char * start_date) {
  char * sql_query = NULL, * where_switch = NULL, * where_sensor = NULL, * where_dimmer = NULL, * where_heater = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, first_result = 1, t_len, str_len;
  char p_device[WORDLENGTH+1], p_switch[WORDLENGTH+1], p_sensor[WORDLENGTH+1], p_dimmer[WORDLENGTH+1], p_heater[WORDLENGTH+1];
  char p_start_date[WORDLENGTH+1], monitor_date[WORDLENGTH+1], monitor_value[WORDLENGTH+1], * one_item = NULL, * all_items = NULL;
  time_t yesterday;
  char * to_return = NULL;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  snprintf(p_device, WORDLENGTH*sizeof(char), "%s", device_name);
  snprintf(p_switch, WORDLENGTH*sizeof(char), "%s", switcher_name);
  snprintf(p_sensor, WORDLENGTH*sizeof(char), "%s", sensor_name);
  snprintf(p_dimmer, WORDLENGTH*sizeof(char), "%s", dimmer_name);
  snprintf(p_heater, WORDLENGTH*sizeof(char), "%s", heater_name);
  
  if (start_date != NULL && 0 != strcmp("", start_date)) {
    snprintf(p_start_date, WORDLENGTH*sizeof(char), "%s", start_date);
  } else {
    time(&yesterday);
    yesterday -= 60*60*24; // set start_date to yesterday
    snprintf(p_start_date, WORDLENGTH*sizeof(char), "%ld", yesterday);
  }
  
  if (switcher_name != NULL && 0 != strcmp("", switcher_name)) {
    where_switch = sqlite3_mprintf("AND mo.sw_id = (SELECT sw_id FROM an_switch WHERE sw_name='%q'\
                      AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_switch, p_device);
  } else {
    strcpy(p_switch, "");
    where_switch = sqlite3_mprintf("");
  }
  
  if (sensor_name != NULL && 0 != strcmp("", sensor_name)) {
    where_sensor = sqlite3_mprintf("AND mo.se_id = (SELECT se_id FROM an_sensor WHERE se_name='%q'\
                      AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_sensor, p_device);
  } else {
    strcpy(p_sensor, "");
    where_sensor = sqlite3_mprintf("");
  }
  
  if (dimmer_name != NULL && 0 != strcmp("", dimmer_name)) {
    where_dimmer = sqlite3_mprintf("AND mo.di_id = (SELECT di_id FROM an_dimmer WHERE di_name='%q'\
                      AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_dimmer, p_device);
  } else {
    strcpy(p_dimmer, "");
    where_dimmer = sqlite3_mprintf("");
  }
  
  if (heater_name != NULL && 0 != strcmp("", heater_name)) {
    where_heater = sqlite3_mprintf("AND mo.he_id = (SELECT he_id FROM an_heater WHERE he_name='%q'\
                      AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_heater, p_device);
  } else {
    strcpy(p_heater, "");
    where_heater = sqlite3_mprintf("");
  }
  
  sql_query = sqlite3_mprintf("SELECT mo.mo_date, mo.mo_result FROM an_monitor mo\
                    LEFT OUTER JOIN an_device de ON de.de_id = mo.de_id\
                    LEFT OUTER JOIN an_switch sw ON sw.sw_id = mo.sw_id\
                    LEFT OUTER JOIN an_sensor se ON se.se_id = mo.se_id\
                    LEFT OUTER JOIN an_dimmer di ON di.di_id = mo.di_id\
                    LEFT OUTER JOIN an_heater he ON he.he_id = mo.he_id\
                    WHERE mo.de_id = (SELECT de_id FROM an_device WHERE de_name='%q')\
                    AND datetime(mo.mo_date, 'unixepoch') >= datetime('%q', 'unixepoch') %s %s %s %s\
                    ORDER BY mo.mo_date ASC",
                    p_device, p_start_date, where_switch, where_sensor, where_dimmer, where_heater);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  sqlite3_free(where_switch);
  sqlite3_free(where_sensor);
  sqlite3_free(where_dimmer);
  sqlite3_free(where_heater);
  
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (get_monitor)");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    one_item = malloc(sizeof(char));
    strcpy(one_item, "");
    all_items = malloc(sizeof(char));
    strcpy(all_items, "");
    while (row_result == SQLITE_ROW) {
      snprintf(monitor_date, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 0));
      snprintf(monitor_value, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 1));
      str_len = snprintf(NULL, 0, json_template_tools_monitor_one_value, monitor_date, monitor_value);
      one_item = realloc(one_item, (str_len+1)*sizeof(char));
      snprintf(one_item, (str_len+1)*sizeof(char), json_template_tools_monitor_one_value, monitor_date, monitor_value);
      if (!first_result) {
        all_items = realloc(all_items, strlen(all_items)+2);
        strcat(all_items, ",");
      }
      all_items = realloc(all_items, strlen(all_items)+strlen(one_item)+1);
      strcat(all_items, one_item);
      first_result = 0;
      row_result = sqlite3_step(stmt);
    }
    t_len = snprintf(NULL, 0, json_template_tools_get_monitor, device_name, p_switch, p_sensor, p_dimmer, p_heater, p_start_date, all_items);
    to_return = malloc((t_len+1)*sizeof(char));
    snprintf(to_return, (t_len+1)*sizeof(char), json_template_tools_get_monitor, device_name, p_switch, p_sensor, p_dimmer, p_heater, p_start_date, all_items);
    free(one_item);
    free(all_items);
  }
  sqlite3_finalize(stmt);
  return to_return;
}

/**
 * Write the message given in parameters to the current outputs if the current level matches
 */
void log_message(unsigned long level, const char * message, ...) {
  va_list argp, args_cpy;
  size_t out_len = 0;
  char * out = NULL;
  va_start(argp, message);
  va_copy(args_cpy, argp);
  out_len = vsnprintf(NULL, 0, message, argp);
  out = malloc(out_len+sizeof(char));
  vsnprintf(out, (out_len+sizeof(char)), message, args_cpy);
  write_log(LOG_MODE_CURRENT, LOG_LEVEL_CURRENT, NULL, level, out);
  free(out);
  va_end(argp);
  va_end(args_cpy);
}

/**
 * Main function for logging messages
 * Warning ! Contains static variables used for not having to pass general configuration values every time you call log_message
 */
int write_log(unsigned long init_mode, unsigned long init_level, char * init_log_file, unsigned long level, const char * message) {
  static unsigned long cur_mode, cur_level;
  static FILE * cur_log_file;
  time_t now;
  
  time(&now);
  
  if (init_mode != LOG_MODE_CURRENT) {
    cur_mode = init_mode;
  }
  
  if (init_level != LOG_LEVEL_CURRENT) {
    cur_level = init_level;
  }

  if (init_log_file != NULL) {
    if ((cur_log_file = fopen(init_log_file, "a+")) == NULL) {
      perror("Error opening log file");
      return 0;
    }
  }
  
  // write message to expected output if level expected
  if (cur_level >= level) {
    if (message != NULL) {
      if (cur_mode & LOG_MODE_CONSOLE) {
        write_log_console(now, level, message);
      }
      if (cur_mode & LOG_MODE_SYSLOG) {
        write_log_syslog(level, message);
      }
      if (cur_mode & LOG_MODE_FILE) {
        write_log_file(now, cur_log_file, level, message);
      }
    }
  }
  
  return 1;
}

/**
 * Write log message to console output (stdout or stderr)
 */
void write_log_console(time_t date, unsigned long level, const char * message) {
  char * level_name = NULL, date_stamp[20];
  FILE * output = NULL;
  struct tm * tm_stamp;
  
  tm_stamp = localtime (&date);
  
  strftime (date_stamp, sizeof(date_stamp), "%Y-%m-%d %H:%M:%S", tm_stamp);
  switch (level) {
    case LOG_LEVEL_ERROR:
      level_name = "ERROR";
      break;
    case LOG_LEVEL_WARNING:
      level_name = "WARNING";
      break;
    case LOG_LEVEL_INFO:
      level_name = "INFO";
      break;
    case LOG_LEVEL_DEBUG:
      level_name = "DEBUG";
      break;
    default:
      level_name = "NONE";
      break;
  }
  if (level & LOG_LEVEL_WARNING) {
    // Write to stderr
    output = stderr;
  } else {
    // Write to stdout
    output = stdout;
  }
  fprintf(output, "%s - Angharad %s: %s\n", date_stamp, level_name, message);
  fflush(output);
}

/**
 * Write log message to syslog
 */
void write_log_syslog(unsigned long level, const char * message) {
  openlog("Angharad", LOG_PID|LOG_CONS, LOG_USER);
  switch (level) {
    case LOG_LEVEL_ERROR:
      syslog( LOG_ERR, "%s", message );
      break;
    case LOG_LEVEL_WARNING:
      syslog( LOG_WARNING, "%s", message );
      break;
    case LOG_LEVEL_INFO:
      syslog( LOG_INFO, "%s", message );
      break;
    case LOG_LEVEL_DEBUG:
      syslog( LOG_DEBUG, "%s", message );
      break;
  }
  closelog();
}

/**
 * Append log message to the log file
 */
void write_log_file(time_t date, FILE * log_file, unsigned long level, const char * message) {
  char * level_name = NULL, date_stamp[20];
  struct tm * tm_stamp;
  
  if (log_file != NULL) {
    tm_stamp = localtime (&date);
    strftime (date_stamp, sizeof(date_stamp), "%Y-%m-%d %H:%M:%S", tm_stamp);
    switch (level) {
      case LOG_LEVEL_ERROR:
        level_name = "ERROR";
        break;
      case LOG_LEVEL_WARNING:
        level_name = "WARNING";
        break;
      case LOG_LEVEL_INFO:
        level_name = "INFO";
        break;
      case LOG_LEVEL_DEBUG:
        level_name = "DEBUG";
        break;
      default:
        level_name = "NONE";
        break;
    }
    fprintf(log_file, "%s - Angharad %s: %s\n", date_stamp, level_name, message);
    fflush(log_file);
  }
}

/**
 * get the commands list on json format
 * it returns the content of the api_rest.json compiled as a binary file
 * It's not magic ! You MUST manually update the api_rest.json file if you add/remove/modify output commands
 */
char * get_json_list_commands() {
  return NULL;
  /*extern uint8_t binary_api_rest_json_size[] asm("_binary_api_rest_json_size");
  extern char _binary_api_rest_json_start;
  extern char _binary_api_rest_json_end;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  size_t json_list_len = (size_t)((void *)binary_api_rest_json_size);
  char * json_list_data = malloc(json_list_len+sizeof(char));
  char * json_list_p = &_binary_api_rest_json_start;
  int json_list_i = 0;
  while ( json_list_p != &_binary_api_rest_json_end ) {
    json_list_data[json_list_i++] = *json_list_p++;
  }
  json_list_data[json_list_i] = '\0';
  return json_list_data;*/
}
