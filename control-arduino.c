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
 * Commands send to one or multiple devices
 */

/**
 * Parse the result of a command OVERVIEW or REFRESH
 * get all the switcher values in a table, the the sensor values in another table, then merge the results into json
 * OVERVIEW format: {NAME:<name>;SWITCHES,<switchid1>:<status>,<switchid2>:<status>,...;SENSORS,<sensorid1>:<value>,<sensorid2>:<value>;HEATERS,<heaterid1>:<on>|<warming>|<temp>}
 */
char * parse_overview_arduino(sqlite3 * sqlite3_db, char * overview_result) {
  char *datas, *source, *saveptr, * overview_result_cpy = NULL, *data, *saveptr2, key[WORDLENGTH+1]={0}, value[WORDLENGTH+1]={0}, device[WORDLENGTH+1]={0}, tmp_value[WORDLENGTH+1]={0}, sanitized[WORDLENGTH+1]={0}, heater_value[WORDLENGTH+1]={0};
  char one_element[MSGLENGTH+1], * str_switches = NULL, * str_sensors = NULL, * str_heaters = NULL, * str_dimmers = NULL, * output = NULL, * tags = NULL, ** tags_array = NULL;
  int i;
  switcher * switches = NULL;
  sensor * sensors = NULL;
  heater * heaters = NULL;
  int nb_switches = 0, nb_sensors = 0, nb_heaters = 0;
  
  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, output_len;
  
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
      snprintf(device, WORDLENGTH, "%s", value);
      sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT de_id FROM an_device WHERE de_name='%q'", device);
      sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
      if (sql_result != SQLITE_OK) {
        log_message(LOG_INFO, "Error preparing sql query device fetch");
      } else {
        row_result = sqlite3_step(stmt);
        if (row_result != SQLITE_ROW) {
          sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_device (de_name, de_display, de_active) VALUES ('%q', '%q', 1)", device, device);
          if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
            log_message(LOG_INFO, "Error inserting an_device %s", sql_query);
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
        
        switches = realloc(switches, (nb_switches+1)*sizeof(struct _switcher));
        snprintf(switches[nb_switches].name, WORDLENGTH, "%s", key);
        switches[nb_switches].status = strtol(value, NULL, 10);
        
        // Default values
        switches[nb_switches].type = 0;
        switches[nb_switches].monitored = 0;
        switches[nb_switches].monitored_every = 0;
        switches[nb_switches].monitored_next = 0;
        sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_display, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next FROM an_switch WHERE sw_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", key, device);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_INFO, "Error preparing sql query switch fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            sanitize_json_string((char*)sqlite3_column_text(stmt, 0), switches[nb_switches].display, WORDLENGTH);
            switches[nb_switches].enabled = sqlite3_column_int(stmt, 1);
            switches[nb_switches].type = sqlite3_column_int(stmt, 2);
            switches[nb_switches].monitored = sqlite3_column_int(stmt, 3);
            switches[nb_switches].monitored_every = sqlite3_column_int(stmt, 4);
            switches[nb_switches].monitored_next = sqlite3_column_int(stmt, 5);
          } else {
            // No result, default value
            snprintf(switches[nb_switches].display, WORDLENGTH, "%s", switches[nb_switches].name);
            switches[nb_switches].enabled = 1;
            
            // Creating data in database
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_switch (de_id, sw_name, sw_display, sw_status, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next) VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%q', 1, 0, 0, 0, 0)", device, key, key, value);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_INFO, "Error inserting an_switch %s", sql_query);
            }
          }
        }
        sqlite3_finalize(stmt);
        nb_switches++;
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
        sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT se_display, se_unit, se_active, se_monitored, se_monitored_every, se_monitored_next from an_sensor where se_name='%q' and de_id in (select de_id from an_device where de_name='%q')", key, device);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_INFO, "Error preparing sql query sensor fetch");
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
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_sensor (de_id, se_name, se_display, se_active, se_unit, se_monitored, se_monitored_every, se_monitored_next) VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', 1, '', 0, 0, 0)", device, key, key);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_INFO, "Error inserting an_sensor %s", sql_query);
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
        parse_heater(sqlite3_db, device, key, heater_value, &heaters[nb_heaters]);
        nb_heaters++;
        data = strtok_r( NULL, ",", &saveptr2);
      }
    }
    datas = strtok_r( NULL, ";", &saveptr );
  }
    
  // Arranging the results
  str_switches = malloc(2*sizeof(char));
  strcpy(str_switches, "[");
  for (i=0; i<nb_switches; i++) {
    tags_array = get_tags(sqlite3_db, device, DATA_SWITCH, switches[i].name);
    tags = build_json_tags(tags_array);
    strcpy(one_element, "");
    if (i>0) {
      strncat(one_element, ",", MSGLENGTH);
    }
    strncat(one_element, "{\"name\":\"", MSGLENGTH);
    sanitize_json_string(switches[i].name, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"display\":\"", MSGLENGTH);
    sanitize_json_string(switches[i].display, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"status\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", switches[i].status);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"type\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", switches[i].type);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"enabled\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", switches[i].enabled?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", switches[i].monitored?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_every\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", switches[i].monitored_every);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_next\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%ld", switches[i].monitored_next);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"tags\":", MSGLENGTH);
    str_switches = realloc(str_switches, strlen(str_switches)+strlen(one_element)+strlen(tags)+2);
    strcat(str_switches, one_element);
    strcat(str_switches, tags);
    strcat(str_switches, "}");
    free(tags);
    free_tags(tags_array);
  }
  str_switches = realloc(str_switches, strlen(str_switches)+2);
  strcat(str_switches, "]");
  
  str_sensors = malloc(2*sizeof(char));
  strcpy(str_sensors, "[");
  for (i=0; i<nb_sensors; i++) {
    tags_array = get_tags(sqlite3_db, device, DATA_SENSOR, sensors[i].name);
    tags = build_json_tags(tags_array);
    strcpy(one_element, "");
    if (i>0) {
      strncat(one_element, ",", MSGLENGTH);
    }
    strncat(one_element, "{\"name\":\"", MSGLENGTH);
    sanitize_json_string(sensors[i].name, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"display\":\"", MSGLENGTH);
    sanitize_json_string(sensors[i].display, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"value\":", MSGLENGTH);
    strncat(one_element, sensors[i].value, MSGLENGTH);
    strncat(one_element, ",\"unit\":\"", MSGLENGTH);
    strncat(one_element, sensors[i].unit, MSGLENGTH);
    strncat(one_element, "\",\"enabled\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", sensors[i].enabled?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", sensors[i].monitored?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_every\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", sensors[i].monitored_every);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_next\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%ld", sensors[i].monitored_next);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"tags\":", MSGLENGTH);
    str_sensors = realloc(str_sensors, strlen(str_sensors)+strlen(one_element)+strlen(tags)+2);
    strcat(str_sensors, one_element);
    strcat(str_sensors, tags);
    strcat(str_sensors, "}");
    free(tags);
    free_tags(tags_array);
  }
  str_sensors = realloc(str_sensors, strlen(str_sensors)+2);
  strcat(str_sensors, "]");
  
  str_heaters = malloc(2*sizeof(char));
  strcpy(str_heaters, "[");
  for (i=0; i<nb_heaters; i++) {
    tags_array = get_tags(sqlite3_db, device, DATA_HEATER, heaters[i].name);
    tags = build_json_tags(tags_array);
    strcpy(one_element, "");
    sanitize_json_string(heaters[i].name, tmp_value, WORDLENGTH);
    snprintf(one_element, MSGLENGTH, "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\",\"tags\":", i>0?",":"", heaters[i].name, heaters[i].display, heaters[i].enabled?"true":"false", heaters[i].set?"true":"false", heaters[i].on?"true":"false", heaters[i].heat_max_value, heaters[i].unit);
    str_heaters = realloc(str_heaters, strlen(str_heaters)+strlen(one_element)+strlen(tags)+2);
    strcat(str_heaters, one_element);
    strcat(str_heaters, tags);
    strcat(str_heaters, "}");
    free(tags);
    free_tags(tags_array);
  }
  str_heaters = realloc(str_heaters, strlen(str_heaters)+2);
  strcat(str_heaters, "]");
  
  // TODO
  str_dimmers = malloc(3*sizeof(char));
  strcpy(str_dimmers, "[]");
  
  output_len = 59+strlen(device)+strlen(str_switches)+strlen(str_sensors)+strlen(str_heaters)+strlen(str_dimmers);
  output = malloc(output_len*sizeof(char));
  snprintf(output, output_len-1, "{\"name\":\"%s\",\"switches\":%s,\"sensors\":%s,\"heaters\":%s,\"dimmers\":%s}", device, str_switches, str_sensors, str_heaters, str_dimmers);
  
  // Free all allocated pointers before return
  free(switches);
  switches = NULL;
  free(sensors);
  sensors = NULL;
  free(heaters);
  heaters = NULL;
  free(str_switches);
  str_switches = NULL;
  free(str_sensors);
  str_sensors = NULL;
  free(str_heaters);
  str_heaters = NULL;
  free(str_dimmers);
  str_dimmers = NULL;
  free(overview_result_cpy);
  overview_result_cpy = NULL;
  return output;
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
