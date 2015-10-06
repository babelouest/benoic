/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * Arduino UNO devices calls
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
 * Parse the result of a command OVERVIEW or REFRESH
 * get all the switcher values in a table, the the sensor values in another table, then merge the results into json
 * OVERVIEW format: {NAME:<name>;SWITCHES,<switchid1>:<status>,<switchid2>:<status>,...;SENSORS,<sensorid1>:<value>,<sensorid2>:<value>;HEATERS,<heaterid1>:<on>|<warming>|<temp>}
 */
char * parse_overview_arduino(sqlite3 * sqlite3_db, char * overview_result) {
  char *datas, *source, *saveptr, * overview_result_cpy = NULL, *data, *saveptr2, key[WORDLENGTH+1]={0},
        value[WORDLENGTH+1]={0}, device_name[WORDLENGTH+1]={0}, heater_value[WORDLENGTH+1]={0};

  int i;
  switcher * switchers = NULL;
  sensor * sensors = NULL;
  heater * heaters = NULL;
  dimmer * dimmers = NULL;
  int nb_switchers = 0, nb_sensors = 0, nb_heaters = 0, nb_dimmers = 0;
  char * to_return = NULL;
  
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  overview_result_cpy = malloc(strlen(overview_result));
  snprintf(overview_result_cpy, strlen(overview_result)-1, "%s", overview_result+1);
  overview_result_cpy[strlen(overview_result_cpy) - 1] = '\0';
  source = overview_result_cpy;
  datas = strtok_r( source, ";", &saveptr );
  
  while (NULL != datas) {
    // Look for the data type
    if (0 == strncmp(datas, "NAME", strlen("NAME"))) {
      // Name parsing
      i = strcspn(datas, ":");
      memset(key, 0, WORDLENGTH*sizeof(char));
      memset(value, 0, WORDLENGTH*sizeof(char));
      strncpy(key, datas, i*sizeof(char));
      strncpy(value, datas+i+1, WORDLENGTH*sizeof(char));
      snprintf(device_name, WORDLENGTH*sizeof(char), "%s", value);
      sql_query = sqlite3_mprintf("SELECT de_id FROM an_device WHERE de_name='%q'", device_name);
      sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
      sqlite3_free(sql_query);
      if (sql_result != SQLITE_OK) {
        log_message(LOG_LEVEL_WARNING, "Error preparing sql query (parse_overview_arduino)");
      } else {
        row_result = sqlite3_step(stmt);
        if (row_result != SQLITE_ROW) {
          sql_query = sqlite3_mprintf("INSERT INTO an_device (de_name, de_display, de_active) VALUES ('%q', '%q', 1)", device_name, device_name);
          if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
            log_message(LOG_LEVEL_WARNING, "Error inserting Device");
          }
          sqlite3_free(sql_query);
        }
      }
      sqlite3_finalize(stmt);
      
    } else if (0 == strncmp(datas, "SWITCHES", strlen("SWITCHES"))) {
      data = strtok_r( datas, ",", &saveptr2); // SWITCHES title
      data = strtok_r( NULL, ",", &saveptr2); // First occurence
      while (NULL != data) {
        // parsing data
        i = strcspn(data, ":");
        memset(key, 0, WORDLENGTH*sizeof(char));
        memset(value, 0, WORDLENGTH*sizeof(char));
        strncpy(key, data, i*sizeof(char));
        strncpy(value, data+i+1, WORDLENGTH*sizeof(char));
        
        switchers = realloc(switchers, (nb_switchers+1)*sizeof(struct _switcher));
        snprintf(switchers[nb_switchers].name, WORDLENGTH*sizeof(char), "%s", key);
        switchers[nb_switchers].status = strtol(value, NULL, 10);
        
        // Default values
        switchers[nb_switchers].type = 0;
        switchers[nb_switchers].monitored = 0;
        switchers[nb_switchers].monitored_every = 0;
        switchers[nb_switchers].monitored_next = 0;
        sql_query = sqlite3_mprintf("SELECT sw_display, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next\
                          FROM an_switch WHERE sw_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", key, device_name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        sqlite3_free(sql_query);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_LEVEL_WARNING, "Error preparing sql query switch fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            strncpy(switchers[nb_switchers].display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
            switchers[nb_switchers].enabled = sqlite3_column_int(stmt, 1);
            switchers[nb_switchers].type = sqlite3_column_int(stmt, 2);
            switchers[nb_switchers].monitored = sqlite3_column_int(stmt, 3);
            switchers[nb_switchers].monitored_every = sqlite3_column_int(stmt, 4);
            switchers[nb_switchers].monitored_next = sqlite3_column_int(stmt, 5);
          } else {
            // No result, default value
            snprintf(switchers[nb_switchers].display, WORDLENGTH*sizeof(char), "%s", switchers[nb_switchers].name);
            switchers[nb_switchers].enabled = 1;
            
            // Creating data in database
            sql_query = sqlite3_mprintf("INSERT INTO an_switch\
                             (de_id, sw_name, sw_display, sw_status, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next)\
                             VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%q', 1, 0, 0, 0, 0)",
                             device_name, key, key, value);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_LEVEL_WARNING, "Error inserting an_switch");
            }
            sqlite3_free(sql_query);
          }
        }
        sqlite3_finalize(stmt);
        nb_switchers++;
        data = strtok_r( NULL, ",", &saveptr2);
      }
    } else if (0 == strncmp(datas, "DIMMERS", strlen("DIMMERS"))) {
      data = strtok_r( datas, ",", &saveptr2); // DIMMERS title
      data = strtok_r( NULL, ",", &saveptr2); // First occurence
      while (NULL != data) {
        // parsing data
        i = strcspn(data, ":");
        memset(key, 0, WORDLENGTH*sizeof(char));
        memset(value, 0, WORDLENGTH*sizeof(char));
        strncpy(key, data, i*sizeof(char));
        strncpy(value, data+i+1, WORDLENGTH*sizeof(char));
        
        dimmers = realloc(dimmers, (nb_dimmers+1)*sizeof(struct _switcher));
        snprintf(dimmers[nb_dimmers].name, WORDLENGTH*sizeof(char), "%s", key);
        dimmers[nb_dimmers].value = strtol(value, NULL, 10);
        
        // Default values
        dimmers[nb_dimmers].monitored = 0;
        dimmers[nb_dimmers].monitored_every = 0;
        dimmers[nb_dimmers].monitored_next = 0;
        sql_query = sqlite3_mprintf("SELECT di_display, di_active, di_monitored, di_monitored_every, di_monitored_next FROM an_dimmer\
          WHERE di_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", 
          dimmers[nb_dimmers].name,
          device_name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        sqlite3_free(sql_query);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_LEVEL_WARNING, "Error preparing sql query dimmer fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            strncpy(dimmers[nb_dimmers].display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
            dimmers[nb_dimmers].enabled = sqlite3_column_int(stmt, 1);
            dimmers[nb_dimmers].monitored = sqlite3_column_int(stmt, 2);
            dimmers[nb_dimmers].monitored_every = sqlite3_column_int(stmt, 3);
            dimmers[nb_dimmers].monitored_next = sqlite3_column_int(stmt, 4);
          } else {
            // No result, default value
            snprintf(dimmers[nb_dimmers].display, WORDLENGTH*sizeof(char), "%s", dimmers[nb_dimmers].name);
            dimmers[nb_dimmers].enabled = 1;
            
            // Creating data in database
            sql_query = sqlite3_mprintf("INSERT INTO an_dimmer\
                              (de_id, di_name, di_display, di_value, di_monitored, di_monitored_every, di_monitored_next)\
                              VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d', 0, 0, 0)",
                              device_name, dimmers[nb_dimmers].name, dimmers[nb_dimmers].name, dimmers[nb_dimmers].value);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_LEVEL_WARNING, "Error inserting an_dimmer");
            }
            sqlite3_free(sql_query);
          }
        }
        sqlite3_finalize(stmt);
        nb_dimmers++;
        data = strtok_r( NULL, ",", &saveptr2);
      }
    } else if (0 == strncmp(datas, "SENSORS", strlen("SENSORS"))) {
      data = strtok_r( datas, ",", &saveptr2); // SENSORS title
      data = strtok_r( NULL, ",", &saveptr2); // First occurence
      while (NULL != data) {
        // parsing data
        i = strcspn(data, ":");
        memset(key, 0, WORDLENGTH*sizeof(char));
        memset(value, 0, WORDLENGTH*sizeof(char));
        strncpy(key, data, i*sizeof(char));
        strncpy(value, data+i+1, WORDLENGTH*sizeof(char));

        sensors = realloc(sensors, (nb_sensors+1)*sizeof(struct _sensor));
        snprintf(sensors[nb_sensors].name, WORDLENGTH*sizeof(char), "%s", key);
        snprintf(sensors[nb_sensors].value, WORDLENGTH*sizeof(char), "%s", value);
        snprintf(sensors[nb_sensors].display, WORDLENGTH*sizeof(char), "%s", sensors[nb_sensors].name);
        strcpy(sensors[nb_sensors].unit, "");
        sensors[nb_sensors].value_type = VALUE_TYPE_NONE;
        sensors[nb_sensors].enabled = 1;
        sensors[nb_sensors].monitored = 0;
        sensors[nb_sensors].monitored_every = 0;
        sensors[nb_sensors].monitored_next = 0;
        sql_query = sqlite3_mprintf("SELECT se_display, se_unit, se_value_type, se_active, se_monitored, se_monitored_every, se_monitored_next\
                        FROM an_sensor WHERE se_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", key, device_name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        sqlite3_free(sql_query);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_LEVEL_WARNING, "Error preparing sql query sensor fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            strncpy(sensors[nb_sensors].display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
            strncpy(sensors[nb_sensors].unit, (char*)sqlite3_column_text(stmt, 1), WORDLENGTH);
            sensors[nb_sensors].value_type = sqlite3_column_int(stmt, 2);
            sensors[nb_sensors].enabled = sqlite3_column_int(stmt, 3);
            sensors[nb_sensors].monitored = sqlite3_column_int(stmt, 4);
            sensors[nb_sensors].monitored_every = sqlite3_column_int(stmt, 5);
            sensors[nb_sensors].monitored_next = sqlite3_column_int(stmt, 6);
          } else {
            // Creating data in database
            sql_query = sqlite3_mprintf("INSERT INTO an_sensor\
                              (de_id, se_name, se_display, se_active, se_unit, se_monitored, se_monitored_every, se_monitored_next)\
                              VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', 1, '', 0, 0, 0)",
                              device_name, key, key);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_LEVEL_WARNING, "Error inserting an_sensor %s", sql_query);
            }
            sqlite3_free(sql_query);
          }
        }
        sqlite3_finalize(stmt);
        nb_sensors++;
        data = strtok_r( NULL, ",", &saveptr2);
      }
    } else if (0 == strncmp(datas, "HEATERS", strlen("HEATERS"))) {
      data = strtok_r( datas, ",", &saveptr2); // HEATERS title
      data = strtok_r( NULL, ",", &saveptr2); // First occurence
      while (NULL != data) {
        // parsing data
        i = strcspn(data, ":");
        memset(key, 0, WORDLENGTH*sizeof(char));
        memset(value, 0, WORDLENGTH*sizeof(char));
        strncpy(key, data, i);
        strncpy(value, data+i+1, WORDLENGTH*sizeof(char));
        
        heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
        snprintf(heater_value, WORDLENGTH*sizeof(char), "%s", value);
        parse_heater_arduino(sqlite3_db, device_name, key, heater_value, &heaters[nb_heaters]);
        nb_heaters++;
        data = strtok_r( NULL, ",", &saveptr2);
      }
    }
    datas = strtok_r( NULL, ";", &saveptr );
  }
  
  to_return = build_overview_output(sqlite3_db, device_name, switchers, nb_switchers, sensors, nb_sensors, heaters, nb_heaters, dimmers, nb_dimmers);
  free(switchers);
  free(sensors);
  free(dimmers);
  free(heaters);
  free(overview_result_cpy);
  return to_return;
}

/**
 * Tell if the terminal is connected
 */
int is_connected_arduino(device * terminal) {
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (!(terminal != NULL && terminal->enabled && ((struct _arduino_device *) terminal->element)->serial_fd != -1)) {
    if (terminal!=NULL) {
      terminal->enabled=0;
    }
    return 0;
  } else {
    return 1;
  }
}

/**
 * Connect the device
 */
int connect_device_arduino(device * terminal, device ** terminals, unsigned int nb_terminal) {
  int i=0;
  char filename[WORDLENGTH+1] = {0};
  char cur_name[WORDLENGTH+1] = {0};
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (terminal == NULL) {
    return -1;
  } else {
    for (i=0; i<128; i++) {
      snprintf(filename, WORDLENGTH*sizeof(char), "%s%d", terminal->uri, i);
      if (!is_file_opened_arduino(filename, terminals, nb_terminal) && access(filename, F_OK) != -1) {
        ((struct _arduino_device *) terminal->element)->serial_fd = serialport_init(filename, ((struct _arduino_device *) terminal->element)->serial_baud);
        if (((struct _arduino_device *) terminal->element)->serial_fd != -1) {
          serialport_flush(((struct _arduino_device *) terminal->element)->serial_fd);
          get_name_arduino(terminal, cur_name);
          if (0 == strncmp(cur_name, terminal->name, WORDLENGTH)) {
            terminal->enabled=1;
            snprintf(((struct _arduino_device *) terminal->element)->serial_file, WORDLENGTH*sizeof(char), "%s", filename);
            return ((struct _arduino_device *) terminal->element)->serial_fd;
          } else {
            close_device(terminal);
            terminal->enabled=0;
          }
        }
      }
    }
  }
  return -1;
}

/**
 * Check if the file is already opened for a device
 */
int is_file_opened_arduino(char * serial_file, device ** terminal, unsigned int nb_terminal) {
  int i;
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  for (i=0; i<nb_terminal; i++) {
    if (terminal[i] != NULL && 0 == strncmp(serial_file, ((struct _arduino_device *) terminal[i])->serial_file, WORDLENGTH)) {
      return 1;
    }
  }
  return 0;
}

/**
 * Reconnect the device if it was disconnected for example
 */
int reconnect_device_arduino(device * terminal, device ** terminals, unsigned int nb_terminal) {
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (terminal == NULL) {
    return -1;
  } else {
    if (((struct _arduino_device *) terminal->element)->serial_file != NULL) {
      ((struct _arduino_device *) terminal->element)->serial_fd = serialport_init(((struct _arduino_device *) terminal->element)->serial_file, ((struct _arduino_device *) terminal->element)->serial_baud);
    }
    if (((struct _arduino_device *) terminal->element)->serial_fd == -1) {
      strcpy(((struct _arduino_device *) terminal->element)->serial_file, "");
      return connect_device_arduino(terminal, terminals, nb_terminal);
    } else {
      terminal->enabled=1;
      return ((struct _arduino_device *) terminal->element)->serial_fd;
    }
  }
}

/**
 * Close the connection to the device
 */
int close_device_arduino(device * terminal) {
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (terminal == NULL) {
    return 0;
  } else {
    terminal->enabled=0;
    return serialport_close(((struct _arduino_device *) terminal->element)->serial_fd);
  }
}

/**
 * Changes the state of a switcher on the designated terminal
 */
int set_switch_state_arduino(device * terminal, char * switcher, int status) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result = ERROR_SWITCH;
  char * read_cpy, * end_ptr;

  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "SETSWITCH,%s,%d\n", switcher, status);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1));
    strcpy(read_cpy, serial_read+1);
    result = strtol(read_cpy, &end_ptr, 10);
    if (read_cpy == end_ptr) {
      result = ERROR_SWITCH;
    }
    free(read_cpy);
  }
  //serialport_flush(terminal->serial_fd);
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Toggle the switcher state
 */
int toggle_switch_state_arduino(device * terminal, char * switcher) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=ERROR_SWITCH;
  char * read_cpy, * end_ptr;

  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "TOGGLESWITCH,%s\n", switcher);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc(strlen(serial_read)+1);
    strcpy(read_cpy, serial_read+1);
    result = strtol(read_cpy, &end_ptr, 10);
    if (read_cpy == end_ptr) {
      result = ERROR_SWITCH;
    }
    free(read_cpy);
  }
  //serialport_flush(terminal->serial_fd);
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Get the state of a switcher on the designated terminal
 */
int get_switch_state_arduino(device * terminal, char * switcher, int force) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=ERROR_SWITCH;
  char * read_cpy, * end_ptr;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  if (force) {
    snprintf(serial_command, WORDLENGTH*sizeof(char), "GETSWITCH,%s,1\n", switcher);
  } else {
    snprintf(serial_command, WORDLENGTH*sizeof(char), "GETSWITCH,%s\n", switcher);
  }
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1));
    strcpy(read_cpy, serial_read+1);
    result = strtol(read_cpy, &end_ptr, 10);
    if (read_cpy == end_ptr) {
      result = ERROR_SWITCH;
    }
    free(read_cpy);
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Get the value of the designated sensor on the designated terminal
 */
float get_sensor_value_arduino(device * terminal, char * sensor, int force) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = 5000;
  float result = ERROR_SENSOR;
  char * read_cpy, * end_ptr;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  if (force) {
    snprintf(serial_command, WORDLENGTH*sizeof(char), "SENSOR,%s,1\n", sensor);
  } else {
    snprintf(serial_command, WORDLENGTH*sizeof(char), "SENSOR,%s\n", sensor);
  }
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1));
    strcpy(read_cpy, serial_read+1);
    result = strtof(read_cpy, &end_ptr);
    if (read_cpy == end_ptr) {
      result = ERROR_SENSOR;
    }
    free(read_cpy);
  }
  if (force) {
    serialport_flush(((struct _arduino_device *) terminal->element)->serial_fd);
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Send a heartbeat command to tell if the terminal is still responding
 */
int send_heartbeat_arduino(device * terminal) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result = 0;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (!terminal->enabled) {
    return 0;
  }
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  
  snprintf(serial_command, WORDLENGTH*sizeof(char), "MARCO\n");
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    if (0 == strcmp("{POLO}", serial_read)) {
      result = 1;
    } else {
      result = 0;
    }
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Get the overview of all switches and sensors for the device
 */
char * get_overview_arduino(sqlite3 * sqlite3_db, device * terminal) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  char output[MSGLENGTH+1] = {0};
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return NULL;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "OVERVIEW\n");
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, output, eolchar, MSGLENGTH, timeout);
  }
  pthread_mutex_unlock(&terminal->lock);
  return parse_overview_arduino(sqlite3_db, output);
}

/**
 * Send a REFRESH command to the selected terminal and parse the result into the output var
 */
char * get_refresh_arduino(sqlite3 * sqlite3_db, device * terminal) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  char output[MSGLENGTH+1];
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return NULL;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "REFRESH\n");
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, output, eolchar, MSGLENGTH, timeout);
  }
  pthread_mutex_unlock(&terminal->lock);
  return parse_overview_arduino(sqlite3_db, output);
}

/**
 * Get the name of the device
 */
int get_name_arduino(device * terminal, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  char buffer[WORDLENGTH+1];
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return 0;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "NAME\n");
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, buffer, eolchar, WORDLENGTH, timeout);
    if (output != NULL) {
      // Remove first and last character and copy to output
      strncpy(output, buffer+1, WORDLENGTH);
      output[strlen(output) - 1] = '\0';
    }
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Get the Heater current status
 */
heater * get_heater_arduino(sqlite3 * sqlite3_db, device * terminal, char * heat_id) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  heater * cur_heater = malloc(sizeof(heater));
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return 0;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "GETHEATER,%s\n", heat_id);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    if (parse_heater_arduino(sqlite3_db, terminal->name, heat_id, serial_read+1, cur_heater)) {
      pthread_mutex_unlock(&terminal->lock);
      return cur_heater;
    } else {
      free(cur_heater);
      pthread_mutex_unlock(&terminal->lock);
      return NULL;
    }
  }
  free(cur_heater);
  pthread_mutex_unlock(&terminal->lock);
  return NULL;
}

/**
 * Set the Heater current status
 */
heater * set_heater_arduino(sqlite3 * sqlite3_db, device * terminal, char * heat_id, int heat_enabled, float max_heat_value) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  heater * cur_heater = malloc(sizeof(heater));
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return 0;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "SETHEATER,%s,%d,%.2f\n", heat_id, heat_enabled, max_heat_value);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    if (parse_heater_arduino(sqlite3_db, terminal->name, heat_id, serial_read+1, cur_heater)) {
      pthread_mutex_unlock(&terminal->lock);
      return cur_heater;
    } else {
      free(cur_heater);
      pthread_mutex_unlock(&terminal->lock);
      return NULL;
    }
  }
  free(cur_heater);
  pthread_mutex_unlock(&terminal->lock);
  return NULL;
}

/**
 * Get the dimmer value
 * I didn't make a dimmer controllable via an Arduino so I made a fony one for the development of the server
 * If you have a schema, I would love to try
 */
int get_dimmer_value_arduino(device * terminal, char * dimmer){
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=ERROR_DIMMER;
  char * read_cpy, * end_ptr;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "GETDIMMER,%s\n", dimmer);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1));
    strcpy(read_cpy, serial_read+1);
    result = strtol(read_cpy, &end_ptr, 10);
    if (read_cpy == end_ptr) {
      result = ERROR_SWITCH;
    }
    free(read_cpy);
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Set the dimmer value
 * I didn't make a dimmer controllable via an Arduino so I made a fony one for the development of the server
 * If you have a schema, I would love to try
 */
int set_dimmer_value_arduino(device * terminal, char * dimmer, int value) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result = ERROR_DIMMER;
  char * read_cpy, * end_ptr;

  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  snprintf(serial_command, WORDLENGTH*sizeof(char), "SETDIMMER,%s,%d\n", dimmer, value);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1));
    strcpy(read_cpy, serial_read+1);
    result = strtol(read_cpy, &end_ptr, 10);
    if (read_cpy == end_ptr) {
      result = ERROR_SWITCH;
    }
    free(read_cpy);
  }
  //serialport_flush(terminal->serial_fd);
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Parse the get heater results
 */
int parse_heater_arduino(sqlite3 * sqlite3_db, char * device, char * heater_name, char * source, heater * cur_heater) {
  char * heat_set, * heat_on, * heat_max_value, * saveptr;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char * sql_query = NULL;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  heat_set = strtok_r(source, "|", &saveptr);
  heat_on = strtok_r(NULL, "|", &saveptr);
  heat_max_value = strtok_r(NULL, "|", &saveptr);
  if (heat_set == NULL || heat_on == NULL || heat_max_value == NULL || cur_heater == NULL) {
    log_message(LOG_LEVEL_WARNING, "Error parsing heater data");
    return 0;
  } else {
    strncpy(cur_heater->name, heater_name, WORDLENGTH);
    strncpy(cur_heater->display, heater_name, WORDLENGTH);
    strncpy(cur_heater->device, device, WORDLENGTH);
    cur_heater->set = strcmp(heat_set,"1")==0?1:0;
    cur_heater->on = strcmp(heat_on,"1")==0?1:0;
    cur_heater->heat_max_value = strtof(heat_max_value, NULL);
    cur_heater->value_type = VALUE_TYPE_NONE;
    strcpy(cur_heater->unit, "");
    cur_heater->monitored = 0;
    cur_heater->monitored_every = 0;
    cur_heater->monitored_next = 0;
    
    sql_query = sqlite3_mprintf("SELECT he_display, he_enabled, he_unit, he_value_type, he_monitored, he_monitored_every, he_monitored_next FROM an_heater WHERE he_name='%q'\
                      and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", heater_name, device);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_LEVEL_WARNING, "Error preparing sql query (parse_heater_arduino)");
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        if (sqlite3_column_text(stmt, 0) != NULL) {
          strncpy(cur_heater->display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
        }
        cur_heater->enabled = sqlite3_column_int(stmt, 1);
        if (sqlite3_column_text(stmt, 2) != NULL) {
          strncpy(cur_heater->unit, (char*)sqlite3_column_text(stmt, 2), WORDLENGTH);
        }
        cur_heater->value_type = sqlite3_column_int(stmt, 3);
        cur_heater->monitored = sqlite3_column_int(stmt, 4);
        cur_heater->monitored_every = sqlite3_column_int(stmt, 5);
        cur_heater->monitored_next = sqlite3_column_int(stmt, 6);
        
      } else {
        snprintf(cur_heater->display, WORDLENGTH*sizeof(char), "%s", heater_name);
        cur_heater->enabled = 1;
        strcpy(cur_heater->unit, "");
      }
    }
    sqlite3_finalize(stmt);
    if (cur_heater->value_type == VALUE_TYPE_FAHRENHEIT) {
      cur_heater->heat_max_value = fahrenheit_to_celsius(cur_heater->heat_max_value);
    }
    return 1;
  }
}
