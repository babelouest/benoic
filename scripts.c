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

// TODO: add actions list to all outputs
static const char json_template_scripts_setscript[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"tags\":%s}";
static const char json_template_scripts_addscript[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"tags\":%s}";
static const char json_template_scripts_getscript_notags[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\"}";
static const char json_template_scripts_getscript_tags[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"tags\":%s}";
static const char json_template_scripts_getactionscript[] = "{\"id\":%d,\"name\":\"%s\",\"rank\":%d,\"enabled\":%s}";
static const char json_template_scripts_getscripts[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"actions\":[%s],\"tags\":%s}";

/**
 * Get the different scripts ids present
 */
char * get_scripts(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char str_id[WORDLENGTH+1], cur_name[WORDLENGTH+1], device_name[WORDLENGTH+1], cur_script[WORDLENGTH+1];
  int cur_id, cur_enabled;
  char * scripts = malloc(2*sizeof(char)), * sql_query = NULL, * one_item = NULL, * actions = NULL, * tags = NULL, ** tags_array = NULL;
  int str_len;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (device == NULL) {
    sql_query = sqlite3_mprintf("SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name\
                      FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id");
  } else {
    sql_query = sqlite3_mprintf("SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name\
                      FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id\
                      WHERE de.de_id IN (SELECT de_id FROM an_device WHERE de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query");
    free(scripts);
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    strcpy(scripts, "");
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      snprintf(cur_script, WORDLENGTH*sizeof(char), "%d", sqlite3_column_int(stmt, 0));
      if (row_result == SQLITE_ROW) {
        if (strlen(scripts) > 0) {
          scripts = realloc(scripts, (strlen(scripts)+2));
          strcat(scripts, ",");
        }
        cur_id = sqlite3_column_int(stmt, 0);
        snprintf(str_id, WORDLENGTH*sizeof(char), "%d", cur_id);
        snprintf(cur_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 1));
        tags_array = get_tags(sqlite3_db, NULL, DATA_SCRIPT, str_id);
        tags = build_json_tags(tags_array);
        cur_enabled = sqlite3_column_int(stmt, 2);
        if (sqlite3_column_text(stmt, 3) != NULL) {
          snprintf(device_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 3));
        } else {
          strcpy(device_name, "");
        }
        actions = get_action_script(sqlite3_db, cur_id);
        if (actions == NULL) {
          log_message(LOG_LEVEL_WARNING, "Error getting actions from script");
        }
        str_len = snprintf(NULL, 0, json_template_scripts_getscripts, cur_id, cur_name, cur_enabled?"true":"false", device_name, actions, tags);
        one_item = malloc((str_len+1)*sizeof(char));
        snprintf(one_item, (str_len+1)*sizeof(char), json_template_scripts_getscripts, cur_id, cur_name, cur_enabled?"true":"false", device_name, actions, tags);
        scripts = realloc(scripts, (strlen(scripts)+strlen(one_item)+1));
        strcat(scripts, one_item);
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
  char * sql_query = NULL, ac_name[WORDLENGTH+1], * actions = malloc(sizeof(char)), * one_item = NULL;
  int rank, ac_id, enabled, oi_len;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (script_id == 0) {
    log_message(LOG_LEVEL_WARNING, "Error getting action scripts, script_id is 0");
    free(actions);
    return NULL;
  }
  
  sql_query = sqlite3_mprintf("SELECT ac.ac_id, ac.ac_name, aas.as_rank, aas.as_enabled FROM an_action_script aas, an_action ac\
                    WHERE ac.ac_id=aas.ac_id AND aas.sc_id='%d' ORDER BY as_rank", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (get_action_script)");
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
        snprintf(ac_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 1));
        rank = sqlite3_column_int(stmt, 2);
        enabled = sqlite3_column_int(stmt, 3);
        oi_len = snprintf(NULL, 0, json_template_scripts_getactionscript, ac_id, ac_name, rank, enabled?"true":"false");
        one_item = malloc((oi_len+1)*sizeof(char));
        snprintf(one_item, (oi_len+1)*sizeof(char), json_template_scripts_getactionscript, ac_id, ac_name, rank, enabled?"true":"false");
        actions = realloc(actions, (strlen(actions)+strlen(one_item)+1)*sizeof(char));
        strcat(actions, one_item);
        free(one_item);
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
  int sql_result, sc_id, sc_enabled, row_result, str_len;
  char * sql_query = NULL, sc_name[WORDLENGTH+1], device[WORDLENGTH+1], * to_return = NULL, * tags = NULL, ** tags_array = NULL;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query = sqlite3_mprintf("SELECT sc.sc_id, sc.sc_name, sc.sc_enabled, de.de_name FROM an_script sc LEFT OUTER JOIN an_device de ON de.de_id = sc.de_id WHERE sc.sc_id = '%q'", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (get_script)");
    sqlite3_finalize(stmt);
    return NULL;
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      sc_id = sqlite3_column_int(stmt, 0);
      snprintf(sc_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 1));
      sc_enabled = sqlite3_column_int(stmt, 2);
      if (sqlite3_column_text(stmt, 3) != NULL) {
        snprintf(device, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 3));
      } else {
        strcpy(device, "");
      }
      if (with_tags) {
        tags_array = get_tags(sqlite3_db, NULL, DATA_SCRIPT, script_id);
        tags = build_json_tags(tags_array);
        str_len = snprintf(NULL, 0, json_template_scripts_getscript_tags, sc_id, sc_name, sc_enabled?"true":"false", device, tags);
        to_return = malloc((str_len+1)*sizeof(char));
        snprintf(to_return, (str_len+1)*sizeof(char), json_template_scripts_getscript_tags, sc_id, sc_name, sc_enabled?"true":"false", device, tags);
        free(tags);
        free_tags(tags_array);
      } else {
        str_len = snprintf(NULL, 0, json_template_scripts_getscript_notags, sc_id, sc_name, sc_enabled?"true":"false", device);
        to_return = malloc((str_len+1)*sizeof(char));
        snprintf(to_return, (str_len+1)*sizeof(char), json_template_scripts_getscript_notags, sc_id, sc_name, sc_enabled?"true":"false", device);
      }
      sqlite3_finalize(stmt);
      return to_return;
    } else {
      log_message(LOG_LEVEL_WARNING, "Script %s not found", script_id);
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
  int sql_result, row_result, script_found = 0;
  char * sql_query = NULL;
  action ac;
    
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query = sqlite3_mprintf("SELECT ac.ac_name, ac.ac_type, de.de_name, sw.sw_name, di.di_name, he.he_name, ac.ac_params, ac.ac_id\
            FROM an_action ac, an_action_script acs LEFT OUTER JOIN an_device de ON de.de_id = ac.de_id\
            LEFT OUTER JOIN an_switch sw ON sw.sw_id = ac.sw_id LEFT OUTER JOIN an_dimmer di ON di.di_id = ac.di_id\
            LEFT OUTER JOIN an_heater he ON he.he_id = ac.he_id WHERE ac.ac_id = acs.ac_id AND acs.sc_id = '%s'\
            AND NOT acs.as_enabled = 0 ORDER BY acs.as_rank", script_id);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (run_script)");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      script_found = 1;
      if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
        snprintf(ac.name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 0));
      } else {
        strcpy(ac.name, "");
      }
      if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
        ac.type = sqlite3_column_int(stmt, 1);
      } else {
        ac.type = ACTION_NONE;
      }
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        snprintf(ac.device, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 2));
      } else {
        memset(ac.device, 0, WORDLENGTH*sizeof(char));
      }
      if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
        snprintf(ac.switcher, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 3));
      } else {
        memset(ac.switcher, 0, WORDLENGTH*sizeof(char));
      }
      if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        snprintf(ac.dimmer, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 4));
      } else {
        memset(ac.dimmer, 0, WORDLENGTH*sizeof(char));
      }
      if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
        snprintf(ac.heater, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 5));
      } else {
        memset(ac.heater, 0, WORDLENGTH*sizeof(char));
      }
      if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        snprintf(ac.params, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 6));
      } else {
        memset(ac.params, 0, WORDLENGTH*sizeof(char));
      }
      ac.id = sqlite3_column_int(stmt, 7);
      if (!run_action(ac, terminal, nb_terminal, sqlite3_db, script_path)) {
        sqlite3_finalize(stmt);
        return 0;
      }
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return (script_found == 1);
  }
  sqlite3_finalize(stmt);
  return 0;
}

/**
 * Add a script into the database
 */
char * add_script(sqlite3 * sqlite3_db, script cur_script) {
  char * sql_query = NULL, * sql_query2 = NULL, ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1], * to_return = NULL;
  int rank=0, tr_len;
  
  char * action, * saveptr, * action_id, * enabled, * saveptr2;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (0 == strcmp("", cur_script.name)) {log_message(LOG_LEVEL_WARNING, "Error inserting script, wrong params"); return NULL;}
  
  sql_query = sqlite3_mprintf("INSERT INTO an_script (sc_name, de_id, sc_enabled)\
                    VALUES ('%q', (SELECT de_id FROM an_device WHERE de_name='%q'), '%d')",
                    cur_script.name, cur_script.device, cur_script.enabled);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    cur_script.id = (int)sqlite3_last_insert_rowid(sqlite3_db);
        
    // Parsing actions, then insert into an_action_script
    action = strtok_r(cur_script.actions, ";", &saveptr);
    while (action != NULL) {
      action_id = strtok_r(action, ",", &saveptr2);
      enabled = strtok_r(NULL, ",", &saveptr2);
      if (action_id != NULL && enabled != NULL) {
        sql_query2 = sqlite3_mprintf("INSERT INTO an_action_script (sc_id, ac_id, as_enabled, as_rank)\
                          VALUES ('%d', '%q', '%s', '%d')", cur_script.id, action, enabled, rank++);
        
        if ( sqlite3_exec(sqlite3_db, sql_query2, NULL, NULL, NULL) != SQLITE_OK ) {
          log_message(LOG_LEVEL_WARNING, "Error inserting action (%d, %s, %d)", cur_script.id, action, rank++);
        }
        sqlite3_free(sql_query2);
      } else {
        log_message(LOG_LEVEL_WARNING, "Error inserting action list, wrong parameters");
      }
      action = strtok_r(NULL, ";", &saveptr);
    }
    snprintf(str_id, WORDLENGTH*sizeof(char), "%d", cur_script.id);
    tags = build_tags_from_list(cur_script.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCRIPT, str_id, tags);
    tr_len = snprintf(NULL, 0, json_template_scripts_addscript, cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device, tags_json);
    to_return = malloc((tr_len+1)*sizeof(char));
    snprintf(to_return, (tr_len+1)*sizeof(char), json_template_scripts_addscript, cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device, tags_json);
    free(tags_json);
    free_tags(tags);
  } else {
    log_message(LOG_LEVEL_WARNING, "Error inserting script");
  }
  sqlite3_free(sql_query);
  return to_return;
}

/**
 * Modifies the specified script
 */
char * set_script(sqlite3 * sqlite3_db, script cur_script) {
  char * sql_query = NULL, * sql_query2 = NULL, ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1], * to_return = NULL, * scripts_list = NULL;
  int rank=0, tr_len;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  char * action_token, * action_id, * enabled, * saveptr, * saveptr2, * actions;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  if (0 == strcmp("", cur_script.name)) {log_message(LOG_LEVEL_WARNING, "Error updating script, wrong params"); return NULL;}
  if (cur_script.id == 0) {log_message(LOG_LEVEL_WARNING, "Error updating script, wrong params"); return NULL;}
  
  // Before updating script, check if its actions don't lead to infinite loop
  scripts_list = malloc((num_digits(cur_script.id)+3)*sizeof(char));
  sprintf(scripts_list, "#%d#", cur_script.id);
  actions = strdup(cur_script.actions);
  action_token = strtok_r(actions, ";", &saveptr);
  while (action_token != NULL) {
    action_id = strtok_r(action_token, ",", &saveptr2);
    enabled = strtok_r(NULL, ",", &saveptr2);
    if (action_id != NULL && enabled != NULL) {
      sql_query = sqlite3_mprintf("SELECT ac_params FROM an_action WHERE ac_id = '%q' AND ac_type = %d", action_id, ACTION_SCRIPT);
			sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
			sqlite3_free(sql_query);
			if (sql_result != SQLITE_OK) {
				log_message(LOG_LEVEL_WARNING, "Error preparing sql query (set_script)");
				sqlite3_finalize(stmt);
        free(scripts_list);
        free(actions);
				return 0;
			} else {
        row_result = sqlite3_step(stmt);
        if (row_result == SQLITE_ROW) {
          if (!detect_infinite_loop_script(sqlite3_db, &scripts_list, sqlite3_column_int(stmt, 0))) {
            log_message(LOG_LEVEL_WARNING, "Error updating script, infinite loop detected");
            sqlite3_finalize(stmt);
            free(scripts_list);
            free(actions);
            return NULL;
          }
        }
      }
    }
    action_token = strtok_r(NULL, ";", &saveptr);
  }
  sqlite3_finalize(stmt);
  free(scripts_list);
  free(actions);
    
  // Reinit action_script list
  sql_query = sqlite3_mprintf("DELETE FROM an_action_script WHERE sc_id='%d'", cur_script.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
    log_message(LOG_LEVEL_WARNING, "Error updating script, wrong params");
  }
  sqlite3_free(sql_query);
  
  sql_query = sqlite3_mprintf("UPDATE an_script SET sc_name='%q', de_id=(SELECT de_id FROM an_device WHERE de_name='%q'),\
                    sc_enabled='%d' WHERE sc_id='%d'", cur_script.name, cur_script.device, cur_script.enabled, cur_script.id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    // Parsing actions, then insert into an_action_script
    action_token = strtok_r(cur_script.actions, ";", &saveptr);
    
    while (action_token != NULL) {
      action_id = strtok_r(action_token, ",", &saveptr2);
      enabled = strtok_r(NULL, ",", &saveptr2);
      if (action_id != NULL && enabled != NULL) {
        sql_query2 = sqlite3_mprintf("INSERT INTO an_action_script (sc_id, ac_id, as_rank, as_enabled) VALUES ('%d', '%q', '%d', '%s')",
                          cur_script.id, action_id, rank++, enabled);
        if ( sqlite3_exec(sqlite3_db, sql_query2, NULL, NULL, NULL) != SQLITE_OK ) {
          log_message(LOG_LEVEL_WARNING, "Error updating action (%d, %s, %d)", cur_script.id, action_token, rank++);
        }
        sqlite3_free(sql_query2);
      } else {
        log_message(LOG_LEVEL_WARNING, "Error updating action list, wrong parameters");
      }
      action_token = strtok_r(NULL, ";", &saveptr);
    }
    snprintf(str_id, WORDLENGTH*sizeof(char), "%d", cur_script.id);
    tags = build_tags_from_list(cur_script.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCRIPT, str_id, tags);
    tr_len = snprintf(NULL, 0, json_template_scripts_setscript, cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device, tags_json);
    to_return = malloc((tr_len+1)*sizeof(char));
    snprintf(to_return, (tr_len+1)*sizeof(char), json_template_scripts_setscript, cur_script.id, cur_script.name, cur_script.enabled?"true":"false", cur_script.device, tags_json);
    free(tags_json);
    free_tags(tags);
  } else {
    log_message(LOG_LEVEL_WARNING, "Error updating script");
  }
  sqlite3_free(sql_query);
  return to_return;
}

/**
 * Delete the specified script
 */
int delete_script(sqlite3 * sqlite3_db, char * script_id) {
  char * sql_query1 = NULL, * sql_query2 = NULL, * sql_query3 = NULL, * sql_query4 = NULL, * sql_query5 = NULL, * sql_query6 = NULL;
  int result = 0;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
  sql_query1 = sqlite3_mprintf("DELETE FROM an_tag_element WHERE sc_id='%q'", script_id);
  if ( sqlite3_exec(sqlite3_db, sql_query1, NULL, NULL, NULL) == SQLITE_OK ) {
    sql_query2 = sqlite3_mprintf("DELETE FROM an_tag WHERE ta_id NOT IN (SELECT DISTINCT (ta_id) FROM an_tag)");
    if ( sqlite3_exec(sqlite3_db, sql_query2, NULL, NULL, NULL) == SQLITE_OK ) {
      sql_query3 = sqlite3_mprintf("DELETE FROM an_scheduler WHERE sc_id='%q'", script_id);
      if ( sqlite3_exec(sqlite3_db, sql_query3, NULL, NULL, NULL) == SQLITE_OK ) {
        sql_query4 = sqlite3_mprintf("DELETE FROM an_action WHERE ac_type=%d AND ac_params='%q'", ACTION_SCRIPT, script_id);
        if ( sqlite3_exec(sqlite3_db, sql_query4, NULL, NULL, NULL) == SQLITE_OK ) {
          sql_query5 = sqlite3_mprintf("DELETE FROM an_action_script WHERE sc_id='%q'", script_id);
          if ( sqlite3_exec(sqlite3_db, sql_query5, NULL, NULL, NULL) == SQLITE_OK ) {
            sql_query6 = sqlite3_mprintf("DELETE FROM an_script WHERE sc_id='%q'", script_id);
            if ( sqlite3_exec(sqlite3_db, sql_query6, NULL, NULL, NULL) == SQLITE_OK ) {
              result = 1;
            } else {
              log_message(LOG_LEVEL_WARNING, "Error deleting script");
            }
          } else {
            log_message(LOG_LEVEL_WARNING, "Error deleting action_script");
          }
        } else {
          log_message(LOG_LEVEL_WARNING, "Error deleting action");
        }
      } else {
        log_message(LOG_LEVEL_WARNING, "Error deleting schedules");
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
  sqlite3_free(sql_query5);
  sqlite3_free(sql_query6);
  return result;
}

/**
 * Detect if a script is called recursively by itself
 */
int detect_infinite_loop_script(sqlite3 * sqlite3_db, char ** scripts_list, int script_id) {
	char needle[num_digits(script_id)+3];
  sqlite3_stmt *stmt;
  int sql_result, row_result, scripts_list_len, next_script_id;
  char * sql_query = NULL;
  char * scripts_list_append = NULL;
  
  log_message(LOG_LEVEL_DEBUG, "Entering function %s from file %s", __PRETTY_FUNCTION__, __FILE__);
	if (sqlite3_db != NULL && scripts_list != NULL && (*scripts_list) != NULL && script_id != 0) {
		sprintf(needle, "#%d#", script_id);
		if (strstr((*scripts_list), needle) != NULL) {
			// script_id already exists in the scripts_list
			return 0;
		} else {
			// Append script_id to scripts_list and test them
			scripts_list_len = snprintf(NULL, 0, "%s%d#", (*scripts_list), script_id);
			(*scripts_list) = realloc((*scripts_list), (scripts_list_len+1));
      scripts_list_append = malloc((num_digits(script_id)+2)*sizeof(char));
      sprintf(scripts_list_append, "%d#", script_id);
      strncat((*scripts_list), scripts_list_append, scripts_list_len);
      free(scripts_list_append);
			
			// Get all scripts called by this script
			sql_query = sqlite3_mprintf("SELECT ac_params FROM an_action WHERE ac_id in \
																	(SELECT ac_id FROM an_action_script WHERE sc_id='%d')\
																	AND ac_type = %d", script_id, ACTION_SCRIPT);
			sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
			sqlite3_free(sql_query);
			if (sql_result != SQLITE_OK) {
				log_message(LOG_LEVEL_WARNING, "Error preparing sql query (detect_infinite_loop_script)");
				sqlite3_finalize(stmt);
				return 0;
			} else {
				row_result = sqlite3_step(stmt);
				while (row_result == SQLITE_ROW) {
					next_script_id = sqlite3_column_int(stmt, 0);
					if (!detect_infinite_loop_script(sqlite3_db, scripts_list, next_script_id)) {
						// Infinite loop detected, exiting with 0
						sqlite3_finalize(stmt);
						return 0;
					} else {
						row_result = sqlite3_step(stmt);
					}
				}
				// No infinite loop detected in the script
				sqlite3_finalize(stmt);
				return 1;
			}
		}
	} else {
		return 0;
	}
}
