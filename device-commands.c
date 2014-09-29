#include "angharad.h"

/**
 * Commands send to one or multiple devices
 */

/**
 * Tell if the terminal is connected
 */
int is_connected(device * terminal) {
  if (!(terminal != NULL && terminal->enabled && terminal->serial_fd != -1)) {
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
int connect_device(device * terminal) {
  int i=0;
  char filename[WORDLENGTH+1] = {0};
  char cur_name[WORDLENGTH+1] = {0};
  
  if (terminal == NULL) {
    return -1;
  } else {
    if (terminal->type == TYPE_SERIAL) {
      for (i=0; i<128; i++) {
        snprintf(filename, WORDLENGTH, "%s%d", terminal->uri, i);
        if (access(filename, F_OK) != -1) {
          terminal->serial_fd = serialport_init(filename, terminal->serial_baud);
          if (terminal->serial_fd != -1) {
            serialport_flush(terminal->serial_fd);
            get_name(terminal, cur_name);
            cur_name[strlen(cur_name) - 1] = '\0';
            if (0 == strncmp(cur_name+1, terminal->name, WORDLENGTH)) {
              terminal->enabled=1;
              snprintf(terminal->serial_file, WORDLENGTH, "%s", filename);
              return terminal->serial_fd;
            } else {
              close_device(terminal);
              terminal->enabled=0;
            }
          }
        }
      }
    }
  }
  return -1;
}

/**
 * Reconnect the device if it was disconnected for example
 */
int reconnect_device(device * terminal) {
  if (terminal == NULL) {
    return -1;
  } else {
    if (terminal->serial_file != NULL) {
      terminal->serial_fd = serialport_init(terminal->serial_file, terminal->serial_baud);
    }
    if (terminal->serial_fd == -1) {
      return connect_device(terminal);
    } else {
      terminal->enabled=1;
      return terminal->serial_fd;
    }
  }
}

/**
 * Close the connection to the device
 */
int close_device(device * terminal) {
  if (terminal == NULL) {
    return 0;
  } else {
    terminal->enabled=0;
    return serialport_close(terminal->serial_fd);
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
	char * output = malloc(2*sizeof(char)), one_item[MSGLENGTH+1];

  strcpy(output, "");
  for (i=0; i<nb_terminal; i++) {
    strcpy(cur_name, "");
    strcpy(one_item, "");
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT de_display, de_active FROM an_device where de_name = '%q'", terminal[i]->name);
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
        strncat(one_item, "}", MSGLENGTH);
      } else {
        if (one_item == NULL) {
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
      }
    }
    sqlite3_finalize(stmt);
    output = realloc(output, strlen(output)+strlen(one_item)+1);
    strcat(output, one_item);
  }
  return output;
}

/**
 * Changes the state of a pin on the designated terminal
 */
int set_switch_state(device * terminal, char * pin, int status) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=-1;

  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "SETSWITCH%s,%d\n", pin, status);
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    result = strtol(serial_read+1, NULL, 10);
  }
  serialport_flush(terminal->serial_fd);
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Get the state of a pin on the designated terminal
 */
int get_switch_state(device * terminal, char * pin, int force) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=-1;
  
  pthread_mutex_lock(&terminal->lock);
  if (force == 1) {
    snprintf(serial_command, WORDLENGTH, "GETSWITCH%s,1\n", pin);
  } else {
    snprintf(serial_command, WORDLENGTH, "GETSWITCH%s\n", pin);
  }
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    result = strtol(serial_read+1, NULL, 10);
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Get the value of the designated sensor on the designated terminal
 */
float get_sensor_value(device * terminal, char * sensor, int force) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = 5000;
  float result=-999.;
  
  pthread_mutex_lock(&terminal->lock);
  if (force) {
    snprintf(serial_command, WORDLENGTH, "%s,1\n", sensor);
  } else {
    snprintf(serial_command, WORDLENGTH, "%s\n", sensor);
  }
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    result = atof(serial_read+1);
  }
  if (force) {
    serialport_flush(terminal->serial_fd);
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Send a heartbeat command to tell if the terminal is still responding
 */
int send_heartbeat(device * terminal) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result = 0;
  
  pthread_mutex_lock(&terminal->lock);
  if (!terminal->enabled) {
    return 0;
  }
  
  snprintf(serial_command, WORDLENGTH, "MARCO\n");
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
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
 * Get the overview of all pins and sensors for the device
 */
int get_overview(device * terminal, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "OVERVIEW\n");
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(terminal->serial_fd, output, eolchar, MSGLENGTH, timeout);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Send a REFRESH command to the selected terminal and parse the result into the output var
 */
int get_refresh(device * terminal, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "REFRESH\n");
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(terminal->serial_fd, output, eolchar, MSGLENGTH, timeout);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Get the name of the device
 */
int get_name(device * terminal, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "NAME\n");
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(terminal->serial_fd, output, eolchar, WORDLENGTH, timeout);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Get the Heater current status
 */
int get_heater(device * terminal, char * heat_id, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "GET%s\n", heat_id);
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    snprintf(output, WORDLENGTH, "%s", serial_read+1);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Set the Heater current status
 */
int set_heater(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * output) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  
  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "SET%s,%d,%.2f\n", heat_id, heat_enabled, max_heat_value);
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serial_result = serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    snprintf(output, WORDLENGTH, "%s", serial_read+1);
  }
  pthread_mutex_unlock(&terminal->lock);
  return (serial_result != -1);
}

/**
 * Get the light status
 */
int get_light(device * terminal, char * light) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=-1;
  
  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "GET%s\n", light);
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    result = strtol(serial_read+1, NULL, 10);
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * Get the light status
 */
int set_light(device * terminal, char * light, unsigned int status) {
  char eolchar = '}';
  char serial_command[WORDLENGTH+1] = {0}, serial_read[WORDLENGTH+1] = {0};
  int serial_result;
  int timeout = TIMEOUT;
  int result=-1;

  pthread_mutex_lock(&terminal->lock);
  snprintf(serial_command, WORDLENGTH, "SET%s,%d\n", light, status);
  serial_result = serialport_write(terminal->serial_fd, serial_command);
  if (serial_result != -1) {
    serialport_read_until(terminal->serial_fd, serial_read, eolchar, WORDLENGTH, timeout);
    serial_read[strlen(serial_read) - 1] = '\0';
    result = strtol(serial_read+1, NULL, 10);
  }
  serialport_flush(terminal->serial_fd);
  pthread_mutex_unlock(&terminal->lock);
  return result;
}
