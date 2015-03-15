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

/**
 * Commands non dedicated to devices (actions, scripts, schedules, parsing, etc.)
 */

/**
 * Get the different actions for a specific device or for all devices
 */
char * get_actions(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char cur_name[WORDLENGTH+1]={0}, cur_device[WORDLENGTH+1]={0}, cur_switch[WORDLENGTH+1]={0}, cur_sensor[WORDLENGTH+1]={0}, cur_heater[WORDLENGTH+1]={0}, cur_params[WORDLENGTH+1]={0}, * tags = NULL, cur_action[WORDLENGTH+1], ** tags_array = NULL;
  char * actions = malloc(2*sizeof(char)), sql_query[MSGLENGTH+1], one_item[MSGLENGTH+1];
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ac.ac_id, ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, se.se_name, he.he_name, ac.ac_params FROM an_action ac LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_sensor se ON se.se_id = ac.se_id LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ac.ac_id, ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, se.se_name, he.he_name, ac.ac_params FROM an_action ac LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_sensor se ON se.se_id = ac.se_id LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id WHERE ac.de_id in (SELECT de_id FROM an_device where de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    row_result = sqlite3_step(stmt);
    strcpy(actions, "");
    while (row_result == SQLITE_ROW) {
      snprintf(cur_action, WORDLENGTH, "%d", sqlite3_column_int(stmt, 0));
      if (strlen(actions) > 0) {
        actions = realloc(actions, (strlen(actions)+2)*sizeof(char));
        strcat(actions, ",");
      }
      sanitize_json_string((char*)sqlite3_column_text(stmt, 1)==NULL?"":(char*)sqlite3_column_text(stmt, 1), cur_name, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 3)==NULL?"":(char*)sqlite3_column_text(stmt, 3), cur_device, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 4)==NULL?"":(char*)sqlite3_column_text(stmt, 4), cur_switch, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 5)==NULL?"":(char*)sqlite3_column_text(stmt, 5), cur_sensor, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 6)==NULL?"":(char*)sqlite3_column_text(stmt, 6), cur_heater, WORDLENGTH);
      sanitize_json_string((char*)sqlite3_column_text(stmt, 7)==NULL?"":(char*)sqlite3_column_text(stmt, 7), cur_params, WORDLENGTH);
      tags_array = get_tags(sqlite3_db, NULL, DATA_ACTION, cur_action);
      tags = build_json_tags(tags_array);
      snprintf(one_item, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"switcher\":\"%s\",\"sensor\":\"%s\",\"heater\":\"%s\",\"params\":\"%s\",\"tags\":",
        sqlite3_column_int(stmt, 0),
        cur_name,
        sqlite3_column_int(stmt, 2),
        cur_device,
        cur_switch,
        cur_sensor,
        cur_heater,
        cur_params);
      actions = realloc(actions, (strlen(actions)+strlen(one_item)+strlen(tags)+3)*sizeof(char));
      strcat(actions, one_item);
      strcat(actions, tags);
      strcat(actions, "}");
      row_result = sqlite3_step(stmt);
      free(tags);
      free_tags(tags_array);
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
  char str_id[WORDLENGTH+1], cur_name[WORDLENGTH+1], device_name[WORDLENGTH+1], cur_script[WORDLENGTH+1];
  int cur_id, cur_enabled;
  char * scripts = malloc(2*sizeof(char)), sql_query[MSGLENGTH+1], * one_item = NULL, * actions = NULL, * tags = NULL, ** tags_array = NULL;
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id WHERE de.de_id in (SELECT de_id FROM an_device where de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(scripts);
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    strcpy(scripts, "");
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      snprintf(cur_script, WORDLENGTH, "%d", sqlite3_column_int(stmt, 0));
      if (row_result == SQLITE_ROW) {
        if (strlen(scripts) > 0) {
          scripts = realloc(scripts, (strlen(scripts)+2)*sizeof(char));
          strcat(scripts, ",");
        }
        cur_id = sqlite3_column_int(stmt, 0);
        snprintf(str_id, WORDLENGTH, "%d", cur_id);
        snprintf(cur_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
        tags_array = get_tags(sqlite3_db, NULL, DATA_SCRIPT, str_id);
        tags = build_json_tags(tags_array);
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
        one_item = malloc((strlen(cur_name)+strlen(device_name)+strlen(actions)+67+num_digits(cur_id))*sizeof(char));
        sprintf(one_item, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"actions\":[%s],\"tags\":", cur_id, cur_name, cur_enabled?"true":"false", device_name, actions);
        scripts = realloc(scripts, (strlen(scripts)+strlen(one_item)+strlen(tags)+2)*sizeof(char));
        strcat(scripts, one_item);
        strcat(scripts, tags);
        strcat(scripts, "}");
        free(actions);
        free(one_item);
        free(tags);
        free_tags(tags_array);
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
  char sql_query[MSGLENGTH+1], ac_name[WORDLENGTH+1], * actions = malloc(sizeof(char)), one_item[MSGLENGTH+1];
  int rank, ac_id, enabled;
  
  if (script_id == 0) {
    log_message(LOG_INFO, "Error getting action scripts, script_id is 0");
    free(actions);
    return NULL;
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ac.ac_id, ac.ac_name, aas.as_rank, aas.as_enabled FROM an_action_script aas, an_action ac WHERE ac.ac_id=aas.ac_id AND aas.sc_id='%d' ORDER BY as_rank", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    free(actions);
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
        enabled = sqlite3_column_int(stmt, 3);
        sprintf(one_item, "{\"id\":%d,\"name\":\"%s\",\"rank\":%d,\"enabled\":%s}", ac_id, ac_name, rank, enabled?"true":"false");
        actions = realloc(actions, (strlen(actions)+strlen(one_item)+1)*sizeof(char));
        strcat(actions, one_item);
      } else if (row_result == SQLITE_DONE) {
        break;
      } else {
        free(actions);
        sqlite3_finalize(stmt);
        return NULL;
      }
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return actions;
  }
}

/**
 * get the selected script
 */
char * get_script(sqlite3 * sqlite3_db, char * script_id, int with_tags) {
  sqlite3_stmt *stmt;
  int sql_result, sc_id, sc_enabled, row_result;
  char sql_query[MSGLENGTH+1], sc_name[WORDLENGTH+1], device[WORDLENGTH+1], tmp[WORDLENGTH*3], * to_return = NULL, * tags = NULL, ** tags_array = NULL;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id WHERE sc.sc_id = '%q'", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sc_id = sqlite3_column_int(stmt, 0);
      snprintf(sc_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      sanitize_json_string(sc_name, sc_name, WORDLENGTH);
      sc_enabled = sqlite3_column_int(stmt, 2);
      if (sqlite3_column_text(stmt, 3) != NULL) {
        snprintf(device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 3));
      } else {
        strcpy(device, "");
      }
      if (with_tags) {
        tags_array = get_tags(sqlite3_db, NULL, DATA_SCRIPT, script_id);
        tags = build_json_tags(tags_array);
        to_return = malloc((55+strlen(sc_name)+strlen(device)+strlen(tags)+num_digits(sc_id))*sizeof(char));
        snprintf(to_return, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"tags\":%s}", sc_id, sc_name, sc_enabled?"true":"false", device, tags);
        free(tags);
        free_tags(tags_array);
      } else {
        to_return = malloc((46+strlen(sc_name)+strlen(device)+num_digits(sc_id))*sizeof(char));
        snprintf(to_return, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\"}", sc_id, sc_name, sc_enabled?"true":"false", device);
      }
      sqlite3_finalize(stmt);
      return to_return;
    } else {
      snprintf(tmp, MSGLENGTH, "Script %s not found", script_id);
      log_message(LOG_INFO, tmp);
      sqlite3_finalize(stmt);
      return NULL;
    }
  }
}

/**
 * Run a script by running its actions one by one
 * Each action should be ran if the precedent ran successfully,
 * and its result is consistent to the expected result
 */
int run_script(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, char * script_path, char * script_id) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char sql_query[MSGLENGTH+1];
  action ac;
    
  snprintf(sql_query, MSGLENGTH, "SELECT ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, di.di_name, he.he_name, ac.ac_params, ac.ac_id FROM an_action ac, an_action_script acs LEFT OUTER JOIN an_device de on de.de_id = ac.de_id LEFT OUTER JOIN an_switch sw on sw.sw_id = ac.sw_id LEFT OUTER JOIN an_dimmer di on di.di_id = ac.di_id LEFT OUTER JOIN an_heater he on he.he_id = ac.he_id WHERE ac.ac_id = acs.ac_id AND acs.sc_id = '%s' AND NOT acs.as_enabled = 0 ORDER BY acs.as_rank", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query %s", sql_query);
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        snprintf(ac.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
      } else {
        strcpy(ac.name, "");
      }
      if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
        ac.type = sqlite3_column_int(stmt, 1);
      } else {
        ac.type = ACTION_NONE;
      }
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        snprintf(ac.device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      } else {
        memset(ac.device, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
        snprintf(ac.switcher, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 3));
      } else {
        memset(ac.switcher, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        snprintf(ac.dimmer, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 4));
      } else {
        memset(ac.dimmer, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
        snprintf(ac.heater, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 5));
      } else {
        memset(ac.heater, 0, WORDLENGTH*sizeof(char));;
      }
      if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        snprintf(ac.params, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 6));
      } else {
        memset(ac.params, 0, MSGLENGTH*sizeof(char));;
      }
      ac.id = sqlite3_column_int(stmt, 7);
      if (!run_action(ac, terminal, nb_terminal, sqlite3_db, script_path)) {
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
int run_action(action ac, device ** terminal, unsigned int nb_terminal, sqlite3 * sqlite3_db, char * script_path) {
  char tmp[WORDLENGTH+1] = {0}, jo_command[MSGLENGTH+1] = {0}, message_log[MSGLENGTH+1] = {0}, str_system[MSGLENGTH+1] = {0};
  int heat_set, sleep_val, toggle_set;
  float heat_max_value;
  int dimmer_value;
  device * cur_terminal;
  FILE * command_stream;
  
  switch (ac.type) {
    case ACTION_SET_SWITCH:
      snprintf(message_log, MSGLENGTH, "run_action: set_switch_state %s %s %s", ac.device, ac.switcher, ac.params);
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled) {
          set_switch_state(cur_terminal, ac.switcher, (0 == strcmp(ac.params, "1")));
          if (!save_startup_switch_status(sqlite3_db, cur_terminal->name, ac.switcher, (0 == strcmp(ac.params, "1")))) {
            log_message(LOG_INFO, "Error saving switcher status in the database");
          }
        }
      }
      break;
    case ACTION_TOGGLE_SWITCH:
      snprintf(message_log, MSGLENGTH, "run_action: toggle_switch_state %s %s", ac.device, ac.switcher);
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled) {
          toggle_set = toggle_switch_state(cur_terminal, ac.switcher);
          if (!save_startup_switch_status(sqlite3_db, cur_terminal->name, ac.switcher, toggle_set)) {
            log_message(LOG_INFO, "Error saving switcher status in the database");
          }
        }
      }
      break;
    case ACTION_DIMMER:
      snprintf(message_log, MSGLENGTH, "run_action: set_dimmer_value %s %s %s", ac.device, ac.dimmer, ac.params);
      if (ac.device != NULL) {
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled) {
          dimmer_value = strtol(ac.params, NULL, 10);
          set_dimmer_value(cur_terminal, ac.dimmer, dimmer_value);
          if (!save_startup_dimmer_value(sqlite3_db, cur_terminal->name, ac.dimmer, dimmer_value)) {
            log_message(LOG_INFO, "Error saving dimmer status in the database");
          }
        }
      }
      break;
    case ACTION_HEATER:
      snprintf(message_log, MSGLENGTH, "run_action: set_heater %s %s %s", ac.device, ac.heater, ac.params);
      if (ac.device != NULL && ac.heater != NULL && ac.params != NULL) {
        heat_set = ac.params[0]=='1'?1:0;
        heat_max_value = strtof(ac.params+2, NULL);
        cur_terminal = get_device_from_name(ac.device, terminal, nb_terminal);
        if (cur_terminal->enabled && set_heater(cur_terminal, ac.heater, heat_set, heat_max_value, tmp)) {
          if (!save_startup_heater_status(sqlite3_db, cur_terminal->name, ac.heater, heat_set, heat_max_value)) {
            log_message(LOG_INFO, "Error saving heater status in the database");
          }
        }
      }
      break;
    case ACTION_SCRIPT:
      snprintf(message_log, MSGLENGTH, "run_action: run script %s", ac.params);
      if (strlen(ac.params) > 0) {
        if (!run_script(sqlite3_db, terminal, nb_terminal, script_path, ac.params)) {
          return 0;
        }
      }
      break;
    case ACTION_SLEEP:
      snprintf(message_log, MSGLENGTH, "run_action: sleep %s", ac.params);
      sleep_val = strtol(ac.params, NULL, 10);
      if (sleep_val > 0) {
        usleep(sleep_val*1000);
      }
    case ACTION_SYSTEM:
      snprintf(message_log, MSGLENGTH, "run_action: system %s", ac.params);
      snprintf(str_system, MSGLENGTH, "%s/%s", script_path, ac.params);
      command_stream = popen(str_system, "r");
      if (command_stream == NULL) {
        snprintf(message_log, MSGLENGTH, "unable to run command %s", ac.params);
        return 0;
      } else {
        log_message(LOG_INFO, "Begin command result");
        while (fgets(tmp, WORDLENGTH, command_stream) != NULL) {
          log_message(LOG_INFO, tmp);
        }
        log_message(LOG_INFO, "End command result");
      }
      pclose(command_stream);
      break;
    default:
      break;
  }
  snprintf(jo_command, MSGLENGTH, "run_action \"%s\" (id:%d, type:%d)", ac.name, ac.id, ac.type);
  journal(sqlite3_db, "run_script", jo_command, "");
  log_message(LOG_INFO, message_log);
  return 1;
}

/**
 * Get all the schedules for the specified device
 * Or all the schedules if device is NULL
 */
char * get_schedules(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char sql_query[MSGLENGTH+1], * one_item = NULL, cur_name[WORDLENGTH+1], cur_device[WORDLENGTH+1], cur_schedule[WORDLENGTH+1], * tags = NULL, ** tags_array = NULL;
  int cur_id, remove_after_done = 0;
  long next_time;
  char script_id[WORDLENGTH+1], * scripts = malloc(2*sizeof(char)), * script = NULL;
  int enabled, repeat_schedule, repeat_schedule_value;
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh.sh_id, sh.sh_name, sh.sh_enabled, sh.sh_next_time, sh.sh_repeat_schedule, sh.sh_repeat_schedule_value, sh.sc_id, de.de_name, sh.sh_remove_after_done FROM an_scheduler sh LEFT OUTER JOIN an_device de ON de.de_id = sh.de_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh.sh_id, sh.sh_name, sh.sh_enabled, sh.sh_next_time, sh.sh_repeat_schedule, sh.sh_repeat_schedule_value, sh.sc_id, de.de_name, sh.sh_remove_after_done FROM an_scheduler sh LEFT OUTER JOIN an_device de ON de.de_id = sh.de_id WHERE sh.de_id in (SELECT de_id FROM an_device where de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
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
      snprintf(cur_schedule, WORDLENGTH, "%d", cur_id);
      tags_array = get_tags(sqlite3_db, NULL, DATA_SCHEDULE, cur_schedule);
      tags = build_json_tags(tags_array);
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
      remove_after_done = sqlite3_column_int(stmt, 8);
      sanitize_json_string(cur_device, cur_device, WORDLENGTH);
      script = get_script(sqlite3_db, script_id, 0);
      if (script == NULL) {
        script = malloc(3*sizeof(char));
        strcpy(script, "{}");
      }
      sanitize_json_string(cur_name, cur_name, WORDLENGTH);
      one_item = malloc((124+num_digits(cur_id)+strlen(cur_name)+num_digits(cur_id)+strlen(cur_device)+num_digits_l(next_time)+num_digits(repeat_schedule)+num_digits(repeat_schedule_value)+num_digits(remove_after_done)+strlen(tags)+strlen(script))*sizeof(char));
      sprintf(one_item, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"next_time\":%ld,\"repeat\":%d,\"repeat_value\":%d,\"remove_after_done\":%d,\"tags\":%s,\"script\":%s}", cur_id, cur_name, enabled?"true":"false", cur_device, next_time, repeat_schedule, repeat_schedule_value, remove_after_done, tags, script);
      scripts = realloc(scripts, (strlen(scripts)+strlen(one_item)+1)*sizeof(char));
      strcat(scripts, one_item);
      free(one_item);
      free(script);
      free(tags);
      free_tags(tags_array);
      one_item = NULL;
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return scripts;
  }
}

/**
 * Change the state of a schedule
 */
int enable_schedule(sqlite3 * sqlite3_db, char * schedule, char * status, char * command_result) {
  char sql_query[MSGLENGTH+1], * script = NULL, script_id[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  struct _schedule cur_schedule;

  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_scheduler SET sh_enabled='%q' WHERE sh_id='%q'", status, schedule);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh_id, sh_name, sh_enabled, sh_next_time, sh_repeat_schedule, sh_repeat_schedule_value, sc_id FROM an_scheduler WHERE sh_id='%q'", schedule);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
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
        script = get_script(sqlite3_db, script_id, 0);
        if (script == NULL) {
          script = malloc(3*sizeof(char));
          strcpy(script, "{}");
        }
        sanitize_json_string(cur_schedule.name, cur_schedule.name, WORDLENGTH);
        snprintf(command_result, MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat\":%d,\"repeat_value\":%d,\"script\":%s}", cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, script);
        sqlite3_finalize(stmt);
        free(script);
        return 1;
      } else {
        log_message(LOG_INFO, "Error getting schedule data");
        sqlite3_finalize(stmt);
        return 0;
      }
    }
  } else {
    log_message(LOG_INFO, "Error updating schedule");
    sqlite3_finalize(stmt);
    return 0;
  }
}

/**
 * Change the display name and the enable settings for a device
 */
char * set_device_data(sqlite3 * sqlite3_db, device cur_device) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, * to_return = NULL;
  int str_len=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_device (de_id, de_name, de_display, de_active) VALUES ((SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%d')", cur_device.name, cur_device.name, cur_device.display, cur_device.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_device.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_DEVICE, cur_device.name, tags);
    sanitize_json_string(cur_device.name, cur_device.name, WORDLENGTH);
    sanitize_json_string(cur_device.display, cur_device.display, WORDLENGTH);
    str_len = (95+strlen(cur_device.name)+strlen(cur_device.display)+strlen(tags_json));
    to_return = malloc((str_len+1)*sizeof(char));
    snprintf(to_return, str_len, "{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"tags\":%s}", cur_device.name, cur_device.display, cur_device.enabled?"true":"false", tags_json);
    free(tags_json);
    free_tags(tags);
  }
  return to_return;
}

/**
 * Change the display name, the type and the enable settings for a device
 */
char * set_switch_data(sqlite3 * sqlite3_db, switcher cur_switch) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, * to_return = NULL;
  int str_len=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_switch (sw_id, de_id, sw_name, sw_display, sw_type, sw_active, sw_status, sw_monitored, sw_monitored_every, sw_monitored_next) VALUES ((SELECT sw_id FROM an_switch where sw_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%d', '%d', (SELECT sw_status FROM an_switch where sw_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), '%d', '%d', 0)", cur_switch.name, cur_switch.device, cur_switch.device, cur_switch.name, cur_switch.display, cur_switch.type, cur_switch.enabled, cur_switch.name, cur_switch.device, cur_switch.monitored, cur_switch.monitored_every);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_switch.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_switch.device, DATA_SWITCH, cur_switch.name, tags);
    sanitize_json_string(cur_switch.name, cur_switch.name, WORDLENGTH);
    sanitize_json_string(cur_switch.display, cur_switch.display, WORDLENGTH);
    str_len = 59+strlen(cur_switch.name)+strlen(cur_switch.display)+strlen(tags_json);
    to_return = malloc(str_len+1*sizeof(char));
    snprintf(to_return, str_len, "{\"name\":\"%s\",\"display\":\"%s\",\"type\":%d,\"enabled\":%s,\"tags\":%s}", cur_switch.name, cur_switch.display, cur_switch.type, cur_switch.enabled?"true":"false", tags_json);
    free(tags_json);
    free_tags(tags);
  }
  return to_return;
}

/**
 * Change the display name and the enable settings for a device
 */
char * set_sensor_data(sqlite3 * sqlite3_db, sensor cur_sensor) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, * to_return = NULL;
  int str_len=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_sensor (se_id, de_id, se_name, se_display, se_unit, se_active, se_monitored, se_monitored_every, se_monitored_next) VALUES ((SELECT se_id FROM an_sensor where se_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%q', '%d', '%d', '%d', 0)", cur_sensor.name, cur_sensor.device, cur_sensor.device, cur_sensor.name, cur_sensor.display, cur_sensor.unit, cur_sensor.enabled, cur_sensor.monitored, cur_sensor.monitored_every);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_sensor.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_sensor.device, DATA_SENSOR, cur_sensor.name, tags);
    sanitize_json_string(cur_sensor.name, cur_sensor.name, WORDLENGTH);
    sanitize_json_string(cur_sensor.display, cur_sensor.display, WORDLENGTH);
    sanitize_json_string(cur_sensor.unit, cur_sensor.unit, WORDLENGTH);
    str_len = 60+strlen(cur_sensor.name)+strlen(cur_sensor.display)+strlen(cur_sensor.unit)+strlen(tags_json);
    to_return = malloc(str_len+1*sizeof(char));
    snprintf(to_return, str_len, "{\"name\":\"%s\",\"display\":\"%s\",\"unit\":\"%s\",\"enabled\":%s,\"tags\":%s}", cur_sensor.name, cur_sensor.display, cur_sensor.unit, cur_sensor.enabled?"true":"false", tags_json);
    free(tags_json);
    free_tags(tags);
  }
  return to_return;
}

/**
 * Change the display name, the unit and the enable settings for a heater
 */
char * set_heater_data(sqlite3 * sqlite3_db, heater cur_heater) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, * to_return = NULL;
  int str_len=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_heater (he_id, de_id, he_name, he_display, he_unit, he_enabled) VALUES ((SELECT he_id FROM an_heater where he_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%q', '%d')", cur_heater.name, cur_heater.device, cur_heater.device, cur_heater.name, cur_heater.display, cur_heater.unit, cur_heater.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_heater.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_heater.device, DATA_HEATER, cur_heater.name, tags);
    sanitize_json_string(cur_heater.name, cur_heater.name, WORDLENGTH);
    sanitize_json_string(cur_heater.display, cur_heater.display, WORDLENGTH);
    sanitize_json_string(cur_heater.unit, cur_heater.unit, WORDLENGTH);
    str_len = 60+strlen(cur_heater.name)+strlen(cur_heater.display)+strlen(cur_heater.unit)+strlen(tags_json);
    to_return = malloc(str_len+1*sizeof(char));
    snprintf(to_return, MSGLENGTH, "{\"name\":\"%s\",\"display\":\"%s\",\"unit\":\"%s\",\"enabled\":%s,\"tags\":%s}", cur_heater.name, cur_heater.display, cur_heater.unit, cur_heater.enabled?"true":"false", tags_json);
    free(tags_json);
    free_tags(tags);
  }
  return to_return;
}

/**
 * Change the display name and the enable settings for a dimmer
 */
char * set_dimmer_data(sqlite3 * sqlite3_db, dimmer cur_dimmer) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, * to_return = NULL;
  int str_len=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_dimmer (di_id, de_id, di_name, di_display, di_active) VALUES ((SELECT di_id FROM an_dimmer where di_name='%q' and de_id in (SELECT de_id FROM an_device where de_name='%q')), (SELECT de_id FROM an_device where de_name='%q'), '%q', '%q', '%d')", cur_dimmer.name, cur_dimmer.device, cur_dimmer.device, cur_dimmer.name, cur_dimmer.display, cur_dimmer.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_dimmer.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_dimmer.device, DATA_DIMMER, cur_dimmer.name, tags);
    sanitize_json_string(cur_dimmer.name, cur_dimmer.name, WORDLENGTH);
    sanitize_json_string(cur_dimmer.display, cur_dimmer.display, WORDLENGTH);
    str_len = 50+strlen(cur_dimmer.name)+strlen(cur_dimmer.display)+strlen(tags_json);
    to_return = malloc(str_len+1*sizeof(char));
    snprintf(to_return, str_len, "{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"tags\":%s}", cur_dimmer.name, cur_dimmer.display, cur_dimmer.enabled?"true":"false", tags_json);
    free(tags_json);
    free_tags(tags);
  }
  return to_return;
}

/**
 * Add an action into the database
 */
int add_action(sqlite3 * sqlite3_db, action cur_action, char * command_result) {
  char sql_query[MSGLENGTH+1], str_id[WORDLENGTH+1], device[WORDLENGTH+1], switcher[WORDLENGTH+1], dimmer[WORDLENGTH+1], sensor[WORDLENGTH+1], heater[WORDLENGTH+1], params[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL;
  
  // Verify input data
  if (0 == strcmp(cur_action.name, "")) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SET_SWITCH && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.switcher, "")) || 0 == strcmp(cur_action.params, ""))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_TOGGLE_SWITCH && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.switcher, ""))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_DIMMER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.dimmer, "") || (0 == strcmp(cur_action.params, "")))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if (cur_action.type == ACTION_HEATER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.heater, "") || (0 == strcmp(cur_action.params, "")))) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  if ((cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) && 0 == strcmp(cur_action.params, "")) {log_message(LOG_INFO, "Error inserting action, wrong params"); return 0;}
  
  if (cur_action.type == ACTION_SET_SWITCH) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH, "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_TOGGLE_SWITCH) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH, "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    strcpy(params, "");
  }
  
  if (cur_action.type == ACTION_DIMMER) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(switcher, "");
    snprintf(dimmer, WORDLENGTH, "%s", cur_action.dimmer);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_HEATER) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    snprintf(heater, WORDLENGTH, "%s", cur_action.heater);
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) {
    strcpy(device, "");
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_action (ac_name, ac_type, de_id, sw_id, di_id, se_id, he_id, ac_params) VALUES ('%q', '%d', (select de_id from an_device where de_name='%q'), (select sw_id from an_switch where sw_name='%q' and de_id in (select de_id from an_device where de_name='%q')), (select di_id from an_dimmer where di_name='%q' and de_id in (select de_id from an_device where de_name='%q')), (select se_id from an_sensor where se_name='%q' and de_id in (select de_id from an_device where de_name='%q')), (select he_id from an_heater where he_name='%q' and de_id in (select de_id from an_device where de_name='%q')), '%q')", cur_action.name, cur_action.type, device, switcher, device, dimmer, device, sensor, device, heater, device, params);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    cur_action.id = (int)sqlite3_last_insert_rowid(sqlite3_db);
    snprintf(str_id, WORDLENGTH, "%d", cur_action.id);
    tags = build_tags_from_list(cur_action.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_ACTION, str_id, tags);
    snprintf(command_result, 2*MSGLENGTH, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"switcher\":\"%s\",\"dimmer\":\"%s\",\"sensor\":\"%s\",\"params\":\"%s\",\"tags\":%s}", cur_action.id, cur_action.name, cur_action.type, device, switcher, dimmer, sensor, params, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_INFO, "Error inserting action");
    return 0;
  }
}

/**
 * Modifies the specified action
 */
int set_action(sqlite3 * sqlite3_db, action cur_action, char * command_result) {
  char sql_query[MSGLENGTH+1], device[WORDLENGTH+1], switcher[WORDLENGTH+1], dimmer[WORDLENGTH+1], sensor[WORDLENGTH+1], heater[WORDLENGTH+1], params[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  
  // Verify input data
  if (0 == strcmp(cur_action.name, "")) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_SET_SWITCH && (0 == strcmp(cur_action.device, "") || (0 == strcmp(cur_action.switcher, "")) || 0 == strcmp(cur_action.params, ""))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_TOGGLE_SWITCH && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.switcher, ""))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_DIMMER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.dimmer, "") || (0 == strcmp(cur_action.params, "")))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if (cur_action.type == ACTION_HEATER && (0 == strcmp(cur_action.device, "") || 0 == strcmp(cur_action.heater, "") || (0 == strcmp(cur_action.params, "")))) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  if ((cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) && 0 == strcmp(cur_action.params, "")) {log_message(LOG_INFO, "Error updating action, wrong params"); return 0;}
  
  if (cur_action.type == ACTION_SET_SWITCH) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH, "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_TOGGLE_SWITCH) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    snprintf(switcher, WORDLENGTH, "%s", cur_action.switcher);
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    strcpy(params, "");
  }
  
  if (cur_action.type == ACTION_DIMMER) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(switcher, "");
    snprintf(dimmer, WORDLENGTH, "%s", cur_action.dimmer);
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_HEATER) {
    snprintf(device, WORDLENGTH, "%s", cur_action.device);
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    snprintf(heater, WORDLENGTH, "%s", cur_action.heater);
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  if (cur_action.type == ACTION_SYSTEM || cur_action.type == ACTION_SLEEP || cur_action.type == ACTION_SCRIPT) {
    strcpy(device, "");
    strcpy(switcher, "");
    strcpy(dimmer, "");
    strcpy(sensor, "");
    strcpy(heater, "");
    snprintf(params, WORDLENGTH, "%s", cur_action.params);
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_action SET ac_name='%q', ac_type='%d', de_id=(select de_id from an_device where de_name='%q'), sw_id=(select sw_id from an_switch where sw_name='%q' and de_id in (select de_id from an_device where de_name='%q')), di_id=(select di_id from an_dimmer where di_name='%q' and de_id in (select de_id from an_device where de_name='%q')), se_id=(select se_id from an_sensor where se_name='%q' and de_id in (select de_id from an_device where de_name='%q')), he_id=(select he_id from an_heater where he_name='%q' and de_id in (select de_id from an_device where de_name='%q')), ac_params='%q' WHERE ac_id='%d'", cur_action.name, cur_action.type, device, switcher, device, dimmer, device, sensor, device, heater, device, params, cur_action.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    snprintf(str_id, WORDLENGTH, "%d", cur_action.id);
    tags = build_tags_from_list(cur_action.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_ACTION, str_id, tags);
    snprintf(command_result, MSGLENGTH*2, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"device\":\"%s\",\"switcher\":\"%s\",\"dimmer\":\"%s\",\"sensor\":\"%s\",\"params\":\"%s\",\"tags\":%s}", cur_action.id, cur_action.name, cur_action.type, device, switcher, dimmer, sensor, params, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_INFO, "Error updating action");
    return 0;
  }
}

/**
 * Delete the specified action
 */
int delete_action(sqlite3 * sqlite3_db, char * action_id) {
  char sql_query[MSGLENGTH+1];
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag_element WHERE ac_id='%q'", action_id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag WHERE ta_id NOT IN (SELECT DISTINCT (ta_id) FROM an_tag)");
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action_script WHERE ac_id='%q'", action_id);
      if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action WHERE ac_id='%q'", action_id);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          return 1;
        } else {
          log_message(LOG_INFO, "Error deleting action");
          return 0;
        }
      } else {
        log_message(LOG_INFO, "Error deleting action_script");
        return 0;
      }
    } else {
      log_message(LOG_INFO, "Error deleting tag");
      return 0;
    }
  } else {
    log_message(LOG_INFO, "Error deleting tag_element");
    return 0;
  }
}

/**
 * Add a script into the database
 */
int add_script(sqlite3 * sqlite3_db, script cur_script, char * command_result) {
  char sql_query[MSGLENGTH+1], tmp[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  int rank=0;
  
  char * action, * saveptr, * action_id, * enabled, * saveptr2;
  
  if (0 == strcmp("", cur_script.name)) {log_message(LOG_INFO, "Error inserting script, wrong params"); return 0;}
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_script (sc_name, de_id, sc_enabled) VALUES ('%q', (SELECT de_id FROM an_device where de_name='%q'), '%d')", cur_script.name, cur_script.device, cur_script.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    cur_script.id = (int)sqlite3_last_insert_rowid(sqlite3_db);
        
    // Parsing actions, then insert into an_action_script
    action = strtok_r(cur_script.actions, ";", &saveptr);
    while (action != NULL) {
      action_id = strtok_r(action, ",", &saveptr2);
      enabled = strtok_r(NULL, ",", &saveptr2);
      if (action_id != NULL && enabled != NULL) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_action_script (sc_id, ac_id, as_enabled, as_rank) VALUES ('%d', '%q', '%s', '%d')", cur_script.id, action, enabled, rank++);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          snprintf(tmp, MSGLENGTH, "Error inserting action (%d, %s, %d)", cur_script.id, action, rank++);
          log_message(LOG_INFO, tmp);
        }
      } else {
        log_message(LOG_INFO, "Error inserting action list, wrong parameters");
      }
      action = strtok_r(NULL, ";", &saveptr);
    }
    snprintf(str_id, WORDLENGTH, "%d", cur_script.id);
    tags = build_tags_from_list(cur_script.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCRIPT, str_id, tags);
    snprintf(command_result, MSGLENGTH*2, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"tags\":%s}", cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_INFO, "Error inserting script");
    return 0;
  }
}

/**
 * Modifies the specified script
 */
int set_script(sqlite3 * sqlite3_db, script cur_script, char * command_result) {
  char sql_query[MSGLENGTH+1], tmp[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  int rank=0;
  
  char * action_token, * action_id, * enabled, * saveptr, * saveptr2;
  
  if (0 == strcmp("", cur_script.name)) {log_message(LOG_INFO, "Error updating script, wrong params"); return 0;}
  if (cur_script.id == 0) {log_message(LOG_INFO, "Error updating script, wrong params"); return 0;}
  
  // Reinit action_script list
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action_script WHERE sc_id='%d'", cur_script.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
    log_message(LOG_INFO, "Error updating script, wrong params");
    return 0;
  }
  sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_script SET sc_name='%q', de_id=(SELECT de_id FROM an_device where de_name='%q'), sc_enabled='%d' WHERE sc_id='%d'", cur_script.name, cur_script.device, cur_script.enabled, cur_script.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    // Parsing actions, then insert into an_action_script
    action_token = strtok_r(cur_script.actions, ";", &saveptr);
    while (action_token != NULL) {
      action_id = strtok_r(action_token, ",", &saveptr2);
      enabled = strtok_r(NULL, ",", &saveptr2);
      if (action_id != NULL && enabled != NULL) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_action_script (sc_id, ac_id, as_rank, as_enabled) VALUES ('%d', '%q', '%d', '%s')", cur_script.id, action_id, rank++, enabled);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          snprintf(tmp, MSGLENGTH, "Error updating action (%d, %s, %d)", cur_script.id, action_token, rank++);
          log_message(LOG_INFO, tmp);
        }
      } else {
        log_message(LOG_INFO, "Error updating action list, wrong parameters");
      }
      action_token = strtok_r(NULL, ";", &saveptr);
    }
    snprintf(str_id, WORDLENGTH, "%d", cur_script.id);
    tags = build_tags_from_list(cur_script.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCRIPT, str_id, tags);
    snprintf(command_result, MSGLENGTH*2, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"tags\":%s}", cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_INFO, "Error updating script");
    return 0;
  }
}

/**
 * Delete the specified script
 */
int delete_script(sqlite3 * sqlite3_db, char * script_id) {
  char sql_query[MSGLENGTH+1];
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag_element WHERE sc_id='%q'", script_id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag WHERE ta_id NOT IN (SELECT DISTINCT (ta_id) FROM an_tag)");
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_scheduler WHERE sc_id='%q'", script_id);
      if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action WHERE ac_type=%d AND ac_params='%q'", ACTION_SCRIPT, script_id);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_action_script WHERE sc_id='%q'", script_id);
          if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
            sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_script WHERE sc_id='%q'", script_id);
            if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              return 1;
            } else {
              log_message(LOG_INFO, "Error deleting script");
              return 0;
            }
          } else {
            log_message(LOG_INFO, "Error deleting action_script");
            return 0;
          }
        } else {
          log_message(LOG_INFO, "Error deleting action");
          return 0;
        }
      } else {
        log_message(LOG_INFO, "Error deleting schedules");
        return 0;
      }
    } else {
      log_message(LOG_INFO, "Error deleting tag");
      return 0;
    }
  } else {
    log_message(LOG_INFO, "Error deleting tag_element");
    return 0;
  }
}

/**
 * Add a schedule into the database
 */
int add_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  
  if (0 == strcmp(cur_schedule.name, "") || (cur_schedule.next_time == 0 && cur_schedule.repeat_schedule == -1) || (cur_schedule.repeat_schedule > -1 && cur_schedule.repeat_schedule_value == 0) || cur_schedule.script == 0) {log_message(LOG_INFO, "Error inserting schedule, wrong params"); return 0;}
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_scheduler (sh_name, sh_enabled, sh_next_time, sh_repeat_schedule, sh_repeat_schedule_value, sh_remove_after_done, de_id, sc_id) VALUES ('%q', '%d', '%d', '%d', '%d', '%d', (SELECT de_id FROM an_device WHERE de_name='%q'), '%d')", cur_schedule.name, cur_schedule.enabled, cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, cur_schedule.device, cur_schedule.script);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    cur_schedule.id = (int)sqlite3_last_insert_rowid(sqlite3_db);
    snprintf(str_id, WORDLENGTH, "%d", cur_schedule.id);
    tags = build_tags_from_list(cur_schedule.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCHEDULE, str_id, tags);
    snprintf(command_result, MSGLENGTH*2, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat_schedule\":%d,\"repeat_schedule_value\":%d,\"remove_after_done\":%d,\"device\":\"%s\",\"script\":%d,\"tags\":%s}", cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, cur_schedule.device, cur_schedule.script, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_INFO, "Error inserting action");
    return 0;
  }
}

/**
 * Modifies the specified schedule
 */
int set_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  
  if (cur_schedule.id == 0 || 0 == strcmp(cur_schedule.name, "") || (cur_schedule.next_time == 0 && cur_schedule.repeat_schedule == -1) || (cur_schedule.repeat_schedule > -1 && cur_schedule.repeat_schedule_value == 0) || cur_schedule.script == 0) {log_message(LOG_INFO, "Error updating schedule, wrong params"); return 0;}
  
    sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_scheduler SET sh_name='%q', sh_enabled='%d', sh_next_time='%d', sh_repeat_schedule='%d', sh_repeat_schedule_value='%d', sh_remove_after_done='%d', de_id=(SELECT de_id FROM an_device WHERE de_name='%q'), sc_id='%d' WHERE sh_id='%d'", cur_schedule.name, cur_schedule.enabled, cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, cur_schedule.device, cur_schedule.script, cur_schedule.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    snprintf(str_id, WORDLENGTH, "%d", cur_schedule.id);
    tags = build_tags_from_list(cur_schedule.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCHEDULE, str_id, tags);
    snprintf(command_result, MSGLENGTH*2, "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat_schedule\":%d,\"repeat_schedule_value\":%d,\"remove_after_done\":%d,\"device\":\"%s\",\"script\":%d,\"tags\":%s}", cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, cur_schedule.device, cur_schedule.script, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_INFO, "Error updating action");
    return 0;
  }
}

/**
 * Delete the specified schedule
 */
int delete_schedule(sqlite3 * sqlite3_db, char * schedule_id) {
  char sql_query[MSGLENGTH+1];
  
  if (schedule_id == NULL || 0 == strcmp("", schedule_id)) {log_message(LOG_INFO, "Error deleting schedule, wrong params"); return 0;}
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag_element WHERE sh_id='%q'", schedule_id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag WHERE ta_id NOT IN (SELECT DISTINCT (ta_id) FROM an_tag)");
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_scheduler WHERE sh_id='%q'", schedule_id);
      return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
    } else {
      log_message(LOG_INFO, "Error deleting tag");
      return 0;
    }
  } else {
    log_message(LOG_INFO, "Error deleting tag_element");
    return 0;
  }
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
int save_startup_heater_status(sqlite3 * sqlite3_db, char * device, char * heater_name, int heat_enabled, float max_heat_value) {
  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, he_id;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT he_id FROM an_heater WHERE he_name = '%q' AND de_id in (SELECT de_id FROM an_device where de_name='%q')", heater_name, device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
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
    sqlite3_finalize(stmt);
    return ( sql_result == SQLITE_OK );
  }
}

/**
 * Save the switch status in the database for startup init
 */
int save_startup_switch_status(sqlite3 * sqlite3_db, char * device, char * switcher, int status) {
  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_id FROM an_switch WHERE de_id in (SELECT de_id FROM an_device where de_name='%q') AND sw_name = '%q'", device, switcher);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_switch SET sw_status='%d' WHERE sw_id='%d'", status, sqlite3_column_int(stmt, 0));
      sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
      sqlite3_finalize(stmt);
      return ( sql_result == SQLITE_OK );
    } else {
      return 0;
    }
  }
}

/**
 * Save the dimmer value in the database for startup init
 */
int save_startup_dimmer_value(sqlite3 * sqlite3_db, char * device, char * dimmer, int value) {
  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT di_id FROM an_dimmer WHERE de_id in (SELECT de_id FROM an_device where de_name='%q') AND di_name = '%q'", device, dimmer);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_dimmer SET di_value='%d' WHERE di_id='%d'", value, sqlite3_column_int(stmt, 0));
      sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
      sqlite3_finalize(stmt);
      return ( sql_result == SQLITE_OK );
    } else {
      return 0;
    }
  }
}

/**
 * Set all the switch to on if their status is on in the database
 */
int set_startup_all_switch(sqlite3 * sqlite3_db, device * cur_device) {
  char sql_query[MSGLENGTH+1], log[MSGLENGTH+1], switch_name[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, state_result=1;
  
  // Do not initiate elements on device connect, because it takes a few seconds to pair elements
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw_name, sw_status FROM an_switch WHERE de_id in (SELECT de_id FROM an_device where de_name='%q') AND sw_status = 1", cur_device->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query set_startup_all_switch");
      sqlite3_finalize(stmt);
      state_result = 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        snprintf(switch_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
        if (set_switch_state(cur_device, switch_name, sqlite3_column_int(stmt, 1)) == ERROR_SWITCH) {
          snprintf(log, MSGLENGTH, "Error setting switcher %s on device %s", switch_name, cur_device->name);
          log_message(LOG_INFO, log);
          state_result = 0;
        }
        row_result = sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);
    }
  }
  return state_result;
}

/**
 * Set all the dimmers values if their status is on in the database
 */
int set_startup_all_dimmer_value(sqlite3 * sqlite3_db, device * cur_device) {
  char sql_query[MSGLENGTH+1], log[MSGLENGTH+1], dimmer_name[WORDLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, result=1;
  
  // Do not initiate elements on device connect, because it takes a few seconds to pair elements
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT di_name, di_value FROM an_dimmer WHERE de_id in (SELECT de_id FROM an_device where de_name='%q')", cur_device->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query set_startup_all_dimmer");
      sqlite3_finalize(stmt);
      result = 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        snprintf(dimmer_name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
        if (set_dimmer_value(cur_device, dimmer_name, sqlite3_column_int(stmt, 1)) == ERROR_DIMMER) {
          snprintf(log, MSGLENGTH, "Error setting dimmer %s on device %s", dimmer_name, cur_device->name);
          log_message(LOG_INFO, log);
          result = 0;
        }
        row_result = sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);
    }
  }
  return result;
}

/**
 * Get the heat status in the database for startup init
 */ 
heater * get_startup_heater_status(sqlite3 * sqlite3_db, char * device) {
  char sql_query[MSGLENGTH+1];
  heater * heaters = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, nb_heaters=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT he.he_id, he.he_name, de.de_name, he.he_enabled, he.he_set, he.he_max_heat_value FROM an_heater he LEFT OUTER JOIN an_device de on de.de_id = he.de_id WHERE he.de_id in (SELECT de_id FROM an_device where de_name='%q')", device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query get_startup_heater_status");
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
 * Initialize the device with the stored init values (heater and switches)
 */
int init_device_status(sqlite3 * sqlite3_db, device * cur_device) {
  heater * heaters;
  int heat_status=1, i=0;
  char output[WORDLENGTH+1];
  
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    heaters = get_startup_heater_status(sqlite3_db, cur_device->name);
    for (i=0; heaters[i].id != -1; i++) {
      if (!set_heater(cur_device, heaters[i].name, heaters[i].set, heaters[i].heat_max_value, output)) {
        heat_status = 0;
      }
    }
    free(heaters);
    return (heat_status && set_startup_all_switch(sqlite3_db, cur_device) && set_startup_all_dimmer_value(sqlite3_db, cur_device));
  } else {
    return 1;
  }
}

/**
 * Gets the monitored value using the given filters
 */
char * get_monitor(sqlite3 * sqlite3_db, const char * device, const char * switcher, const char * sensor, const char * start_date) {
  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result, first_result = 1, t_len;
  char p_device[WORDLENGTH+1], p_switch[WORDLENGTH+1], p_sensor[WORDLENGTH+1], p_start_date[WORDLENGTH+1], where_switch[MSGLENGTH+1], where_sensor[MSGLENGTH+1], monitor_date[WORDLENGTH+1], monitor_value[WORDLENGTH+1], one_item[WORDLENGTH*2 + 1];
  time_t yesterday;
  char * to_return = NULL;
  
  snprintf(p_device, WORDLENGTH, "%s", device);
  snprintf(p_switch, WORDLENGTH, "%s", switcher);
  snprintf(p_sensor, WORDLENGTH, "%s", sensor);
  if (start_date == NULL) {
    time(&yesterday);
    yesterday -= 60*60*24; // set start_date to yesterday
    snprintf(p_start_date, WORDLENGTH, "%ld", yesterday);
  } else {
    snprintf(p_start_date, WORDLENGTH, "%s", start_date);
  }
  
  if (switcher != NULL && 0 != strcmp("0", switcher)) {
    sqlite3_snprintf(MSGLENGTH, where_switch, "AND mo.sw_id = (SELECT sw_id FROM an_switch WHERE sw_name='%q' AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_switch, p_device);
  } else {
    strcpy(p_switch, "");
    strcpy(where_switch, "");
  }
  
  if (sensor != NULL && 0 != strcmp("0", sensor)) {
    sqlite3_snprintf(MSGLENGTH, where_sensor, "AND mo.se_id = (SELECT se_id FROM an_sensor WHERE se_name='%q' AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_sensor, p_device);
  } else {
    strcpy(p_sensor, "");
    strcpy(where_sensor, "");
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT mo.mo_date, mo.mo_result FROM an_monitor mo LEFT OUTER JOIN an_device de ON de.de_id = mo.de_id LEFT OUTER JOIN an_switch sw ON sw.sw_id = mo.sw_id LEFT OUTER JOIN an_sensor se ON se.se_id = mo.se_id WHERE mo.de_id = (SELECT de_id FROM an_device WHERE de_name='%q') AND datetime(mo.mo_date, 'unixepoch') >= datetime('%q', 'unixepoch') %s %s ORDER BY mo.mo_date ASC", p_device, p_start_date, where_switch, where_sensor);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    t_len = (67 + strlen(device) + strlen(p_switch) + strlen(p_sensor) + strlen(p_start_date));
    to_return = malloc(t_len * sizeof(char));
    snprintf(to_return, t_len, "{\"device\":\"%s\",\"switcher\":\"%s\",\"sensor\":\"%s\",\"start_date\":\"%s\",\"values\":[", device, p_switch, p_sensor, p_start_date);
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      snprintf(monitor_date, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
      snprintf(monitor_value, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(one_item, WORDLENGTH*2, "%s{\"date_time\":\"%s\",\"value\":\"%s\"}", first_result?"":",", monitor_date, monitor_value);
      to_return = realloc(to_return, (strlen(to_return)+strlen(one_item)+1)*sizeof(char));
      strcat(to_return, one_item);
      first_result = 0;
      row_result = sqlite3_step(stmt);
    }
    to_return = realloc(to_return, (strlen(to_return)+3) * sizeof(char));
    strcat(to_return, "]}");
  }
  return to_return;
}

/**
 * archive the journal data from the main db to the archive db, until epoch_from, then vacuum the main db
 */
int archive_journal(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from) {
  char sql_query[MSGLENGTH+1] = {0}, message[MSGLENGTH+1] = {0};
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  if (sqlite3_db != NULL && sqlite3_archive_db != NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT jo_date, jo_origin, jo_command, jo_result FROM an_journal WHERE jo_date < '%d'", epoch_from);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query (archive_journal)");
      sqlite3_finalize(stmt);
      return 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_journal (jo_date, jo_origin, jo_command, jo_result) VALUES ('%q', '%q', '%q', '%q')", 
                         (char*)sqlite3_column_text(stmt, 0),
                         (char*)sqlite3_column_text(stmt, 1),
                         (char*)sqlite3_column_text(stmt, 2),
                         (char*)sqlite3_column_text(stmt, 3));
        if ( sqlite3_exec(sqlite3_archive_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          log_message(LOG_INFO, "Error archiving journal");
          sqlite3_finalize(stmt);
          return 0;
        }
        row_result = sqlite3_step(stmt);
      }
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_journal WHERE jo_date < '%d'; vacuum", epoch_from);
    sqlite3_finalize(stmt);
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      snprintf(message, MSGLENGTH, "End archiving journal, limit date %d", epoch_from);
      log_message(LOG_INFO, message);
      return 1;
    } else {
      log_message(LOG_INFO, "Error deleting old journal data");
      return 0;
    }
    }
  } else {
    return 0;
  }
}

/**
 * archive the monitor data from the main db to the archive db, until epoch_from, then vacuum the main db
 */
int archive_monitor(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from) {
  char sql_query[MSGLENGTH+1] = {0}, message[MSGLENGTH+1] = {0};
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  if (sqlite3_db != NULL && sqlite3_archive_db != NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT mo_date, de_id, sw_id, se_id, mo_result FROM an_monitor WHERE mo_date < '%d'", epoch_from);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query (archive_monitor)");
      sqlite3_finalize(stmt);
      return 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_monitor (mo_date, de_id, sw_id, se_id, mo_result) VALUES ('%q', '%d', '%d', '%d', '%q')", 
                         (char*)sqlite3_column_text(stmt, 0),
                         sqlite3_column_int(stmt, 1),
                         sqlite3_column_int(stmt, 2),
                         sqlite3_column_int(stmt, 3),
                         (char*)sqlite3_column_text(stmt, 4));
        if ( sqlite3_exec(sqlite3_archive_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          log_message(LOG_INFO, "Error archiving monitor");
          sqlite3_finalize(stmt);
          return 0;
        }
        row_result = sqlite3_step(stmt);
      }
      sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_monitor WHERE mo_date < '%d'; vacuum", epoch_from);
      sqlite3_finalize(stmt);
      if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
        snprintf(message, MSGLENGTH, "End archiving monitor, limit date %d", epoch_from);
        log_message(LOG_INFO, message);
        return 1;
      } else {
        log_message(LOG_INFO, "Error deleting old monitor data");
        return 0;
      }
    }
  } else {
    return 0;
  }
}

/**
 * archive journal and monitor data from main db, until epoch_from
 */
int archive(sqlite3 * sqlite3_db, char * db_archive_path, unsigned int epoch_from) {
  int rc;
  sqlite3 * sqlite3_archive_db;
  char message[MSGLENGTH+1];
  
  rc = sqlite3_open_v2(db_archive_path, &sqlite3_archive_db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK && sqlite3_archive_db != NULL) {
    snprintf(message, MSGLENGTH, "Database error: %s", sqlite3_errmsg(sqlite3_archive_db));
    log_message(LOG_INFO, message);
    sqlite3_close(sqlite3_archive_db);
    return 0;
  } else {
    rc = archive_journal(sqlite3_db, sqlite3_archive_db, epoch_from) && 
                archive_monitor(sqlite3_db, sqlite3_archive_db, epoch_from);
    sqlite3_close(sqlite3_archive_db);
    return rc;
  }
}

/**
 * gets all the tags from an element
 * return value must be freed after use
 */
char ** get_tags(sqlite3 * sqlite3_db, char * device_name, unsigned int element_type, char * element) {
  char sql_query[MSGLENGTH+1] = {0}, where_element[MSGLENGTH+1] = {0}, cur_tag[WORDLENGTH+1], ** to_return = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, nb_return=0;
  switch (element_type) {
    case DATA_DEVICE:
      sqlite3_snprintf(MSGLENGTH, where_element, "de_id = (SELECT de_id FROM an_device WHERE de_name = '%q')", element);
      break;
    case DATA_SWITCH:
      sqlite3_snprintf(MSGLENGTH, where_element, "sw_id = (SELECT sw_id FROM an_switch WHERE sw_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_SENSOR:
      sqlite3_snprintf(MSGLENGTH, where_element, "se_id = (SELECT se_id FROM an_sensor WHERE se_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_HEATER:
      sqlite3_snprintf(MSGLENGTH, where_element, "he_id = (SELECT he_id FROM an_heater WHERE he_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_DIMMER:
      sqlite3_snprintf(MSGLENGTH, where_element, "di_id = (SELECT di_id FROM an_dimmer WHERE di_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_ACTION:
      sqlite3_snprintf(MSGLENGTH, where_element, "ac_id = '%q'", element);
      break;
    case DATA_SCRIPT:
      sqlite3_snprintf(MSGLENGTH, where_element, "sc_id = '%q'", element);
      break;
    case DATA_SCHEDULE:
      sqlite3_snprintf(MSGLENGTH, where_element, "sh_id = '%q'", element);
      break;
  }
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ta_name FROM an_tag WHERE ta_id in (SELECT ta_id FROM an_tag_element WHERE %s)", where_element);
  
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  to_return = malloc(sizeof(char *));
  to_return[0] = NULL;
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query (get_tags)");
    sqlite3_finalize(stmt);
    return to_return;
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      snprintf(cur_tag, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 0));
      to_return = realloc(to_return, (nb_return+2)*sizeof(char *));
      to_return[nb_return] = malloc(strlen(cur_tag)+1);
      strcpy(to_return[nb_return], cur_tag);
      to_return[nb_return+1] = NULL;
      nb_return++;
      row_result = sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);
  return to_return;
}

/**
 * Builds the json output from the tags list given by get_tags (string built from an array of strings)
 */
char * build_json_tags(char ** tags) {
  char * to_return = NULL, one_tag[(WORDLENGTH*2)+1] = {0};
  int nb_tags=0;
  to_return = malloc(2*sizeof(char));
  strcpy(to_return, "[");
  for (nb_tags=0; tags[nb_tags] != NULL; nb_tags++) {
    sanitize_json_string(tags[nb_tags], one_tag, WORDLENGTH);
    if (nb_tags ==0) {
      to_return = realloc(to_return, strlen(to_return)+strlen(one_tag)+3);
    } else {
      to_return = realloc(to_return, strlen(to_return)+strlen(one_tag)+4);
      strncat(to_return, ",", strlen(to_return)+1);
    }
    strncat(to_return, "\"", strlen(to_return)+strlen(one_tag)+3);
    strncat(to_return, one_tag, strlen(to_return)+strlen(one_tag)+3);
    strncat(to_return, "\"", strlen(to_return)+strlen(one_tag)+3);
  }
  to_return = realloc(to_return, (strlen(to_return)+2)*sizeof(char));
  strncat(to_return, "]", strlen(to_return)+1);
  return to_return;
}

/**
 * Builds a tags array from a tag list (format x,y,z)
 */
char ** build_tags_from_list(char * tags) {
  char ** to_return = NULL, *saveptr, * cur_tag;
  int counter=0;
  
  to_return = malloc(sizeof(char *));
  to_return[0] = NULL;
  if (tags != NULL) {
    cur_tag = strtok_r(tags, ",", &saveptr);
    if (cur_tag != NULL) {
      while (cur_tag != NULL) {
        to_return = realloc(to_return, (counter+2)*sizeof(char *));
        to_return[counter] = malloc((strlen(cur_tag)+1)*sizeof(char));
        to_return[counter+1] = NULL;
        snprintf(to_return[counter], strlen(cur_tag)+1, "%s", cur_tag);
        cur_tag = strtok_r(NULL, ",", &saveptr);
        counter++;
      }
    }
  }
  return to_return;
}

/**
 * set all the tags for an element
 */
int set_tags(sqlite3 * sqlite3_db, char * device_name, unsigned int element_type, char * element, char ** tags) {
  char sql_query[MSGLENGTH+1] = {0}, where_element[MSGLENGTH+1] = {0}, element_row[WORDLENGTH+1] = {0}, message[MSGLENGTH+1] = {0};
  int counter = 0, cur_tag = -1;
  
  if (tags != NULL) {
    switch (element_type) {
      case DATA_DEVICE:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT de_id FROM an_device WHERE de_name = '%q')", element);
        strcpy(element_row, "de_id");
        break;
      case DATA_SWITCH:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT sw_id FROM an_switch WHERE sw_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "sw_id");
        break;
      case DATA_SENSOR:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT se_id FROM an_sensor WHERE se_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "se_id");
        break;
      case DATA_HEATER:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT he_id FROM an_heater WHERE he_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "he_id");
        break;
      case DATA_DIMMER:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT di_id FROM an_dimmer WHERE di_name = '%q' AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "di_id");
        break;
      case DATA_ACTION:
        sqlite3_snprintf(MSGLENGTH, where_element, "'%q'", element);
        strcpy(element_row, "ac_id");
        break;
      case DATA_SCRIPT:
        sqlite3_snprintf(MSGLENGTH, where_element, "'%q'", element);
        strcpy(element_row, "sc_id");
        break;
      case DATA_SCHEDULE:
        sqlite3_snprintf(MSGLENGTH, where_element, "'%q'", element);
        strcpy(element_row, "sh_id");
        break;
    }
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag_element WHERE %q = %s", element_row, where_element);
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
      log_message(LOG_INFO, "Error deleting old tags (%s)", sql_query);
      return -1;
    } else {
      while (tags[counter] != NULL) {
        cur_tag = get_or_create_tag_id(sqlite3_db, tags[counter]);
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_tag_element (%s, ta_id) VALUES (%s, %d)", element_row, where_element, cur_tag);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          snprintf(message, MSGLENGTH, "Error inserting tag_element %s (%d)", tags[counter], cur_tag);
          log_message(LOG_INFO, message);
        }
        counter++;
      }
      return counter;
    }
  } else {
    return -1;
  }
}

/**
 * Get the tag id from the name or create it if it doesn't exist, then return the new id
 */
int get_or_create_tag_id(sqlite3 * sqlite3_db, char * tag) {
  char sql_query[MSGLENGTH+1] = {0};
  sqlite3_stmt *stmt;
  int sql_result, row_result, to_return = -1;
  
  if (tag != NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ta_id FROM an_tag WHERE ta_name = '%q'", tag);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_INFO, "Error preparing sql query (get_or_create_tag_id)");
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        to_return = sqlite3_column_int(stmt, 0);
      } else {
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_tag (ta_name) VALUES ('%q')", tag);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          to_return = (int)sqlite3_last_insert_rowid(sqlite3_db);
        }
      }
    }
  }
  sqlite3_finalize(stmt);
  return to_return;
}

/**
 * Free tags array
 */
int free_tags(char ** tags) {
  int counter = 0;
  if (tags != NULL) {
    while (tags[counter] != NULL) {
      free(tags[counter]);
      tags[counter] = NULL;
      counter++;
    }
    free(tags);
    return counter;
  } else {
    return 0;
  }
}
