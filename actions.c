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
 * All functions that are not related to hardware calls
 *
 */

#include "angharad.h"

static const char json_template_action_set_action[] = "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"switcher\":\"%s\",\"dimmer\":\"%s\",\"heater\":\"%s\",\"params\":\"%s\",\"tags\":%s}";
static const char json_template_action_add_action[] = "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"switcher\":\"%s\",\"dimmer\":\"%s\",\"heater\":\"%s\",\"params\":\"%s\",\"tags\":%s}";
static const char json_template_action_get_actions[] = "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"switcher\":\"%s\",\"dimmer\":\"%s\",\"heater\":\"%s\",\"params\":\"%s\",\"tags\":%s}";

/**
 * Get the different actions for a specific device or for all devices
 */
char * get_actions(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result, str_len;
  size_t str_len_t;
  char cur_name[WORDLENGTH+1]={0}, cur_device[WORDLENGTH+1]={0}, cur_switch[WORDLENGTH+1]={0}, cur_dimmer[WORDLENGTH+1]={0}, cur_heater[WORDLENGTH+1]={0},
        cur_params[WORDLENGTH+1]={0}, * tags = NULL, cur_action[WORDLENGTH+1], ** tags_array = NULL,
        * actions = malloc(2*sizeof(char)), * sql_query = NULL, * one_item = NULL;
  
  if (device == NULL) {
    sql_query = sqlite3_mprintf("SELECT ac.ac_id, ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, di.di_name, he.he_name,\
                      ac.ac_params FROM an_action ac LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id\
                      LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_dimmer di ON di.di_id = ac.di_id\
                      LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id");
  } else {
    sql_query = sqlite3_mprintf("SELECT ac.ac_id, ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, di.di_name, he.he_name,\
                      ac.ac_params FROM an_action ac LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id\
                      LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_dimmer di ON di.di_id = ac.di_id\
                      LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id WHERE ac.de_id IN (SELECT de_id FROM an_device WHERE de_name = '%q')",
                      device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (get_actions)");
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    row_result = sqlite3_step(stmt);
    strcpy(actions, "");
    while (row_result == SQLITE_ROW) {
      snprintf(cur_action, WORDLENGTH*sizeof(char), "%d", sqlite3_column_int(stmt, 0));
      strncpy(cur_name, (char*)sqlite3_column_text(stmt, 1)==NULL?"":(char*)sqlite3_column_text(stmt, 1), WORDLENGTH);
      strncpy(cur_device, (char*)sqlite3_column_text(stmt, 3)==NULL?"":(char*)sqlite3_column_text(stmt, 3), WORDLENGTH);
      strncpy(cur_switch, (char*)sqlite3_column_text(stmt, 4)==NULL?"":(char*)sqlite3_column_text(stmt, 4), WORDLENGTH);
      strncpy(cur_dimmer, (char*)sqlite3_column_text(stmt, 5)==NULL?"":(char*)sqlite3_column_text(stmt, 5), WORDLENGTH);
      strncpy(cur_heater, (char*)sqlite3_column_text(stmt, 6)==NULL?"":(char*)sqlite3_column_text(stmt, 6), WORDLENGTH);
      strncpy(cur_params, (char*)sqlite3_column_text(stmt, 7)==NULL?"":(char*)sqlite3_column_text(stmt, 7), WORDLENGTH);
      tags_array = get_tags(sqlite3_db, NULL, DATA_ACTION, cur_action);
      tags = build_json_tags(tags_array);
      str_len = snprintf(NULL, 0, json_template_action_get_actions,
        sqlite3_column_int(stmt, 0),
        cur_name,
        sqlite3_column_int(stmt, 2),
        cur_device,
        cur_switch,
        cur_dimmer,
        cur_heater,
        cur_params,
        tags);
      one_item = malloc((str_len+1)*sizeof(char));
      snprintf(one_item, (str_len+1)*sizeof(char), json_template_action_get_actions,
        sqlite3_column_int(stmt, 0),
        cur_name,
        sqlite3_column_int(stmt, 2),
        cur_device,
        cur_switch,
        cur_dimmer,
        cur_heater,
        cur_params,
        tags);
      
      str_len_t = strlen(actions)+strlen(one_item);
      if (strlen(actions) > 0) {
        actions = realloc(actions, str_len_t+2*sizeof(char));
        strcat(actions, ",");
      } else {
        actions = realloc(actions, str_len_t+sizeof(char));
      }
      strcat(actions, one_item);
      row_result = sqlite3_step(stmt);
      free(one_item);
      free(tags);
      free_tags(tags_array);
    }
    sqlite3_finalize(stmt);
    return actions;
  }
}

/**
 * Run the specified action, evaluate the result and return if the result is valid
 */
int run_action(action ac, device ** terminal, unsigned int nb_terminal, sqlite3 * sqlite3_db, char * script_path) {
  char tmp[WORDLENGTH+1] = {0}, message_log[MSGLENGTH+1] = {0}, str_system[MSGLENGTH+1] = {0};
  int heat_set, sleep_val, toggle_set;
  float heat_max_value;
  int dimmer_value;
  device * cur_terminal;
  FILE * command_stream;
  heater * cur_heater = NULL;
  
  switch (ac.type) {
    case ACTION_SET_SWITCH:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: set_switch_state %s %s %s", ac.device, ac.switcher, ac.params);
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal != NULL && cur_terminal->enabled) {
          if (set_switch_state(cur_terminal, ac.switcher, (0 == strcmp(ac.params, "1"))) != ERROR_SWITCH) {
            if (!save_startup_switch_status(sqlite3_db, cur_terminal->name, ac.switcher, (0 == strcmp(ac.params, "1")))) {
              log_message(LOG_LEVEL_WARNING, "Error saving switcher status in the database");
            }
            if (!monitor_store(sqlite3_db, cur_terminal->name, ac.switcher, "", "", "", ac.params)) {
              log_message(LOG_LEVEL_WARNING, "Error monitoring switcher");
            }
          } else {
            log_message(LOG_LEVEL_WARNING, "Error setting switcher status");
          }
        }
      }
      break;
    case ACTION_TOGGLE_SWITCH:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: toggle_switch_state %s %s", ac.device, ac.switcher);
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal != NULL && cur_terminal->enabled) {
          toggle_set = toggle_switch_state(cur_terminal, ac.switcher);
          if (toggle_set != ERROR_SWITCH) {
            if (!save_startup_switch_status(sqlite3_db, cur_terminal->name, ac.switcher, toggle_set)) {
              log_message(LOG_LEVEL_WARNING, "Error saving switcher status in the database");
            }
            if (!monitor_store(sqlite3_db, cur_terminal->name, ac.switcher, "", "", "", (toggle_set==1?"1":"0"))) {
              log_message(LOG_LEVEL_WARNING, "Error monitoring switcher");
            }
          } else {
            log_message(LOG_LEVEL_WARNING, "Error setting switcher status");
          }
        }
      }
      break;
    case ACTION_DIMMER:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: set_dimmer_value %s %s %s", ac.device, ac.dimmer, ac.params);
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal != NULL && cur_terminal->enabled) {
          dimmer_value = strtol(ac.params, NULL, 10);
          if (set_dimmer_value(cur_terminal, ac.dimmer, dimmer_value) != ERROR_DIMMER) {
            if (!save_startup_dimmer_value(sqlite3_db, cur_terminal->name, ac.dimmer, dimmer_value)) {
              log_message(LOG_LEVEL_WARNING, "Error saving dimmer status in the database");
            }
            if (!monitor_store(sqlite3_db, cur_terminal->name, "", "", ac.dimmer, "", ac.params)) {
              log_message(LOG_LEVEL_WARNING, "Error monitoring dimmer");
            }
          } else {
            log_message(LOG_LEVEL_WARNING, "Error setting dimmer value");
          }
        }
      }
      break;
    case ACTION_HEATER:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: set_heater %s %s %s", ac.device, ac.heater, ac.params);
      if (ac.device != NULL && ac.heater != NULL && ac.params != NULL) {
        heat_set = ac.params[0]=='1'?1:0;
        heat_max_value = strtof(ac.params+2, NULL);
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        cur_heater = set_heater(sqlite3_db, cur_terminal, ac.heater, heat_set, heat_max_value);
        if (cur_heater != NULL) {
          if (cur_terminal != NULL && cur_terminal->enabled && cur_heater != NULL) {
            if (!save_startup_heater_status(sqlite3_db, cur_terminal->name, ac.heater, heat_set, heat_max_value)) {
              log_message(LOG_LEVEL_WARNING, "Error saving heater status in the database");
            }
            if (!monitor_store(sqlite3_db, cur_terminal->name, "", ac.heater, "", "", (heat_set?"0.0":ac.params))) {
              log_message(LOG_LEVEL_WARNING, "Error monitoring heater");
            }
          }
          free(cur_heater);
        } else {
          log_message(LOG_LEVEL_WARNING, "Error setting heater");
        }
      }
      break;
    case ACTION_SCRIPT:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: run script %s", ac.params);
      if (strlen(ac.params) > 0) {
        if (!run_script(sqlite3_db, terminal, nb_terminal, script_path, ac.params)) {
          return 0;
        }
      }
      break;
    case ACTION_SLEEP:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: sleep %s", ac.params);
      sleep_val = strtol(ac.params, NULL, 10);
      if (sleep_val > 0) {
        usleep(sleep_val*1000);
      }
    case ACTION_SYSTEM:
      snprintf(message_log, MSGLENGTH*sizeof(char), "run_action: system %s", ac.params);
      snprintf(str_system, MSGLENGTH*sizeof(char), "%s/%s", script_path, ac.params);
      command_stream = popen(str_system, "r");
      if (command_stream == NULL) {
        log_message(LOG_LEVEL_WARNING, "unable to run command %s", ac.params);
        return 0;
      } else {
        log_message(LOG_LEVEL_INFO, "Begin command result");
        while (fgets(tmp, WORDLENGTH*sizeof(char), command_stream) != NULL) {
          log_message(LOG_LEVEL_INFO, tmp);
        }
        log_message(LOG_LEVEL_INFO, "End command result");
      }
      pclose(command_stream);
      break;
    default:
      break;
  }
  log_message(LOG_LEVEL_INFO, "run_action \"%s\" (id:%d, type:%d)", ac.name, ac.id, ac.type);
  return 1;
}

/**
 * Add an action into the database
 */
char * add_action(sqlite3 * sqlite3_db, action cur_action) {
  char * sql_query, str_id[WORDLENGTH+1], device[WORDLENGTH+1], switcher[WORDLENGTH+1], dimmer[WORDLENGTH+1], sensor[WORDLENGTH+1], heater[WORDLENGTH+1], params[MSGLENGTH+1], * to_return = NULL, ** tags = NULL, * tags_json = NULL;
  int tr_len;
  
  // Verify input data
  if (0 == strcmp(cur_action.name, "")) {log_message(LOG_LEVEL_WARNING, "Error inserting action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_SET_SWITCH && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.switcher, "")) ||
      0 == strcmp(cur_action.params, ""))) {log_message(LOG_LEVEL_WARNING, "Error inserting action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_TOGGLE_SWITCH && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.switcher, ""))) {log_message(LOG_LEVEL_WARNING, "Error inserting action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_DIMMER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.dimmer, "") ||
      (0 == strcmp(cur_action.params, "")))) {log_message(LOG_LEVEL_WARNING, "Error inserting action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_HEATER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.heater, "") ||
      (0 == strcmp(cur_action.params, "")))) {log_message(LOG_LEVEL_WARNING, "Error inserting action, wrong params"); return NULL;}
  if ((cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) &&
      0 == strcmp(cur_action.params, "")) {log_message(LOG_LEVEL_WARNING, "Error inserting action, wrong params"); return NULL;}
  
  if (cur_action.type == ACTION_SET_SWITCH) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH*sizeof(char), "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_TOGGLE_SWITCH) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH*sizeof(char), "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    strcpy(params, "");
  }
  
  if (cur_action.type == ACTION_DIMMER) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    strcpy(switcher, "");
    snprintf(dimmer, WORDLENGTH*sizeof(char), "%s", cur_action.dimmer);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_HEATER) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    snprintf(heater, WORDLENGTH*sizeof(char), "%s", cur_action.heater);
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) {
    strcpy(device, "");
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  sql_query = sqlite3_mprintf("INSERT INTO an_action (ac_name, ac_type, de_id, sw_id, di_id, se_id, he_id, ac_params)\
                    VALUES ('%q', '%d', (SELECT de_id FROM an_device WHERE de_name='%q'),\
                    (SELECT sw_id FROM an_switch WHERE sw_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT di_id FROM an_dimmer WHERE di_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT se_id FROM an_sensor WHERE se_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT he_id FROM an_heater WHERE he_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')), '%q')",
                    cur_action.name, cur_action.type, device, switcher, device, dimmer, device, sensor, device, heater, device, params);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    cur_action.id = (int)sqlite3_last_insert_rowid(sqlite3_db);
    snprintf(str_id, WORDLENGTH*sizeof(char), "%d", cur_action.id);
    tags = build_tags_from_list(cur_action.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_ACTION, str_id, tags);
    tr_len = snprintf(NULL, 0, json_template_action_add_action, cur_action.id, cur_action.name, cur_action.type, device, switcher, dimmer, sensor, params, tags_json);
    to_return = malloc((tr_len+1)*sizeof(char));
    snprintf(to_return, (tr_len+1)*sizeof(char), json_template_action_add_action, cur_action.id, cur_action.name, cur_action.type, device, switcher, dimmer, sensor, params, tags_json);
    free(tags_json);
    free_tags(tags);
  } else {
    log_message(LOG_LEVEL_WARNING, "Error inserting action");
  }
  sqlite3_free(sql_query);
  return to_return;
}

/**
 * Modifies the specified action
 */
char * set_action(sqlite3 * sqlite3_db, action cur_action) {
  char * sql_query, device[WORDLENGTH+1], switcher[WORDLENGTH+1], dimmer[WORDLENGTH+1], sensor[WORDLENGTH+1],
        heater[WORDLENGTH+1], params[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1], * to_return = NULL;
  int tr_len;
  
  // Verify input data
  if (0 == strcmp(cur_action.name, "")) {log_message(LOG_LEVEL_WARNING, "Error updating action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_SET_SWITCH && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.switcher, "")) ||
      0 == strcmp(cur_action.params, ""))) {log_message(LOG_LEVEL_WARNING, "Error updating action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_TOGGLE_SWITCH && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.switcher, ""))) {log_message(LOG_LEVEL_WARNING, "Error updating action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_DIMMER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.dimmer, "") ||
      (0 == strcmp(cur_action.params, "")))) {log_message(LOG_LEVEL_WARNING, "Error updating action, wrong params"); return NULL;}
  if (cur_action.type == ACTION_HEATER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.heater, "") ||
      (0 == strcmp(cur_action.params, "")))) {log_message(LOG_LEVEL_WARNING, "Error updating action, wrong params"); return NULL;}
  if ((cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) &&
      0 == strcmp(cur_action.params, "")) {log_message(LOG_LEVEL_WARNING, "Error updating action, wrong params"); return NULL;}
  
  if (cur_action.type == ACTION_SET_SWITCH) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH*sizeof(char), "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_TOGGLE_SWITCH) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH*sizeof(char), "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    strcpy(params, "");
  }
  
  if (cur_action.type == ACTION_DIMMER) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    strcpy(switcher, "");
    snprintf(dimmer, WORDLENGTH*sizeof(char), "%s", cur_action.dimmer);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_HEATER) {
    snprintf(device, WORDLENGTH*sizeof(char), "%s", cur_action.device);
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    snprintf(heater, WORDLENGTH*sizeof(char), "%s", cur_action.heater);
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) {
    strcpy(device, "");
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH*sizeof(char), "%s", cur_action.params);
  }
  sql_query = sqlite3_mprintf("UPDATE an_action SET ac_name='%q', ac_type='%d',\
                  de_id=(SELECT de_id FROM an_device WHERE de_name='%q'),\
                  sw_id=(SELECT sw_id FROM an_switch WHERE sw_name='%q'\
                  AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                  di_id=(SELECT di_id FROM an_dimmer WHERE di_name='%q'\
                  and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                  se_id=(SELECT se_id FROM an_sensor WHERE se_name='%q'\
                  and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                  he_id=(SELECT he_id FROM an_heater WHERE he_name='%q'\
                  and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                  ac_params='%q' WHERE ac_id='%d'", 
                  cur_action.name, cur_action.type, device, switcher, device, dimmer, device, sensor, 
                  device, heater, device, params, cur_action.id);

  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    snprintf(str_id, WORDLENGTH*sizeof(char), "%d", cur_action.id);
    tags = build_tags_from_list(cur_action.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_ACTION, str_id, tags);
    tr_len = snprintf(NULL, 0, json_template_action_set_action, cur_action.id, cur_action.name, cur_action.type, device, switcher, dimmer, sensor, params, tags_json);
    to_return = malloc((tr_len+1)*sizeof(char));
    snprintf(to_return, (tr_len+1)*sizeof(char), json_template_action_set_action, cur_action.id, cur_action.name, cur_action.type, device, switcher, dimmer, sensor, params, tags_json);
    free(tags_json);
    free_tags(tags);
  } else {
    log_message(LOG_LEVEL_WARNING, "Error updating action");
  }
  sqlite3_free(sql_query);
  return to_return;
}

/**
 * Delete the specified action
 */
int delete_action(sqlite3 * sqlite3_db, char * action_id) {
  char *sql_query1 = NULL, *sql_query2 = NULL, *sql_query3 = NULL, *sql_query4 = NULL;
  int result = 0;
  
  sql_query1 = sqlite3_mprintf("DELETE FROM an_tag_element WHERE ac_id='%q'", action_id);
  if ( sqlite3_exec(sqlite3_db, sql_query1, NULL, NULL, NULL) == SQLITE_OK ) {
    sql_query2 = sqlite3_mprintf("DELETE FROM an_tag WHERE ta_id NOT IN (SELECT DISTINCT (ta_id) FROM an_tag)");
    if ( sqlite3_exec(sqlite3_db, sql_query2, NULL, NULL, NULL) == SQLITE_OK ) {
      sql_query3 = sqlite3_mprintf("DELETE FROM an_action_script WHERE ac_id='%q'", action_id);
      if ( sqlite3_exec(sqlite3_db, sql_query3, NULL, NULL, NULL) == SQLITE_OK ) {
        sql_query4 = sqlite3_mprintf("DELETE FROM an_action WHERE ac_id='%q'", action_id);
        if ( sqlite3_exec(sqlite3_db, sql_query4, NULL, NULL, NULL) == SQLITE_OK ) {
          result = 1;
        } else {
          log_message(LOG_LEVEL_WARNING, "Error deleting action");
        }
      } else {
        log_message(LOG_LEVEL_WARNING, "Error deleting action_script");
      }
    } else {
      log_message(LOG_LEVEL_WARNING, "Error deleting tag");
    }
  } else {
    log_message(LOG_LEVEL_WARNING, "Error deleting tag_element");
  }
  sqlite3_free(sql_query1);
  sqlite3_free(sql_query2);
  sqlite3_free(sql_query3);
  sqlite3_free(sql_query4);
  return result;
}
