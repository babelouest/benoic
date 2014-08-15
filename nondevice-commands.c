#include "angharad.h"

/**
 * Commands non dedicated to devices (actions, scripts, schedules, parsing, etc.)
 */

/**
 * Parse the result of a command OVERVIEW or REFRESH
 * get all the pin values in a table, the the sensor values in another table, then merge the results into json
 */
char * parse_overview(sqlite3 * sqlite3_db, char * overview_result) {
  char *tmp, *source, *saveptr, * overview_result_cpy = NULL, key[WORDLENGTH+1]={0}, value[WORDLENGTH+1]={0}, device[WORDLENGTH+1]={0}, tmp_value[WORDLENGTH+1], sanitized[WORDLENGTH+1], heater_value[WORDLENGTH+1]={0};
  char * one_element = malloc((MSGLENGTH+1)*sizeof(char)), * str_pins = NULL, * str_sensors = NULL, * str_heaters = NULL, * str_lights = NULL, * output = NULL;
  int i;
  pin * pins = NULL;
  sensor * sensors = NULL;
  heater * heaters = NULL;
  light * lights = NULL;
  int nb_pins = 0, nb_sensors = 0, nb_heaters = 0, nb_lights = 0;
  
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  overview_result_cpy = malloc(strlen(overview_result)*sizeof(char));
  snprintf(overview_result_cpy, strlen(overview_result)-1, "%s", overview_result+1);
  overview_result_cpy[strlen(overview_result_cpy) - 1] = '\0';
  source = overview_result_cpy;
  tmp = strtok_r( source, ",", &saveptr );
  
  while (NULL != tmp) {
    i = strcspn(tmp, ":");
    memset(key, 0, WORDLENGTH*sizeof(char));
    memset(value, 0, WORDLENGTH*sizeof(char));
    strncpy(key, tmp, i);
    strncpy(value, tmp+i+1, WORDLENGTH);
    if (0 == strncmp(key, "NAME", WORDLENGTH)) {
      snprintf(device, WORDLENGTH, "%s", value);
    } else if (0 == strncmp(key, "PIN", strlen("PIN"))) {
      pins = realloc(pins, (nb_pins+1)*sizeof(struct _pin));
      snprintf(pins[nb_pins].name, WORDLENGTH, "%s", key);
      pins[nb_pins].status = strtol(value, NULL, 10);
      sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_display, sw_active, sw_type from an_switch where sw_name='%q' and de_id in (select de_id from an_device where de_name='%q')", key, device);
      sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
      if (sql_result != SQLITE_OK) {
        log_message(LOG_INFO, "Error preparing sql query");
      } else {
        row_result = sqlite3_step(stmt);
        if (row_result == SQLITE_ROW) {
          sanitize_json_string((char*)sqlite3_column_text(stmt, 0), pins[nb_pins].display, WORDLENGTH);
          pins[nb_pins].enabled = sqlite3_column_int(stmt, 1);
          pins[nb_pins].type = sqlite3_column_int(stmt, 2);
        } else {
          // No result, default value
          snprintf(pins[nb_pins].display, WORDLENGTH, "%s", pins[nb_pins].name);
          pins[nb_pins].enabled = 1;
        }
      }
      sqlite3_finalize(stmt);
      nb_pins++;
    } else if (0 == strncmp(key, "LIGHT", strlen("LIGHT"))) {
      lights = realloc(lights, (nb_lights+1)*sizeof(struct _light));
      snprintf(lights[nb_lights].name, WORDLENGTH, "%s", key);
      snprintf(lights[nb_lights].device, WORDLENGTH, "%s", device);
      snprintf(lights[nb_lights].display, WORDLENGTH, "%s", key);
      lights[nb_lights].enabled = 1;
      lights[nb_lights].on = strncmp("1", value, 1)==0?1:0;
      sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT li_display, li_enabled from an_light where li_name='%q' and de_id in (select de_id from an_device where de_name='%q')", key, device);
      sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
      if (sql_result != SQLITE_OK) {
        log_message(LOG_INFO, "Error preparing sql query");
      } else {
        row_result = sqlite3_step(stmt);
        if (row_result == SQLITE_ROW) {
          sanitize_json_string((char*)sqlite3_column_text(stmt, 0), lights[nb_lights].display, WORDLENGTH);
          lights[nb_lights].enabled = sqlite3_column_int(stmt, 1);
        }
      }
      sqlite3_finalize(stmt);
      nb_lights++;
    } else if (0 == strncmp(key, "HEATER", strlen("HEATER"))) {
      heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
      snprintf(heater_value, WORDLENGTH, "%s", value);
      parse_heater(sqlite3_db, device, key, heater_value, &heaters[nb_heaters]);
      nb_heaters++;
    } else {
      sensors = realloc(sensors, (nb_sensors+1)*sizeof(struct _sensor));
      snprintf(sensors[nb_sensors].name, WORDLENGTH, "%s", key);
      snprintf(sensors[nb_sensors].value, WORDLENGTH, "%s", value);
      sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT se_display, se_unit, se_active from an_sensor where se_name='%q' and de_id in (select de_id from an_device where de_name='%q')", key, device);
      sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
      if (sql_result != SQLITE_OK) {
        log_message(LOG_INFO, "Error preparing sql query");
      } else {
        row_result = sqlite3_step(stmt);
        if (row_result == SQLITE_ROW) {
          sanitize_json_string((char*)sqlite3_column_text(stmt, 0), sensors[nb_sensors].display, WORDLENGTH);
          sanitize_json_string((char*)sqlite3_column_text(stmt, 1), sensors[nb_sensors].unit, WORDLENGTH);
          sensors[nb_sensors].enabled = sqlite3_column_int(stmt, 2);
        } else {
          // No result, default value
          snprintf(sensors[nb_sensors].display, WORDLENGTH, "%s", sensors[nb_sensors].name);
          strcpy(sensors[nb_sensors].unit, "");
          sensors[nb_sensors].enabled = 1;
        }
      }
      sqlite3_finalize(stmt);
      nb_sensors++;
    }
    tmp = strtok_r( NULL, ",", &saveptr );
  }
  
  // Arranging the results
  str_pins = malloc(2*sizeof(char));
  strcpy(str_pins, "[");
  for (i=0; i<nb_pins; i++) {
    strcpy(one_element, "");
    if (i>0) {
      strncat(one_element, ",", MSGLENGTH);
    }
    strncat(one_element, "{\"name\":\"", MSGLENGTH);
    sanitize_json_string(pins[i].name, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"display\":\"", MSGLENGTH);
    sanitize_json_string(pins[i].display, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"status\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", pins[i].status);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"type\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", pins[i].type);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"enabled\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", pins[i].enabled?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, "}", MSGLENGTH);
    str_pins = realloc(str_pins, strlen(str_pins)+strlen(one_element)+1);
    strcat(str_pins, one_element);
  }
  str_pins = realloc(str_pins, strlen(str_pins)+2);
  strcat(str_pins, "]");
  
  str_sensors = malloc(2*sizeof(char));
  strcpy(str_sensors, "[");
  for (i=0; i<nb_sensors; i++) {
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
    strncat(one_element, "}", MSGLENGTH);
    str_sensors = realloc(str_sensors, strlen(str_sensors)+strlen(one_element)+1);
    strcat(str_sensors, one_element);
  }
  str_sensors = realloc(str_sensors, strlen(str_sensors)+2);
  strcat(str_sensors, "]");
  
  str_heaters = malloc(2*sizeof(char));
  strcpy(str_heaters, "[");
  for (i=0; i<nb_heaters; i++) {
    strcpy(one_element, "");
    sanitize_json_string(heaters[i].name, tmp_value, WORDLENGTH);
    snprintf(one_element, MSGLENGTH, "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"set\":%s,\"on\":%s,\"max_value\":%.2f,\"unit\":\"%s\"}", i>0?",":"", heaters[i].name, heaters[i].display, heaters[i].enabled?"true":"false", heaters[i].set?"true":"false", heaters[i].on?"true":"false", heaters[i].heat_max_value, heaters[i].unit);
    str_heaters = realloc(str_heaters, strlen(str_heaters)+strlen(one_element)+1);
    strcat(str_heaters, one_element);
  }
  str_heaters = realloc(str_heaters, strlen(str_heaters)+2);
  strcat(str_heaters, "]");
  
  str_lights = malloc(2*sizeof(char));
  strcpy(str_lights, "[");
  for (i=0; i<nb_lights; i++) {
    strcpy(one_element, "");
    snprintf(one_element, MSGLENGTH, "%s{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"on\":%s}", i>0?",":"", lights[i].name, lights[i].display, lights[i].enabled?"true":"false", lights[i].on?"true":"false");
    str_lights = realloc(str_lights, strlen(str_lights)+strlen(one_element)+1);
    strcat(str_lights, one_element);
  }
  str_lights = realloc(str_lights, strlen(str_lights)+2);
  strcat(str_lights, "]");
  
  output = malloc((53+strlen(device)+strlen(str_pins)+strlen(str_lights)+strlen(str_sensors)+strlen(str_heaters))*sizeof(char));
  snprintf(output, MSGLENGTH, "{\"name\":\"%s\",\"pins\":%s,\"lights\":%s,\"sensors\":%s,\"heaters\":%s}", device, str_pins, str_lights, str_sensors, str_heaters);
  
  // Free all allocated pointers before return
  free(sql_query);
  sql_query = NULL;
  free(one_element);
  one_element = NULL;
  free(pins);
  pins = NULL;
  free(sensors);
  sensors = NULL;
  free(lights);
  lights = NULL;
  free(heaters);
  heaters = NULL;
  free(str_pins);
  str_pins = NULL;
  free(str_heaters);
  str_heaters = NULL;
  free(str_lights);
  str_lights = NULL;
  free(overview_result_cpy);
  overview_result_cpy = NULL;
  return output;
}

/**
 * Get the different actions for a specific device or for all devices
 */
char * get_actions(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char cur_name[WORDLENGTH+1]={0}, cur_device[WORDLENGTH+1]={0}, cur_pin[WORDLENGTH+1]={0}, cur_sensor[WORDLENGTH+1]={0}, cur_heater[WORDLENGTH+1]={0}, cur_params[WORDLENGTH+1]={0};
  char * actions = malloc(2*sizeof(char)), * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), * one_item = malloc((MSGLENGTH+1)*sizeof(char));
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ac.ac_id, ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, se.se_name, he.he_name, ac.ac_params FROM an_action ac LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_sensor se ON se.se_id = ac.se_id LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ac.ac_id, ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, se.se_name, he.he_name, ac.ac_params FROM an_action ac LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_sensor se ON se.se_id = ac.se_id LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id WHERE ac.de_id in (SELECT de_id FROM an_device where de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    row_result = sqlite3_step(stmt);
    strcpy(actions, "");
    while (row_result == SQLITE_ROW) {
      if (strlen(actions) > 0) {
        actions = realloc(actions, (strlen(actions)+2)*sizeof(char));
        strcat(actions, ",");
      }
      sanitize_json_string((char*)sqlite3_column_text(stmt, 1)==NULL?"":(char*)sqlite3_column_text(stmt, 1), cur_name, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 3)==NULL?"":(char*)sqlite3_column_text(stmt, 3), cur_device, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 4)==NULL?"":(char*)sqlite3_column_text(stmt, 4), cur_pin, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 5)==NULL?"":(char*)sqlite3_column_text(stmt, 5), cur_sensor, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 6)==NULL?"":(char*)sqlite3_column_text(stmt, 6), cur_heater, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 7)==NULL?"":(char*)sqlite3_column_text(stmt, 7), cur_params, WORDLENGTH);
      snprintf(one_item, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"pin\":\"%s\",\"sensor\":\"%s\",\"heater\":\"%s\",\"params\":\"%s\"}",
        sqlite3_column_int(stmt, 0),
        cur_name,
        sqlite3_column_int(stmt, 2),
        cur_device,
        cur_pin,
        cur_sensor,
        cur_heater,
        cur_params);
      actions = realloc(actions, (strlen(actions)+strlen(one_item)+2)*sizeof(char));
      strcat(actions, one_item);
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return actions;
  }
}

/**
 * Get the different scripts ids present
 */
char * get_scripts(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char cur_name[WORDLENGTH+1], device_name[WORDLENGTH+1];
  int cur_id, cur_enabled;
  char * scripts = malloc(2*sizeof(char)), * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), * one_item = NULL, * actions = NULL;
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id WHERE de.de_id in (SELECT de_id FROM an_device where de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(scripts);
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    strcpy(scripts, "");
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      if (row_result == SQLITE_ROW) {
        if (strlen(scripts) > 0) {
          scripts = realloc(scripts, (strlen(scripts)+2)*sizeof(char));
          strcat(scripts, ",");
        }
        cur_id = sqlite3_column_int(stmt, 0);
        snprintf(cur_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
        cur_enabled = sqlite3_column_int(stmt, 2);
        if (sqlite3_column_text(stmt, 3) != NULL) {
          snprintf(device_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 3));
        } else {
          strcpy(device_name, "");
        }
        sanitize_json_string(cur_name, cur_name, WORDLENGTH);
        sanitize_json_string(device_name, device_name, WORDLENGTH);
        actions = get_action_script(sqlite3_db, cur_id);
        if (actions == NULL) {
          log_message(LOG_INFO, "Error getting actions from script");
        }
        one_item = malloc((strlen(cur_name)+strlen(device_name)+strlen(actions)+60+num_digits(cur_id))*sizeof(char));
        sprintf(one_item, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"actions\":[%s]}", cur_id, cur_name, cur_enabled?"true":"false", device_name, actions);
        scripts = realloc(scripts, (strlen(scripts)+strlen(one_item)+1)*sizeof(char));
        strcat(scripts, one_item);
        free(actions);
        free(one_item);
      } else if (row_result == SQLITE_DONE) {
        break;
      } else {
        free(scripts);
        sqlite3_finalize(stmt);
        return NULL;
      }
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return scripts;
  }
}

/**
 * Get the actions associated with the given script id
 */
char * get_action_script(sqlite3 * sqlite3_db, int script_id) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), ac_name[WORDLENGTH+1], value_condition[WORDLENGTH+1], * actions = malloc(sizeof(char)), * one_item = malloc((MSGLENGTH+1)*sizeof(char));
  int rank, result_condition, ac_id;
  
  if (script_id == 0) {
    log_message(LOG_INFO, "Error getting action scripts, script_id is 0");
    free(sql_query);
    free(actions);
    free(one_item);
    return NULL;
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ac.ac_id, ac.ac_name, aas.as_rank, aas.as_result_condition, aas.as_value_condition FROM an_action_script aas, an_action ac WHERE ac.ac_id=aas.ac_id AND aas.sc_id='%d' ORDER BY as_rank", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(actions);
    free(one_item);
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    strcpy(actions, "");
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      if (strlen(actions) > 0) {
        actions = realloc(actions, (strlen(actions)+2)*sizeof(char));
        strcat(actions, ",");
      }
      if (row_result == SQLITE_ROW) {
        ac_id = sqlite3_column_int(stmt, 0);
        snprintf(ac_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
        sanitize_json_string(ac_name, ac_name, WORDLENGTH);
        rank = sqlite3_column_int(stmt, 2);
        result_condition = sqlite3_column_int(stmt, 3);
        if (sqlite3_column_type(stmt, 4) == SQLITE_NULL) {
          strcpy(value_condition, "-1");
        } else {
          snprintf(value_condition, WORDLENGTH, "%d", sqlite3_column_int(stmt, 4));
        }
        sprintf(one_item, "{\"id\":%d,\"name\":\"%s\",\"rank\":%d,\"result_condition\":%d,\"result_value\":%s}", ac_id, ac_name, rank, result_condition, value_condition);
        actions = realloc(actions, (strlen(actions)+strlen(one_item)+1)*sizeof(char));
        strcat(actions, one_item);
      } else if (row_result == SQLITE_DONE) {
        break;
      } else {
        free(actions);
        free(one_item);
        sqlite3_finalize(stmt);
        return NULL;
      }
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    free(one_item);
    return actions;
  }
}

/**
 * get the selected script
 */
int get_script(sqlite3 * sqlite3_db, char * script_id, char * overview) {
  sqlite3_stmt *stmt;
  int sql_result, sc_id, sc_enabled, row_result;
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), sc_name[WORDLENGTH+1], device[WORDLENGTH+1], tmp[WORDLENGTH*3];
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id WHERE sc.sc_id = '%q'", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sc_id = sqlite3_column_int(stmt, 0);
      snprintf(sc_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      sanitize_json_string(sc_name, sc_name, WORDLENGTH);
      sc_enabled = sqlite3_column_int(stmt, 2);
      snprintf(device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 3));
      snprintf(overview, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\"}", sc_id, sc_name, sc_enabled?"true":"false", (0==strcmp(device,"(null)"))?"":device);
      sqlite3_finalize(stmt);
      return 1;
    } else {
      snprintf(tmp, MSGLENGTH, "Script %s not found", script_id);
      log_message(LOG_INFO, tmp);
      sqlite3_finalize(stmt);
      return 0;
    }
  }
}

/**
 * Run a script by running its actions one by one
 * Each action should be ran if the precedent ran successfully,
 * and its result is consistent to the expected result
 */
int run_script(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, char * script_id) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  action ac;
    
  snprintf(sql_query, MSGLENGTH, "SELECT ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, se.se_name, ac.ac_params, acs.as_result_condition, acs.as_value_condition, ac.ac_id FROM an_action ac, an_action_script acs LEFT OUTER JOIN an_device de on de.de_id = ac.de_id LEFT OUTER JOIN an_switch sw on sw.sw_id = ac.sw_id LEFT OUTER JOIN an_sensor se on se.se_id = ac.se_id WHERE ac.ac_id = acs.ac_id AND acs.sc_id = '%s' order by acs.as_rank", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        snprintf(ac.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
      }
      if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
        ac.type = sqlite3_column_int(stmt, 1);
      }
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        snprintf(ac.device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      } else {
        memset(ac.device, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
        snprintf(ac.pin, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 3));
      } else {
        memset(ac.pin, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        snprintf(ac.sensor, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 4));
      } else {
        memset(ac.sensor, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
        snprintf(ac.params, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 5));
      } else {
        memset(ac.params, 0, MSGLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        ac.condition_result = sqlite3_column_int(stmt, 6);
      }
      /*printf("current action\n\tname: %s\n\ttype: %d\n\tdevice: %s\n\tpin: %s\n\tsensor: %s\n\tparams: %s\n\tcondition result: %d\n", 
            ac.name,
            ac.type,
            ac.device,
            ac.pin,
            ac.sensor,
            ac.params,
            ac.condition_result);*/
      switch (ac.type) {
        case ACTION_GET:
        case ACTION_SET:
        case ACTION_HEATER:
          ac.condition_value.type = VALUE_INT;
          if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            ac.condition_value.i_value = sqlite3_column_int(stmt, 7);
          }
          break;
        case ACTION_SENSOR:
          ac.condition_value.type = VALUE_FLOAT;
          if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            ac.condition_value.f_value = (float)sqlite3_column_double(stmt, 7);
          }
          break;
        case ACTION_DEVICE:
        case ACTION_OVERVIEW:
        case ACTION_REFRESH:
        case ACTION_SYSTEM:
        default:
          ac.condition_value.type = VALUE_STRING;
          if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            snprintf(ac.condition_value.s_value, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 7));
          }
          break;
      }
      ac.id = sqlite3_column_int(stmt, 8);
      if (!run_action(ac, terminal, nb_terminal, sqlite3_db)) {
        sqlite3_finalize(stmt);
        return 0;
      }
      row_result = sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);
  return 1;
}

/**
 * Run the specified action, evaluate the result and return if the result is valid
 */
int run_action(action ac, device ** terminal, unsigned int nb_terminal, sqlite3 * sqlite3_db) {
  char str_result[MSGLENGTH+1], tmp[WORDLENGTH+1], jo_command[MSGLENGTH+1], jo_result[MSGLENGTH+1];
  value first;
  int heat_set;
  float heat_max_value;
  device * cur_terminal;
  
  switch (ac.type) {
    case ACTION_DEVICE:
      if (get_devices(sqlite3_db, terminal, nb_terminal)) {
        first.type = VALUE_STRING;
        ac.result_value = first;
      } else {
        return 0;
      }
      break;
    case ACTION_OVERVIEW:
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled && get_overview(cur_terminal, str_result)) {
          first.type = VALUE_STRING;
          snprintf(first.s_value, WORDLENGTH, "%s", str_result);
          ac.result_value = first;
        } else {
          return 0;
        }
      } else {
        return 0;
      }
      break;
    case ACTION_REFRESH:
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled && get_refresh(cur_terminal, str_result)) {
          first.type = VALUE_STRING;
          snprintf(first.s_value, WORDLENGTH, "%s", str_result);
          ac.result_value = first;
        } else {
          return 0;
        }
      } else {
        return 0;
      }
      break;
    case ACTION_GET:
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        first.type = VALUE_INT;
        if (cur_terminal->enabled) {
          first.i_value = get_switch_state(cur_terminal, ac.pin+3, (0 == strcmp(ac.params, "1")));
        } else {
          first.i_value = 0;
        }
        ac.result_value = first;
      }
      break;
    case ACTION_SET:
      if (ac.device != NULL) {
        first.type = VALUE_INT;
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled) {
          first.i_value = set_switch_state(cur_terminal, ac.pin+3, (0 == strcmp(ac.params, "1")));
          if (!set_startup_pin_status(sqlite3_db, cur_terminal->name, ac.pin+3, (0 == strcmp(ac.params, "1")))) {
            log_message(LOG_INFO, "Error saving pin status in the database");
          }
        } else {
          first.i_value = 0;
        }
        ac.result_value = first;
      }
      break;
    case ACTION_SENSOR:
      if (ac.device != NULL && ac.sensor != NULL) {
        first.type = VALUE_FLOAT;
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled) {
          first.f_value = get_sensor_value(get_device_from_name(ac.device, terminal, nb_terminal), ac.sensor, (0 == strcmp(ac.params, "1")));
        } else {
          first.f_value = 0.0;
        }
        ac.result_value = first;
      }
      break;
    case ACTION_HEATER:
      if (ac.device != NULL && ac.heater != NULL && ac.params != NULL) {
        heat_set = ac.params[0]=='1'?1:0;
        heat_max_value = strtof(ac.params+2, NULL);
        first.type = VALUE_INT;
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled && set_heater(cur_terminal, ac.heater, heat_set, heat_max_value, tmp)) {
          first.i_value = 1;
          if (!set_startup_heater_status(sqlite3_db, cur_terminal->name, ac.heater, heat_set, heat_max_value)) {
            log_message(LOG_INFO, "Error saving heater status in the database");
          }
        } else {
          first.i_value = 0;
        }
        ac.result_value = first;
      }
      break;
    case ACTION_SYSTEM:
      system(ac.params);
      break;
  }
  snprintf(jo_command, MSGLENGTH, "run_action \"%s\" (id:%d, type:%d)", ac.name, ac.id, ac.type);
  switch (ac.result_value.type) {
    case VALUE_INT:
      snprintf(jo_result, MSGLENGTH, "%d", ac.result_value.i_value);
      break;
    case VALUE_FLOAT:
      snprintf(jo_result, MSGLENGTH, "%.2f", ac.result_value.f_value);
      break;
    case VALUE_STRING:
      snprintf(jo_result, MSGLENGTH, "%s", ac.result_value.s_value);
      break;
  }
  journal(sqlite3_db, "run_script", jo_command, jo_result);
  return evaluate_values(ac);
}

/**
 * Evaluate the action result
 */
int evaluate_values(action ac) {
  value first, next;
  int condition;
  
  first = ac.result_value;
  next = ac.condition_value;
  condition = ac.condition_result;
  
  if (condition == CONDITION_NO) {
    return 1; // No need to compare
  } else if (first.type != next.type) {
    return 0; // No need to compare
  } else {
    switch (condition) {
      case CONDITION_EQ:
        if (first.type == VALUE_INT) {
          return (first.i_value == next.i_value);
        } else if (first.type == VALUE_FLOAT) {
          return (first.f_value == next.f_value);
        } else if (first.type == VALUE_STRING) {
          return (0 == strncmp(first.s_value, next.s_value, WORDLENGTH));
        } else {
          return 0;
        }
        break;
      case CONDITION_LT:
        if (first.type == VALUE_INT) {
          return (first.i_value < next.i_value);
        } else if (first.type == VALUE_FLOAT) {
          return (first.f_value < next.f_value);
        } else if (first.type == VALUE_STRING) {
          return (0 > strncmp(first.s_value, next.s_value, WORDLENGTH));
        } else {
          return 0;
        }
        break;
      case CONDITION_LE:
        if (first.type == VALUE_INT) {
          return (first.i_value <= next.i_value);
        } else if (first.type == VALUE_FLOAT) {
          return (first.f_value <= next.f_value);
        } else if (first.type == VALUE_STRING) {
          return (0 >= strncmp(first.s_value, next.s_value, WORDLENGTH));
        } else {
          return 0;
        }
        break;
      case CONDITION_GE:
        if (first.type == VALUE_INT) {
          return (first.i_value > next.i_value);
        } else if (first.type == VALUE_FLOAT) {
          return (first.f_value > next.f_value);
        } else if (first.type == VALUE_STRING) {
          return (0 < strncmp(first.s_value, next.s_value, WORDLENGTH));
        } else {
          return 0;
        }
        break;
      case CONDITION_GT:
        if (first.type == VALUE_INT) {
          return (first.i_value >= next.i_value);
        } else if (first.type == VALUE_FLOAT) {
          return (first.f_value >= next.f_value);
        } else if (first.type == VALUE_STRING) {
          return (0 <= strncmp(first.s_value, next.s_value, WORDLENGTH));
        } else {
          return 0;
        }
        break;
      case CONDITION_NE:
        if (first.type == VALUE_INT) {
          return (first.i_value != next.i_value);
        } else if (first.type == VALUE_FLOAT) {
          return (first.f_value != next.f_value);
        } else if (first.type == VALUE_STRING) {
          return (0 != strncmp(first.s_value, next.s_value, WORDLENGTH));
        } else {
          return 0;
        }
        break;
      case CONDITION_CO: // Not applicable to numerics
        if (first.type == VALUE_STRING) {
          return (NULL == strstr(first.s_value, next.s_value));
        } else {
          return 0;
        }
        break;
      default:
        return 0;
        break;
    }
  }
}

/**
 * Get all the schedules for the specified device
 * Or all the schedules if device is NULL
 */
char * get_schedules(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), * one_item = NULL, cur_name[WORDLENGTH+1], cur_device[WORDLENGTH+1];
  int cur_id;
  long next_time;
  char * script = malloc((MSGLENGTH+1)*sizeof(char)), script_id[WORDLENGTH+1], * scripts = malloc(2*sizeof(char));
  int enabled, repeat_schedule, repeat_schedule_value;
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh.sh_id, sh.sh_name, sh.sh_enabled, sh.sh_next_time, sh.sh_repeat_schedule, sh.sh_repeat_schedule_value, sh.sc_id, de.de_name FROM an_scheduler sh LEFT OUTER JOIN an_device de ON de.de_id = sh.de_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh.sh_id, sh.sh_name, sh.sh_enabled, sh.sh_next_time, sh.sh_repeat_schedule, sh.sh_repeat_schedule_value, sh.sc_id, de.de_name FROM an_scheduler sh LEFT OUTER JOIN an_device de ON de.de_id = sh.de_id WHERE sh.de_id in (SELECT de_id FROM an_device where de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(script);
    free(scripts);
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    strcpy(scripts, "");
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      if (strlen(scripts) > 0) {
        scripts = realloc(scripts, (strlen(scripts)+2)*sizeof(char));
        strcat(scripts, ",");
      }
      cur_id = sqlite3_column_int(stmt, 0);
      snprintf(cur_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      sanitize_json_string(cur_name, cur_name, WORDLENGTH);
      enabled = sqlite3_column_int(stmt, 2);
      next_time = (long)sqlite3_column_int(stmt, 3);
      repeat_schedule = sqlite3_column_int(stmt, 4);
      repeat_schedule_value = sqlite3_column_int(stmt, 5);
      snprintf(script_id, WORDLENGTH, "%d", sqlite3_column_int(stmt, 6));
      if (sqlite3_column_text(stmt, 7) != NULL) {
        snprintf(cur_device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 7));
      } else {
        strcpy(cur_device, "");
      }
      sanitize_json_string(cur_device, cur_device, WORDLENGTH);
      if (!get_script(sqlite3_db, script_id, script)) {
        strcpy(script, "{}");
      }
      sanitize_json_string(cur_name, cur_name, WORDLENGTH);
      one_item = malloc((95+num_digits(cur_id)+strlen(cur_name)+num_digits(cur_id)+strlen(cur_device)+num_digits_l(next_time)+num_digits(repeat_schedule)+num_digits(repeat_schedule_value)+strlen(script))*sizeof(char));
      sprintf(one_item, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"next_time\":%ld,\"repeat\":%d,\"repeat_value\":%d,\"script\":%s}", cur_id, cur_name, enabled?"true":"false", cur_device, next_time, repeat_schedule, repeat_schedule_value, script);
      scripts = realloc(scripts, (strlen(scripts)+strlen(one_item)+1)*sizeof(char));
      strcat(scripts, one_item);
      free(one_item);
      one_item = NULL;
      row_result = sqlite3_step(stmt);
    }
    free(script);
    sqlite3_finalize(stmt);
    return scripts;
  }
}

/**
 * Change the state of a schedule
 */
int enable_schedule(sqlite3 * sqlite3_db, char * schedule, char * status, char * command_result) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), * script = malloc((MSGLENGTH+1)*sizeof(char)), script_id[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  struct _schedule cur_schedule;

  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_scheduler SET sh_enabled='%q' WHERE sh_id='%q'", status, schedule);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh_id, sh_name, sh_enabled, sh_next_time, sh_repeat_schedule, sh_repeat_schedule_value, sc_id FROM an_scheduler WHERE sh_id='%q'", schedule);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query");
      sqlite3_finalize(stmt);
      return 0;
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        cur_schedule.id = sqlite3_column_int(stmt, 0);
        snprintf(cur_schedule.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
        cur_schedule.enabled = sqlite3_column_int(stmt, 2);
        cur_schedule.next_time = (long)sqlite3_column_int(stmt, 3);
        cur_schedule.repeat_schedule = sqlite3_column_int(stmt, 4);
        cur_schedule.repeat_schedule_value = sqlite3_column_int(stmt, 5);
        if (!update_schedule(sqlite3_db, &cur_schedule)) {
          log_message(LOG_INFO, "Error updating schedule on database");
        }
        snprintf(script_id, WORDLENGTH, "%d", sqlite3_column_int(stmt, 6));
        if (!get_script(sqlite3_db, script_id, script)) {
          strcpy(script, "{}");
        }
        sanitize_json_string(cur_schedule.name, cur_schedule.name, WORDLENGTH);
        snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat\":%d,\"repeat_value\":%d,\"script\":%s}", cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, script);
        free(script);
        sqlite3_finalize(stmt);
        return 1;
      } else {
        log_message(LOG_INFO, "Error getting schedule data");
        free(script);
        sqlite3_finalize(stmt);
        return 0;
      }
    }
  } else {
    log_message(LOG_INFO, "Error updating schedule");
    free(script);
    sqlite3_finalize(stmt);
    return 0;
  }
}

/**
 * Change the display name and the enable settings for a device
 */
int set_device_data(sqlite3 * sqlite3_db, device cur_device, char * command_result) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_device (de_id, de_name, de_display, de_active) VALUES ((SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%d')", cur_device.name, cur_device.name, cur_device.display, cur_device.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sanitize_json_string(cur_device.name, cur_device.name, WORDLENGTH);
    sanitize_json_string(cur_device.display, cur_device.display, WORDLENGTH);
    snprintf(command_result, MSGLENGTH, "{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s}", cur_device.name, cur_device.display, cur_device.enabled?"true":"false");
    free(sql_query);
    return 1;
  } else {
    free(sql_query);
    return 0;
  }
}

/**
 * Change the display name, the type and the enable settings for a device
 */
int set_pin_data(sqlite3 * sqlite3_db, pin cur_pin, char * command_result) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_switch (sw_id, de_id, sw_name, sw_display, sw_type, sw_active, sw_status) VALUES ((SELECT sw_id FROM an_switch where sw_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%d', '%d', (SELECT sw_status FROM an_switch where sw_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')))", cur_pin.name, cur_pin.device, cur_pin.device, cur_pin.name, cur_pin.display, cur_pin.type, cur_pin.enabled, cur_pin.name, cur_pin.device);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sanitize_json_string(cur_pin.name, cur_pin.name, WORDLENGTH);
    sanitize_json_string(cur_pin.display, cur_pin.display, WORDLENGTH);
    snprintf(command_result, MSGLENGTH, "{\"name\":\"%s\",\"display\":\"%s\",\"type\":%d,\"enabled\":%s}", cur_pin.name, cur_pin.display, cur_pin.type, cur_pin.enabled?"true":"false");
    free(sql_query);
    return 1;
  } else {
    free(sql_query);
    return 0;
  }
}

/**
 * Change the display name and the enable settings for a device
 */
int set_sensor_data(sqlite3 * sqlite3_db, sensor cur_sensor, char * command_result) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_sensor (se_id, de_id, se_name, se_display, se_unit, se_active) VALUES ((SELECT se_id FROM an_sensor where se_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%q', '%d')", cur_sensor.name, cur_sensor.device, cur_sensor.device, cur_sensor.name, cur_sensor.display, cur_sensor.unit, cur_sensor.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sanitize_json_string(cur_sensor.name, cur_sensor.name, WORDLENGTH);
    sanitize_json_string(cur_sensor.display, cur_sensor.display, WORDLENGTH);
    sanitize_json_string(cur_sensor.unit, cur_sensor.unit, WORDLENGTH);
    snprintf(command_result, MSGLENGTH, "{\"name\":\"%s\",\"display\":\"%s\",\"unit\":\"%s\",\"enabled\":%s}", cur_sensor.name, cur_sensor.display, cur_sensor.unit, cur_sensor.enabled?"true":"false");
    free(sql_query);
    return 1;
  } else {
    free(sql_query);
    return 0;
  }
}

/**
 * Change the display name and the enable settings for a light
 */
int set_light_data(sqlite3 * sqlite3_db, light cur_light, char * command_result) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_light (li_id, de_id, li_name, li_display, li_enabled) VALUES ((SELECT li_id FROM an_light where li_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%d')", cur_light.name, cur_light.device, cur_light.device, cur_light.name, cur_light.display, cur_light.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sanitize_json_string(cur_light.name, cur_light.name, WORDLENGTH);
    sanitize_json_string(cur_light.display, cur_light.display, WORDLENGTH);
    snprintf(command_result, MSGLENGTH, "{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s}", cur_light.name, cur_light.display, cur_light.enabled?"true":"false");
    free(sql_query);
    return 1;
  } else {
    free(sql_query);
    return 0;
  }
}

/**
 * Change the display name, the unit and the enable settings for a heater
 */
int set_heater_data(sqlite3 * sqlite3_db, heater cur_heater, char * command_result) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_heater (he_id, de_id, he_name, he_display, he_unit, he_enabled) VALUES ((SELECT he_id FROM an_heater where he_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%q', '%d')", cur_heater.name, cur_heater.device, cur_heater.device, cur_heater.name, cur_heater.display, cur_heater.unit, cur_heater.enabled);
  //printf("%s\n", sql_query);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sanitize_json_string(cur_heater.name, cur_heater.name, WORDLENGTH);
    sanitize_json_string(cur_heater.display, cur_heater.display, WORDLENGTH);
    sanitize_json_string(cur_heater.unit, cur_heater.unit, WORDLENGTH);
    snprintf(command_result, MSGLENGTH, "{\"name\":\"%s\",\"display\":\"%s\",\"unit\":\"%s\",\"enabled\":%s}", cur_heater.name, cur_heater.display, cur_heater.unit, cur_heater.enabled?"true":"false");
    free(sql_query);
    return 1;
  } else {
    free(sql_query);
    return 0;
  }
}

/**
 * Add an action into the database
 */
int add_action(sqlite3 * sqlite3_db, action cur_action, char * command_result) {
  char * sql_query = NULL, device[WORDLENGTH+1], pin[WORDLENGTH+1], sensor[WORDLENGTH+1], heater[WORDLENGTH+1], params[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  // Verify input data
  if (0 == strcmp(cur_action.name, "")) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_OVERVIEW && 0 == strcmp(cur_action.device, "")) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_REFRESH && 0 == strcmp(cur_action.device, "")) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_GET && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.pin, ""))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SET && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.pin, "")) || 0 == strcmp(cur_action.params, ""))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SENSOR && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.sensor, "")))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_HEATER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.heater, "") || (0 == strcmp(cur_action.params, "")))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SYSTEM && 0 == strcmp(cur_action.params, "")) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  
  sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  if (cur_action.type == ACTION_DEVICE) {
    strcpy(device, "");
    strcpy(pin, "");
    strcpy(sensor, "");
    strcpy(params, "");
  }

  if (cur_action.type == ACTION_REFRESH) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(pin, "");
    strcpy(sensor, "");
    strcpy(params, "");
  }
  
  if (cur_action.type == ACTION_GET) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(pin, WORDLENGTH, "%s", cur_action.pin);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SET) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(pin, WORDLENGTH, "%s", cur_action.pin);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SENSOR) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(pin, "");
    snprintf(sensor, WORDLENGTH, "%s", cur_action.sensor);
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_HEATER) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(pin, "");
    strcpy(sensor, "");
    snprintf(heater, WORDLENGTH, "%s", cur_action.heater);
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SYSTEM) {
    strcpy(device, "");
    strcpy(pin, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
                
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_action (ac_name, ac_type, de_id, sw_id, se_id, he_id, ac_params) VALUES ('%q', '%d', (select de_id from an_device where de_name='%q'), (select sw_id from an_switch where sw_name='%q' and de_id in (select de_id from an_device where de_name='%q')), (select se_id from an_sensor where se_name='%q' and de_id in (select de_id from an_device where de_name='%q')), (select he_id from an_heater where he_name='%q' and de_id in (select de_id from an_device where de_name='%q')), '%q')", cur_action.name, cur_action.type, device, pin, device, sensor, device, heater, device, params);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sprintf(sql_query, "SELECT last_insert_rowid()");
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query last_insert_rowid()");
      free(sql_query);
      sqlite3_finalize(stmt);
      return 0;
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        cur_action.id = sqlite3_column_int(stmt, 0);
        snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"pin\":\"%s\",\"sensor\":\"%s\",\"params\":\"%s\"}", cur_action.id, cur_action.name, cur_action.type, device, pin, sensor, params);
        free(sql_query);
        sqlite3_finalize(stmt);
        return 1;
      } else {
        log_message(LOG_INFO, "Error getting last_insert_rowid()");
        free(sql_query);
        sqlite3_finalize(stmt);
        return 0;
      }
    }
  } else {
    log_message(LOG_INFO, "Error inserting action");
    free(sql_query);
    sqlite3_finalize(stmt);
    return 0;
  }
}

/**
 * Modifies the specified action
 */
int set_action(sqlite3 * sqlite3_db, action cur_action, char * command_result) {
  char * sql_query = NULL, device[WORDLENGTH+1], pin[WORDLENGTH+1], sensor[WORDLENGTH+1], heater[WORDLENGTH+1], params[MSGLENGTH+1];
  
  // Verify input data
  if (0 == strcmp(cur_action.name, "")) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_OVERVIEW && 0 == strcmp(cur_action.device, "")) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_REFRESH && 0 == strcmp(cur_action.device, "")) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_GET && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.pin, ""))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SET && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.pin, "")) || 0 == strcmp(cur_action.params, ""))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SENSOR && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.sensor, "")))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_HEATER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.heater, "") || (0 == strcmp(cur_action.params, "")))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SYSTEM && 0 == strcmp(cur_action.params, "")) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  
  sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  if (cur_action.type == ACTION_DEVICE) {
    strcpy(device, "");
    strcpy(pin, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    strcpy(params, "");
  }

  if (cur_action.type == ACTION_REFRESH) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(pin, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    strcpy(params, "");
  }
  
  if (cur_action.type == ACTION_GET) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(pin, WORDLENGTH, "%s", cur_action.pin);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SET) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(pin, WORDLENGTH, "%s", cur_action.pin);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SENSOR) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(pin, "");
    snprintf(sensor, WORDLENGTH, "%s", cur_action.sensor);
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_HEATER) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(pin, "");
    strcpy(sensor, "");
    snprintf(heater, WORDLENGTH, "%s", cur_action.heater);
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SYSTEM) {
    strcpy(device, "");
    strcpy(pin, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
                
  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_action SET ac_name='%q', ac_type='%d', de_id=(select de_id from an_device where de_name='%q'), sw_id=(select sw_id from an_switch where sw_name='%q' and de_id in (select de_id from an_device where de_name='%q')), se_id=(select se_id from an_sensor where se_name='%q' and de_id in (select de_id from an_device where de_name='%q')), he_id=(select he_id from an_heater where he_name='%q' and de_id in (select de_id from an_device where de_name='%q')), ac_params='%q' WHERE ac_id='%d'", cur_action.name, cur_action.type, device, pin, device, sensor, device, heater, device, params, cur_action.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    free(sql_query);
    snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"pin\":\"%s\",\"sensor\":\"%s\",\"params\":\"%s\"}", cur_action.id, cur_action.name, cur_action.type, device, pin, sensor, params);
    return 1;
  } else {
    free(sql_query);
    log_message(LOG_INFO, "Error updating action");
    return 0;
  }
}

/**
 * Delete the specified action
 */
int delete_action(sqlite3 * sqlite3_db, char * action_id) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action_script WHERE ac_id='%q'", action_id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action WHERE ac_id='%q'", action_id);
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      free(sql_query);
      return 1;
    } else {
      free(sql_query);
      log_message(LOG_INFO, "Error deleting action");
      return 0;
    }
  } else {
    free(sql_query);
    log_message(LOG_INFO, "Error deleting action_script");
    return 0;
  }
}

/**
 * Add a script into the database
 */
int add_script(sqlite3 * sqlite3_db, script cur_script, char * command_result) {
  char * sql_query = NULL, tmp[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, rank=0;
  
  char * action_token, * action, * result_condition, * value_condition, * saveptr, * saveptr2;
  
  if (0 == strcmp("", cur_script.name)) {log_message(LOG_INFO, "Error inserting script, wrong params"); return 0;}
  
  sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_script (sc_name, de_id, sc_enabled) VALUES ('%q', (SELECT de_id FROM an_device where de_name='%q'), '%d')", cur_script.name, cur_script.device, cur_script.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sprintf(sql_query, "SELECT last_insert_rowid()");
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query last_insert_rowid()");
      free(sql_query);
      sqlite3_finalize(stmt);
      return 0;
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        cur_script.id = sqlite3_column_int(stmt, 0);
        
        // Parsing actions, then insert into an_action_script
        action_token = strtok_r(cur_script.actions, ";", &saveptr);
        while (action_token != NULL) {
          action = strtok_r(action_token, ",", &saveptr2);
          result_condition = strtok_r(NULL, ",", &saveptr2);
          value_condition = strtok_r(NULL, ",", &saveptr2);
          
          if (action != NULL && result_condition != NULL && (0 == strcmp("0", result_condition) || value_condition != NULL)) {
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_action_script (sc_id, ac_id, as_rank, as_result_condition, as_value_condition) VALUES ('%d', '%q', '%d', '%q', '%q')", cur_script.id, action, rank++, result_condition, value_condition==NULL?"":value_condition);
            if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
              snprintf(tmp, MSGLENGTH, "Error inserting action (%d, %s, %d, %s, %s", cur_script.id, action, rank++, result_condition, value_condition);
              log_message(LOG_INFO, tmp);
            }
          } else {
            log_message(LOG_INFO, "Error inserting action, wrong parameters");
          }
          action_token = strtok_r(NULL, ";", &saveptr);
        }
        snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\"}", cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device);
        free(sql_query);
        sqlite3_finalize(stmt);
        return 1;
      } else {
        log_message(LOG_INFO, "Error executing sql query last_insert_rowid()");
        free(sql_query);
        sqlite3_finalize(stmt);
        return 0;
      }
    }
  } else {
    log_message(LOG_INFO, "Error inserting script");
    free(sql_query);
    return 0;
  }
}

/**
 * Modifies the specified script
 */
int set_script(sqlite3 * sqlite3_db, script cur_script, char * command_result) {
  char * sql_query = NULL, tmp[MSGLENGTH+1];
  int rank=0;
  
  char * action_token, * action, * result_condition, * value_condition, * saveptr, * saveptr2;
  
  if (0 == strcmp("", cur_script.name)) {log_message(LOG_INFO, "Error updating script, wrong params"); return 0;}
  if (cur_script.id == 0) {log_message(LOG_INFO, "Error updating script, wrong params"); return 0;}
  
  sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  // Reinit action_script list
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action_script WHERE sc_id='%d'", cur_script.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
    log_message(LOG_INFO, "Error updating script, wrong params");
    free(sql_query);
    return 0;
  }
  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_script SET sc_name='%q', de_id=(SELECT de_id FROM an_device where de_name='%q'), sc_enabled='%d' WHERE sc_id='%d'", cur_script.name, cur_script.device, cur_script.enabled, cur_script.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    // Parsing actions, then insert into an_action_script
    action_token = strtok_r(cur_script.actions, ";", &saveptr);
    while (action_token != NULL) {
      action = strtok_r(action_token, ",", &saveptr2);
      result_condition = strtok_r(NULL, ",", &saveptr2);
      value_condition = strtok_r(NULL, ",", &saveptr2);
      if (action != NULL && result_condition != NULL && (0 == strcmp("0", result_condition) || value_condition != NULL)) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_action_script (sc_id, ac_id, as_rank, as_result_condition, as_value_condition) VALUES ('%d', '%q', '%d', '%q', '%q')", cur_script.id, action, rank++, result_condition, value_condition==NULL?"":value_condition);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          snprintf(tmp, MSGLENGTH, "Error updating action (%d, %s, %d, %s, %s", cur_script.id, action, rank++, result_condition, value_condition);
          log_message(LOG_INFO, tmp);
        }
      } else {
        log_message(LOG_INFO, "Error updating action list, wrong parameters");
      }
      action_token = strtok_r(NULL, ";", &saveptr);
    }
    snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\"}", cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device);
    free(sql_query);
    return 1;
  } else {
    log_message(LOG_INFO, "Error updating script");
    free(sql_query);
    return 0;
  }
}

/**
 * Delete the specified script
 */
int delete_script(sqlite3 * sqlite3_db, char * script_id) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action_script WHERE sc_id='%q'", script_id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_script WHERE sc_id='%q'", script_id);
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      free(sql_query);
      return 1;
    } else {
      log_message(LOG_INFO, "Error deleting script");
      free(sql_query);
      return 0;
    }
  } else {
    log_message(LOG_INFO, "Error deleting action_script");
    free(sql_query);
    return 0;
  }
}

/**
 * Add a schedule into the database
 */
int add_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  if (0 == strcmp(cur_schedule.name, "") || (cur_schedule.next_time == 0 && cur_schedule.repeat_schedule == -1) || (cur_schedule.repeat_schedule > -1 && cur_schedule.repeat_schedule_value == 0) || cur_schedule.script == 0) {log_message(LOG_INFO, "Error inserting schedule, wrong params"); return 0;}
  
  sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_scheduler (sh_name, sh_enabled, sh_next_time, sh_repeat_schedule, sh_repeat_schedule_value, de_id, sc_id) VALUES ('%q', '%d', '%d', '%d', '%d', (SELECT de_id FROM an_device WHERE de_name='%q'), '%d')", cur_schedule.name, cur_schedule.enabled, cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.device, cur_schedule.script);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sprintf(sql_query, "SELECT last_insert_rowid()");
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query last_insert_rowid()");
      free(sql_query);
      sqlite3_finalize(stmt);
      return 0;
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        cur_schedule.id = sqlite3_column_int(stmt, 0);
        snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat_schedule\":%d,\"repeat_schedule_value\":%d,\"device\":\"%s\",\"script\":%d}", cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.device, cur_schedule.script);
        free(sql_query);
        sqlite3_finalize(stmt);
        return 1;
      } else {
        log_message(LOG_INFO, "Error getting last_insert_rowid()");
        free(sql_query);
        sqlite3_finalize(stmt);
        return 0;
      }
    }
  } else {
    log_message(LOG_INFO, "Error inserting action");
    free(sql_query);
    sqlite3_finalize(stmt);
    return 0;
  }
}

/**
 * Modifies the specified schedule
 */
int set_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result) {
  char * sql_query = NULL;
  
  if (cur_schedule.id == 0 || 0 == strcmp(cur_schedule.name, "") || (cur_schedule.next_time == 0 && cur_schedule.repeat_schedule == -1) || (cur_schedule.repeat_schedule > -1 && cur_schedule.repeat_schedule_value == 0) || cur_schedule.script == 0) {log_message(LOG_INFO, "Error updating schedule, wrong params"); return 0;}
  
  sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_scheduler SET sh_name='%q', sh_enabled='%d', sh_next_time='%d', sh_repeat_schedule='%d', sh_repeat_schedule_value='%d', de_id=(SELECT de_id FROM an_device WHERE de_name='%q'), sc_id='%d' WHERE sh_id='%d'", cur_schedule.name, cur_schedule.enabled, cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.device, cur_schedule.script, cur_schedule.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat_schedule\":%d,\"repeat_schedule_value\":%d,\"device\":\"%s\",\"script\":%d}", cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.device, cur_schedule.script);
    free(sql_query);
    return 1;
  } else {
    log_message(LOG_INFO, "Error updating action");
    free(sql_query);
    return 0;
  }
}

/**
 * Delete the specified script
 */
int delete_schedule(sqlite3 * sqlite3_db, char * schedule_id) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  int sql_result;
  
  if (schedule_id == NULL || 0 == strcmp("", schedule_id)) {log_message(LOG_INFO, "Error deleting schedule, wrong params"); return 0;}
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_scheduler where sh_id='%q'", schedule_id);
  sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
  free(sql_query);
  return ( sql_result == SQLITE_OK );
}

/**
 * Parse the get heater results
 */
int parse_heater(sqlite3 * sqlite3_db, char * device, char * heater_name, char * source, heater * cur_heater) {
  char * heatSet, * heatOn, * heatMaxValue, * saveptr;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  
  heatSet = strtok_r(source, ";", &saveptr);
  heatOn = strtok_r(NULL, ";", &saveptr);
  heatMaxValue = strtok_r(NULL, ";", &saveptr);
  if (heatSet == NULL || heatOn == NULL || heatMaxValue == NULL) {
    log_message(LOG_INFO, "Error parsing heater data");
    return 0;
  } else {
    sanitize_json_string(heater_name, cur_heater->name, WORDLENGTH);
    sanitize_json_string(device, cur_heater->device, WORDLENGTH);
    cur_heater->set = strcmp(heatSet,"1")==0?1:0;
    cur_heater->on = strcmp(heatOn,"1")==0?1:0;
    cur_heater->heat_max_value = strtof(heatMaxValue, NULL);
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT he_display, he_enabled, he_unit from an_heater where he_name='%q' and de_id in (select de_id from an_device where de_name='%q')", heater_name, device);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query");
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

/**
 * Save the heat status in the database for startup init
 */
int set_startup_heater_status(sqlite3 * sqlite3_db, char * device, char * heater_name, int heat_enabled, float max_heat_value) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  sqlite3_stmt *stmt;
  int sql_result, row_result, he_id;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT he_id FROM an_heater WHERE he_id=(SELECT he_id FROM an_heater WHERE he_name = '%q') AND de_id=(SELECT de_id FROM an_device where de_name='%q')", heater_name, device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(sql_query);
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      he_id = sqlite3_column_int(stmt, 0);
      sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_heater SET he_set='%d', he_max_heat_value='%.2f' WHERE he_id='%d'", heat_enabled, max_heat_value, he_id);
    } else {
      sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_heater (he_id, he_name, de_id, he_set, he_max_heat_value) VALUES ((SELECT he_id FROM an_heater WHERE he_name = '%q' AND de_id=(SELECT de_id FROM an_device where de_name='%q')), '%q', (SELECT de_id FROM an_device where de_name='%q'), '%d', '%2f')", heater_name, device, heater_name, device, heat_enabled, max_heat_value);
    }
    sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
    free(sql_query);
    sqlite3_finalize(stmt);
    return ( sql_result == SQLITE_OK );
  }
}

/**
 * Save the heat status in the database for startup init
 */
int set_startup_pin_status(sqlite3 * sqlite3_db, char * device, char * pin, int status) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_id FROM an_switch WHERE de_id in (SELECT de_id FROM an_device where de_name='%q') AND sw_name = 'PIN%q'", device, pin);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(sql_query);
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_switch SET sw_status='%d' WHERE sw_id='%d'", status, sqlite3_column_int(stmt, 0));
    } else {
      sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_switch (de_id, sw_name, sw_display, sw_status) VALUES ((SELECT de_id FROM an_device where de_name='%q'), 'PIN%q', 'PIN%q', '%d')", device, pin, pin, status);
    }
    sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
    free(sql_query);
    sqlite3_finalize(stmt);
    return ( sql_result == SQLITE_OK );
  }
}

int set_startup_pin_on(sqlite3 * sqlite3_db, device * cur_device) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char)), * log = malloc((MSGLENGTH+1)*sizeof(char)), pin_name[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, state_result=1;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_name FROM an_switch WHERE de_id in (SELECT de_id FROM an_device where de_name='%q') AND sw_status = '1'", cur_device->name);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(log);
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      snprintf(pin_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0)+3);
      if (set_switch_state(cur_device, pin_name, 1) != 1) {
        snprintf(log, MSGLENGTH, "Error setting pin %s on device %s", pin_name, cur_device->name);
        log_message(LOG_INFO, log);
        state_result = 0;
      }
      row_result = sqlite3_step(stmt);
    }
    free(log);
    sqlite3_finalize(stmt);
    return state_result;
  }
}

/**
 * Get the heat status in the database for startup init
 */ 
heater * get_startup_heater_status(sqlite3 * sqlite3_db, char * device) {
  char * sql_query = malloc((MSGLENGTH+1)*sizeof(char));
  heater * heaters = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, nb_heaters=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT he.he_id, he.he_name, de.de_name, he.he_enabled, he.he_set, he.he_max_heat_value FROM an_heater he LEFT OUTER JOIN an_device de on de.de_id = he.de_id WHERE he.de_id in (SELECT de_id FROM an_device where de_name='%q')", device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
      heaters[nb_heaters].id = sqlite3_column_int(stmt, 0);
      snprintf(heaters[nb_heaters].name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(heaters[nb_heaters].device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      heaters[nb_heaters].enabled = sqlite3_column_int(stmt, 3);
      heaters[nb_heaters].set = sqlite3_column_int(stmt, 4);
      heaters[nb_heaters].heat_max_value = (float)sqlite3_column_double(stmt, 5);
      row_result = sqlite3_step(stmt);
      nb_heaters++;
    }
    sqlite3_finalize(stmt);
    heaters = realloc(heaters, (nb_heaters+1)*sizeof(struct _heater));
    heaters[nb_heaters].id = -1;
  }
  return heaters;
}

/**
 * Initialize the device with the stored init values (heater and pins)
 */
int init_device_status(sqlite3 * sqlite3_db, device * cur_device) {
  heater * heaters;
  int heat_status=1, i=0;
  char output[WORDLENGTH+1];
  
  if (cur_device->enabled) {
    heaters = get_startup_heater_status(sqlite3_db, cur_device->name);
    for (i=0; heaters[i].id != -1; i++) {
      if (!set_heater(cur_device, heaters[i].name, heaters[i].set, heaters[i].heat_max_value, output)) {
        heat_status = 0;
      }
    }
    free(heaters);
    return (heat_status && set_startup_pin_on(sqlite3_db, cur_device));
  } else {
    return 1;
  }
}