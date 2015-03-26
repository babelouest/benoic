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

static const char json_template_tools_get_monitor[] = "{\"device\":\"%s\",\"switcher\":\"%s\",\"sensor\":\"%s\",\"start_date\":\"%s\",\"values\":[%s]}";
static const char json_template_tools_monitor_one_value[] = "{\"date_time\":\"%s\",\"value\":\"%s\"}";
/**
 * Save the heat status in the database for startup init
 */
int save_startup_heater_status(sqlite3 * sqlite3_db, char * device, char * heater_name, int heat_enabled, float max_heat_value) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, he_id;
  
  sql_query = sqlite3_mprintf("SELECT he_id FROM an_heater WHERE he_name = '%q'\
                    AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", heater_name, device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (save_startup_heater_status)");
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
  
  sql_query = sqlite3_mprintf("SELECT sw_id FROM an_switch WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q') AND sw_name = '%q'", device, switcher);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (save_startup_switch_status)");
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
  
  sql_query = sqlite3_mprintf("SELECT di_id FROM an_dimmer\
                    WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q') AND di_name = '%q'", device, dimmer);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (save_startup_dimmer_value)");
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
  int sql_result, row_result, state_result=1;
  
  // Do not initiate elements on device connect, because it takes a few seconds to pair elements
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    sql_query = sqlite3_mprintf("SELECT sw_name, sw_status FROM an_switch\
                      WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q') AND sw_status = 1", cur_device->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query (set_startup_all_switch)");
      state_result = 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        snprintf(switch_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 0));
        if (set_switch_state(cur_device, switch_name, sqlite3_column_int(stmt, 1)) == ERROR_SWITCH) {
          log_message(LOG_WARNING, "Error setting switcher %s on device %s", switch_name, cur_device->name);
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
  int sql_result, row_result, result=1;
  
  // Do not initiate elements on device connect, because it takes a few seconds to pair elements
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    sql_query = sqlite3_mprintf("SELECT di_name, di_value FROM an_dimmer WHERE de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", cur_device->name);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query set_startup_all_dimmer");
      result = 0;
    } else {
      row_result = sqlite3_step(stmt);
      while (row_result == SQLITE_ROW) {
        snprintf(dimmer_name, WORDLENGTH*sizeof(char), "%s", (char*)sqlite3_column_text(stmt, 0));
        if (set_dimmer_value(cur_device, dimmer_name, sqlite3_column_int(stmt, 1)) == ERROR_DIMMER) {
          log_message(LOG_WARNING, "Error setting dimmer %s on device %s", dimmer_name, cur_device->name);
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
  
  sql_query = sqlite3_mprintf("SELECT he.he_id, he.he_name, de.de_name, he.he_enabled, he.he_set, he.he_max_heat_value\
                    FROM an_heater he LEFT OUTER JOIN an_device de on de.de_id = he.de_id\
                    WHERE he.de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", device);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (get_startup_heater_status)");
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
  int heat_status=1, i=0;
  
  if (cur_device != NULL && cur_device->enabled && cur_device->type != TYPE_ZWAVE) {
    heaters = get_startup_heater_status(sqlite3_db, cur_device->name);
    for (i=0; heaters[i].id != -1; i++) {
      cur_heater = set_heater(sqlite3_db, cur_device, heaters[i].name, heaters[i].set, heaters[i].heat_max_value);
      if (cur_heater != NULL) {
        heat_status = 0;
      }
      free(cur_heater);
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
char * get_monitor(sqlite3 * sqlite3_db, const char * device_name, const char * switcher_name, const char * sensor_name, const char * start_date) {
  char * sql_query = NULL, * where_switch = NULL, * where_sensor = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, first_result = 1, t_len, str_len;
  char p_device[WORDLENGTH+1], p_switch[WORDLENGTH+1], p_sensor[WORDLENGTH+1], p_start_date[WORDLENGTH+1], monitor_date[WORDLENGTH+1], monitor_value[WORDLENGTH+1], * one_item = NULL, * all_items = NULL;;
  time_t yesterday;
  char * to_return = NULL;
  
  snprintf(p_device, WORDLENGTH*sizeof(char), "%s", device_name);
  snprintf(p_switch, WORDLENGTH*sizeof(char), "%s", switcher_name);
  snprintf(p_sensor, WORDLENGTH*sizeof(char), "%s", sensor_name);
  if (start_date == NULL) {
    time(&yesterday);
    yesterday -= 60*60*24; // set start_date to yesterday
    snprintf(p_start_date, WORDLENGTH*sizeof(char), "%ld", yesterday);
  } else {
    snprintf(p_start_date, WORDLENGTH*sizeof(char), "%s", start_date);
  }
  
  if (switcher_name != NULL && 0 != strcmp("0", switcher_name)) {
    where_switch = sqlite3_mprintf("AND mo.sw_id = (SELECT sw_id FROM an_switch WHERE sw_name='%q'\
                      AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_switch, p_device);
  } else {
    strcpy(p_switch, "");
    where_switch = sqlite3_mprintf("");
  }
  
  if (sensor_name != NULL && 0 != strcmp("0", sensor_name)) {
    where_sensor = sqlite3_mprintf("AND mo.se_id = (SELECT se_id FROM an_sensor WHERE se_name='%q'\
                      AND de_id=(SELECT de_id FROM an_device WHERE de_name='%q'))", p_sensor, p_device);
  } else {
    strcpy(p_sensor, "");
    where_sensor = sqlite3_mprintf("");
  }
  
  sql_query = sqlite3_mprintf("SELECT mo.mo_date, mo.mo_result FROM an_monitor mo\
                    LEFT OUTER JOIN an_device de ON de.de_id = mo.de_id LEFT OUTER JOIN an_switch sw ON sw.sw_id = mo.sw_id\
                    LEFT OUTER JOIN an_sensor se ON se.se_id = mo.se_id WHERE mo.de_id = (SELECT de_id FROM an_device WHERE de_name='%q')\
                    AND datetime(mo.mo_date, 'unixepoch') >= datetime('%q', 'unixepoch') %s %s ORDER BY mo.mo_date ASC",
                    p_device, p_start_date, where_switch, where_sensor);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  sqlite3_free(where_switch);
  sqlite3_free(where_sensor);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (get_monitor)");
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
    t_len = snprintf(NULL, 0, json_template_tools_get_monitor, device_name, p_switch, p_sensor, p_start_date, all_items);
    to_return = malloc((t_len+1)*sizeof(char));
    snprintf(to_return, (t_len+1)*sizeof(char), json_template_tools_get_monitor, device_name, p_switch, p_sensor, p_start_date, all_items);
    free(one_item);
    free(all_items);
  }
  sqlite3_finalize(stmt);
  return to_return;
}

/**
 * archive the journal data from the main db to the archive db, until epoch_from, then vacuum the main db
 */
int archive_journal(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, result = 0;
  
  if (sqlite3_db != NULL && sqlite3_archive_db != NULL) {
    sql_query = sqlite3_mprintf("SELECT jo_date, jo_origin, jo_command, jo_result FROM an_journal WHERE jo_date < '%d'", epoch_from);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query (archive_journal)");
      sqlite3_finalize(stmt);
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        while (row_result == SQLITE_ROW) {
          sql_query = sqlite3_mprintf("INSERT INTO an_journal (jo_date, jo_origin, jo_command, jo_result) VALUES ('%q', '%q', '%q', '%q')", 
                           (char*)sqlite3_column_text(stmt, 0),
                           (char*)sqlite3_column_text(stmt, 1),
                           (char*)sqlite3_column_text(stmt, 2),
                           (char*)sqlite3_column_text(stmt, 3));
          if ( sqlite3_exec(sqlite3_archive_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
            log_message(LOG_WARNING, "Error archiving journal");
            sqlite3_finalize(stmt);
          }
          sqlite3_free(sql_query);
          row_result = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        sql_query = sqlite3_mprintf("DELETE FROM an_journal WHERE jo_date < '%d'; vacuum", epoch_from);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          log_message(LOG_INFO, "End archiving journal, limit date %d", epoch_from);
          result = 1;
        } else {
          log_message(LOG_WARNING, "Error deleting old journal data");
        }
        sqlite3_free(sql_query);
      } else {
        log_message(LOG_INFO, "End archiving journal, no data archived, limit date %d", epoch_from);
        sqlite3_finalize(stmt);
        result = 1;
      }
    }
  }
  return result;
}

/**
 * archive the monitor data from the main db to the archive db, until epoch_from, then vacuum the main db
 */
int archive_monitor(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from) {
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, result = 0;
  
  if (sqlite3_db != NULL && sqlite3_archive_db != NULL) {
    sql_query = sqlite3_mprintf("SELECT mo_date, de_id, sw_id, se_id, he_id, di_id, mo_result FROM an_monitor WHERE mo_date < '%d'", epoch_from);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query (archive_monitor)");
      sqlite3_finalize(stmt);
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        while (row_result == SQLITE_ROW) {
          sql_query = sqlite3_mprintf("INSERT INTO an_monitor (mo_date, de_id, sw_id, se_id, he_id, di_id, mo_result) VALUES ('%q', '%d', '%d', '%d', '%d', '%d', '%q')", 
                           (char*)sqlite3_column_text(stmt, 0),
                           sqlite3_column_int(stmt, 1),
                           sqlite3_column_int(stmt, 2),
                           sqlite3_column_int(stmt, 3),
                           sqlite3_column_int(stmt, 4),
                           sqlite3_column_int(stmt, 5),
                           (char*)sqlite3_column_text(stmt, 6));
          if ( sqlite3_exec(sqlite3_archive_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
            log_message(LOG_WARNING, "Error archiving monitor");
            sqlite3_finalize(stmt);
          }
          sqlite3_free(sql_query);
          row_result = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        sql_query = sqlite3_mprintf("DELETE FROM an_monitor WHERE mo_date < '%d'; vacuum", epoch_from);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
          log_message(LOG_INFO, "End archiving monitor, limit date %d", epoch_from);
          result = 1;
        } else {
          log_message(LOG_WARNING, "Error deleting old monitor data");
        }
        sqlite3_free(sql_query);
      } else {
        log_message(LOG_INFO, "End archiving monitor, no data archived, limit date %d", epoch_from);
        sqlite3_finalize(stmt);
        result = 1;
      }
    }
  }
  return result;
}

/**
 * archive journal and monitor data from main db, until epoch_from
 */
int archive(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from) {
  return archive_journal(sqlite3_db, sqlite3_archive_db, epoch_from) && archive_monitor(sqlite3_db, sqlite3_archive_db, epoch_from);
}

/**
 * Thread run to archive data
 */
void * thread_archive_run(void * args) {
  sqlite3 * sqlite3_db = ((struct archive_args *) args)->sqlite3_db, * sqlite3_archive_db;
  char * db_archive_path = ((struct archive_args *) args)->db_archive_path;
  unsigned int epoch_from = ((struct archive_args *) args)->epoch_from;
  char * sql_query, * sql_query2;
  int rc, la_id;
  
  if (!is_archive_running(db_archive_path)) {
    rc = sqlite3_open_v2(db_archive_path, &sqlite3_archive_db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK && sqlite3_archive_db != NULL) {
      log_message(LOG_WARNING, "Database error: %s", sqlite3_errmsg(sqlite3_archive_db));
    } else {
      sql_query = sqlite3_mprintf("INSERT INTO an_archive_list (la_date_begin, la_status) VALUES (strftime('%%s', 'now'), 0)");
      if ( sqlite3_exec(sqlite3_archive_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
        la_id = (int)sqlite3_last_insert_rowid(sqlite3_archive_db);
        if (archive(sqlite3_db, sqlite3_archive_db, epoch_from)) {
          sql_query2 = sqlite3_mprintf("UPDATE an_archive_list SET la_date_end = strftime('%%s', 'now'), la_status = 1 WHERE la_id = '%d'", la_id);
        } else {
          sql_query2 = sqlite3_mprintf("UPDATE an_archive_list SET la_date_end = strftime('%%s', 'now'), la_status = 2 WHERE la_id = '%d'", la_id);
        }
        if ( sqlite3_exec(sqlite3_archive_db, sql_query2, NULL, NULL, NULL) != SQLITE_OK ) {
          log_message(LOG_WARNING, "Error logging archiving end date");
        }
        sqlite3_free(sql_query2);
      } else {
        log_message(LOG_WARNING, "Error logging archiving start date");
      }
      sqlite3_free(sql_query);
    }
    sqlite3_close(sqlite3_archive_db);
  }
  return NULL;
}

/**
 * Return the epoch time of the last archive
 */
unsigned int get_last_archive(char * db_archive_path) {
  int rc;
  sqlite3 * sqlite3_archive_db;
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result, result = 0;
  
  rc = sqlite3_open_v2(db_archive_path, &sqlite3_archive_db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK && sqlite3_archive_db != NULL) {
    log_message(LOG_WARNING, "Database error: %s", sqlite3_errmsg(sqlite3_archive_db));
    sqlite3_close(sqlite3_archive_db);
    return 0;
  } else {
    sql_query = sqlite3_mprintf("SELECT la_date_end FROM an_archive_list WHERE la_status=1 ORDER BY la_date_end DESC LIMIT 1");
    sql_result = sqlite3_prepare_v2(sqlite3_archive_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query (get_last_archive)");
    } else {
      row_result = sqlite3_step(stmt);
      if (row_result == SQLITE_ROW) {
        result = strtol((char*)sqlite3_column_text(stmt, 0), NULL, 10);
      }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(sqlite3_archive_db);
  }
  return result;
}

/**
 * Return true if an archive is currently running
 */
int is_archive_running(char * db_archive_path) {
  int rc;
  sqlite3 * sqlite3_archive_db;
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  rc = sqlite3_open_v2(db_archive_path, &sqlite3_archive_db, SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK && sqlite3_archive_db != NULL) {
    log_message(LOG_WARNING, "Database error: %s", sqlite3_errmsg(sqlite3_archive_db));
    sqlite3_close(sqlite3_archive_db);
    return 0;
  } else {
    sql_query = sqlite3_mprintf("SELECT la_id FROM an_archive_list WHERE la_status=0");
    sql_result = sqlite3_prepare_v2(sqlite3_archive_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    sqlite3_free(sql_query);
    if (sql_result != SQLITE_OK) {
      row_result = sqlite3_step(stmt);
      sqlite3_finalize(stmt);
      sqlite3_close(sqlite3_archive_db);
      return (row_result == SQLITE_ROW);
    } else {
      sqlite3_finalize(stmt);
      sqlite3_close(sqlite3_archive_db);
      return 0;
    }
  }
}

/**
 * Send a message to syslog
 * and prints the message to stdout if DEBUG mode is on
 */
void log_message(int type, const char * message, ...) {
	va_list argp;
#ifdef DEBUG
  char * out = NULL;
  int out_len = 0;
#endif
  
#ifndef DEBUG
  if (type != LOG_DEBUG) {
#endif
    if (message != NULL) {
      va_start(argp, message);
      openlog("Angharad", LOG_PID|LOG_CONS, LOG_USER);
      vsyslog( type, message, argp );
      closelog();
#ifdef DEBUG
      out_len = strlen(message)+1;
      out = malloc(out_len+1);
      snprintf(out, (out_len+1)*sizeof(char), "%s\n", message);
      vfprintf(stdout, out, argp);
      free(out);
#endif
      va_end(argp);
    }
#ifndef DEBUG
  }
#endif
}

/**
 * Logs the commands, its origin (remote ip address or internal schedule) and its result into the database journal table
 */
int journal(sqlite3 * sqlite3_db, const char * origin, const char * command, const char * result) {
  char * sql_query = NULL;
  int i_result;
  
  sql_query = sqlite3_mprintf("INSERT INTO an_journal (jo_date, jo_origin, jo_command, jo_result) VALUES (strftime('%%s', 'now'), '%q', '%q', '%q')", origin, command, result);
  i_result = ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
  sqlite3_free(sql_query);
  return i_result;
}

/**
 * get the commands list on json format
 * it returns the content of the api_rest.json compiled as a binary file
 * It's not magic ! You MUST manually update the api_rest.json file if you add/remove/modify output commands
 */
char * get_json_list_commands() {
  extern uint8_t binary_api_rest_json_size[] asm("_binary_api_rest_json_size");
  extern char _binary_api_rest_json_start;
  extern char _binary_api_rest_json_end;
  
  size_t json_list_len = (size_t)((void *)binary_api_rest_json_size);
  char * json_list_data = malloc(json_list_len+sizeof(char));
  char * json_list_p = &_binary_api_rest_json_start;
  int json_list_i = 0;
  while ( json_list_p != &_binary_api_rest_json_end ) {
    json_list_data[json_list_i++] = *json_list_p++;
  }
  json_list_data[json_list_i] = '\0';
  return json_list_data;
}