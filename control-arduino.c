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
 * Arduino UNO devices calls
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
  int nb_switchers = 0, nb_sensors = 0, nb_heaters = 0;
  
  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  overview_result_cpy = malloc(strlen(overview_result)*sizeof(char));
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
      strncpy(key, datas, i);
      strncpy(value, datas+i+1, WORDLENGTH);
      snprintf(device_name, WORDLENGTH, "%s", value);
      sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT de_id FROM an_device WHERE de_name='%q'", device_name);
      sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
      if (sql_result != SQLITE_OK) {
        log_message(LOG_WARNING, "Error preparing sql query (parse_overview_arduino)");
      } else {
        row_result = sqlite3_step(stmt);
        if (row_result != SQLITE_ROW) {
          sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_device (de_name, de_display, de_active) VALUES ('%q', '%q', 1)", device_name, device_name);
          if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
            log_message(LOG_WARNING, "Error inserting an_device %s", sql_query);
          }
        }
      }
      
    } else if (0 == strncmp(datas, "SWITCHES", strlen("SWITCHES"))) {
      data = strtok_r( datas, ",", &saveptr2); // SWITCHES title
      data = strtok_r( NULL, ",", &saveptr2); // First occurence
      while (NULL != data) {
        // parsing data
        i = strcspn(data, ":");
        memset(key, 0, WORDLENGTH*sizeof(char));
        memset(value, 0, WORDLENGTH*sizeof(char));
        strncpy(key, data, i);
        strncpy(value, data+i+1, WORDLENGTH);
        
        switchers = realloc(switchers, (nb_switchers+1)*sizeof(struct _switcher));
        snprintf(switchers[nb_switchers].name, WORDLENGTH, "%s", key);
        switchers[nb_switchers].status = strtol(value, NULL, 10);
        
        // Default values
        switchers[nb_switchers].type = 0;
        switchers[nb_switchers].monitored = 0;
        switchers[nb_switchers].monitored_every = 0;
        switchers[nb_switchers].monitored_next = 0;
        sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_display, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next\
                          FROM an_switch WHERE sw_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", key, device_name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_WARNING, "Error preparing sql query switch fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            sanitize_json_string((char*)sqlite3_column_text(stmt, 0), switchers[nb_switchers].display, WORDLENGTH);
            switchers[nb_switchers].enabled = sqlite3_column_int(stmt, 1);
            switchers[nb_switchers].type = sqlite3_column_int(stmt, 2);
            switchers[nb_switchers].monitored = sqlite3_column_int(stmt, 3);
            switchers[nb_switchers].monitored_every = sqlite3_column_int(stmt, 4);
            switchers[nb_switchers].monitored_next = sqlite3_column_int(stmt, 5);
          } else {
            // No result, default value
            snprintf(switchers[nb_switchers].display, WORDLENGTH, "%s", switchers[nb_switchers].name);
            switchers[nb_switchers].enabled = 1;
            
            // Creating data in database
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_switch\
                             (de_id, sw_name, sw_display, sw_status, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next)\
                             VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%q', 1, 0, 0, 0, 0)",
                             device_name, key, key, value);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_WARNING, "Error inserting an_switch %s", sql_query);
            }
          }
        }
        sqlite3_finalize(stmt);
        nb_switchers++;
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
        strncpy(key, data, i);
        strncpy(value, data+i+1, WORDLENGTH);

        sensors = realloc(sensors, (nb_sensors+1)*sizeof(struct _sensor));
        snprintf(sensors[nb_sensors].name, WORDLENGTH, "%s", key);
        snprintf(sensors[nb_sensors].value, WORDLENGTH, "%s", value);
        snprintf(sensors[nb_sensors].display, WORDLENGTH, "%s", sensors[nb_sensors].name);
        strcpy(sensors[nb_sensors].unit, "");
        sensors[nb_sensors].enabled = 1;
        sensors[nb_sensors].monitored = 0;
        sensors[nb_sensors].monitored_every = 0;
        sensors[nb_sensors].monitored_next = 0;
        sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT se_display, se_unit, se_active, se_monitored, se_monitored_every, se_monitored_next\
                        FROM an_sensor WHERE se_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", key, device_name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_WARNING, "Error preparing sql query sensor fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            sanitize_json_string((char*)sqlite3_column_text(stmt, 0), sensors[nb_sensors].display, WORDLENGTH);
            sanitize_json_string((char*)sqlite3_column_text(stmt, 1), sensors[nb_sensors].unit, WORDLENGTH);
            sensors[nb_sensors].enabled = sqlite3_column_int(stmt, 2);
            sensors[nb_sensors].monitored = sqlite3_column_int(stmt, 3);
            sensors[nb_sensors].monitored_every = sqlite3_column_int(stmt, 4);
            sensors[nb_sensors].monitored_next = sqlite3_column_int(stmt, 5);
          } else {
            // Creating data in database
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_sensor\
                              (de_id, se_name, se_display, se_active, se_unit, se_monitored, se_monitored_every, se_monitored_next)\
                              VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', 1, '', 0, 0, 0)",
                              device_name, key, key);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_WARNING, "Error inserting an_sensor %s", sql_query);
            }
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
        strncpy(value, data+i+1, WORDLENGTH);
        
        heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
        snprintf(heater_value, WORDLENGTH, "%s", value);
        parse_heater(sqlite3_db, device_name, key, heater_value, &heaters[nb_heaters]);
        nb_heaters++;
        data = strtok_r( NULL, ",", &saveptr2);
      }
    }
    datas = strtok_r( NULL, ";", &saveptr );
  }
    
  return build_overview_output(sqlite3_db, device_name, switchers, nb_switchers, sensors, nb_sensors, heaters, nb_heaters, NULL, 0);
}

/**
 * Tell if the terminal is connected
 */
int is_connected_arduino(device * terminal) {
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
  
  if (terminal == NULL) {
    return -1;
  } else {
    for (i=0; i<128; i++) {
      snprintf(filename, WORDLENGTH, "%s%d", terminal->uri, i);
      if (!is_file_opened_arduino(filename, terminals, nb_terminal) && access(filename, F_OK) != -1) {
        ((struct _arduino_device *) terminal->element)->serial_fd = serialport_init(filename, ((struct _arduino_device *) terminal->element)->serial_baud);
        if (((struct _arduino_device *) terminal->element)->serial_fd != -1) {
          serialport_flush(((struct _arduino_device *) terminal->element)->serial_fd);
          get_name_arduino(terminal, cur_name);
          cur_name[strlen(cur_name) - 1] = '\0';
          if (0 == strncmp(cur_name+1, terminal->name, WORDLENGTH)) {
            terminal->enabled=1;
            snprintf(((struct _arduino_device *) terminal->element)->serial_file, WORDLENGTH, "%s", filename);
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
  int result=ERROR_SWITCH;
  char * read_cpy, * end_ptr;

  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  snprintf(serial_command, WORDLENGTH, "SETSWITCH,%s,%d\n", switcher, status);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1)*sizeof(char));
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

    if (pthread_mutex_lock(&terminal->lock)) {
      return result;
    }
    snprintf(serial_command, WORDLENGTH, "TOGGLESWITCH,%s\n", switcher);
    serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
    if (serial_result != -1) {
      serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
      serial_read[strlen(serial_read) - 1] = '\0';
      read_cpy = malloc((strlen(serial_read)+1)*sizeof(char));
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
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  if (force) {
    snprintf(serial_command, WORDLENGTH, "GETSWITCH,%s,1\n", switcher);
  } else {
    snprintf(serial_command, WORDLENGTH, "GETSWITCH,%s\n", switcher);
  }
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1)*sizeof(char));
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
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  if (force) {
    snprintf(serial_command, WORDLENGTH, "SENSOR,%s,1\n", sensor);
  } else {
    snprintf(serial_command, WORDLENGTH, "SENSOR,%s\n", sensor);
  }
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    read_cpy = malloc((strlen(serial_read)+1)*sizeof(char));
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
  
  if (!terminal->enabled) {
    return 0;
  }
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  
  snprintf(serial_command, WORDLENGTH, "MARCO\n");
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
  char output[MSGLENGTH+1];
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return NULL;
  }
  snprintf(serial_command, WORDLENGTH, "OVERVIEW\n");
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
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return NULL;
  }
  snprintf(serial_command, WORDLENGTH, "REFRESH\n");
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
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return 0;
  }
  snprintf(serial_command, WORDLENGTH, "NAME\n");
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, output, eolchar, WORDLENGTH, timeout);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Get the Heater current status
 */
int get_heater_arduino(device * terminal, char * heat_id, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return 0;
  }
  snprintf(serial_command, WORDLENGTH, "GETHEATER,%s\n", heat_id);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    snprintf(output, WORDLENGTH, "%s", serial_read+1);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Set the Heater current status
 */
int set_heater_arduino(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return 0;
  }
  snprintf(serial_command, WORDLENGTH, "SETHEATER,%s,%d,%.2f\n", heat_id, heat_enabled, max_heat_value);
  serial_result = serialport_write(((struct _arduino_device *) terminal->element)->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(((struct _arduino_device *) terminal->element)->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    snprintf(output, WORDLENGTH, "%s", serial_read+1);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Not implemented yet
 * Because I didn't make a dimmer controllable via an Arduino
 * If you have a schema, I would love to try
 */
int get_dimmer_value_arduino(device * terminal, char * dimmer){
  return 0.0;
}

/**
 * Not implemented yet
 * Because I didn't make a dimmer controllable via an Arduino
 * If you have a schema, I would love to try
 */
int set_dimmer_value_arduino(device * terminal, char * dimmer, int value) {
  return 0;
}

/**
 * Parse the get heater results
 */
int parse_heater(sqlite3 * sqlite3_db, char * device, char * heater_name, char * source, heater * cur_heater) {
  char * heatSet, * heatOn, * heatMaxValue, * saveptr;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char sql_query[MSGLENGTH+1];
  
  heatSet = strtok_r(source, "|", &saveptr);
  heatOn = strtok_r(NULL, "|", &saveptr);
  heatMaxValue = strtok_r(NULL, "|", &saveptr);
  if (heatSet == NULL || heatOn == NULL || heatMaxValue == NULL) {
    log_message(LOG_WARNING, "Error parsing heater data");
    return 0;
  } else {
    sanitize_json_string(heater_name, cur_heater->name, WORDLENGTH);
    sanitize_json_string(device, cur_heater->device, WORDLENGTH);
    cur_heater->set = strcmp(heatSet,"1")==0?1:0;
    cur_heater->on = strcmp(heatOn,"1")==0?1:0;
    cur_heater->heat_max_value = strtof(heatMaxValue, NULL);
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT he_display, he_enabled, he_unit FROM an_heater WHERE he_name='%q'\
                      and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", heater_name, device);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query (parse_heater)");
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        if (sqlite3_column_text(stmt, 0) != NULL) {
          sanitize_json_string((char*)sqlite3_column_text(stmt, 0), cur_heater->display, WORDLENGTH);
        } else {
          snprintf(cur_heater->display, WORDLENGTH, "%s", heater_name);
        }
        cur_heater->enabled = sqlite3_column_int(stmt, 1);
        if (sqlite3_column_text(stmt, 2) != NULL) {
          sanitize_json_string((char*)sqlite3_column_text(stmt, 2), cur_heater->unit, WORDLENGTH);
        } else {
          strcpy(cur_heater->unit, "");
        }
      } else {
        snprintf(cur_heater->display, WORDLENGTH, "%s", heater_name);
        cur_heater->enabled = 1;
        strcpy(cur_heater->unit, "");
      }
    }
    sqlite3_finalize(stmt);
    return 1;
  }
}
