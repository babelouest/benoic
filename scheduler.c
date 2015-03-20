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
 * Scheduler functions
 *
 */

#include "angharad.h"

static const char json_template_scheduler_enableschedule[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat\":%d,\"repeat_value\":%d,\"script\":%s}";
static const char json_template_scheduler_empty[] = "{}";
static const char json_template_scheduler_getschedules[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"device\":\"%s\",\"next_time\":%ld,\"repeat\":%d,\"repeat_value\":%d,\"remove_after_done\":%d,\"tags\":%s,\"script\":%s}";
static const char json_template_scheduler_setschedule[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat_schedule\":%d,\"repeat_schedule_value\":%d,\"remove_after_done\":%d,\"device\":\"%s\",\"script\":%d,\"tags\":%s}";
static const char json_template_scheduler_addschedule[] = "{\"id\":%d,\"name\":\"%s\",\"enabled\":%s,\"next_time\":%ld,\"repeat_schedule\":%d,\"repeat_schedule_value\":%d,\"remove_after_done\":%d,\"device\":\"%s\",\"script\":%d,\"tags\":%s}";

/**
 * Thread function for the scheduler
 * So if the last scheduled scripts are not finished to run
 * (because of a long sleep function for example),
 * The next scheduler doesn't have to wait for the previous to finish.
 */
void * thread_scheduler_run(void * args) {
  char sql_query[MSGLENGTH+1];
  int sql_result, row_result;
  switcher sw;
  sensor se;
  sqlite3_stmt *stmt;

  // Get configuration variables
  struct config_elements * config = (struct config_elements *) args;
  
  // Run scheduler manager
  run_scheduler(config->sqlite3_db, config->terminal, config->nb_terminal, config->script_path);
  
  
  // Monitor switches
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw.sw_id, sw.sw_name, de.de_name, sw.sw_monitored_every, sw.sw_monitored_next\
                    FROM an_switch sw, an_device de WHERE sw_monitored=1 AND de.de_id = sw.de_id");
  sql_result = sqlite3_prepare_v2(config->sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (switches monitored)");
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      sw.id = sqlite3_column_int(stmt, 0);
      snprintf(sw.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(sw.device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      sw.monitored_every = sqlite3_column_int(stmt, 3);
      sw.monitored_next = (time_t)sqlite3_column_int(stmt, 4);
      monitor_switch(config->sqlite3_db, config->terminal, config->nb_terminal, sw);
      row_result = sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);
  stmt = NULL;

  // Monitor sensors
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT se.se_id, se.se_name, de.de_name, se.se_monitored_every, se.se_monitored_next\
                    FROM an_sensor se, an_device de WHERE se_monitored=1 AND de.de_id = se.de_id");
  sql_result = sqlite3_prepare_v2(config->sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (sensors monitored)");
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      se.id = sqlite3_column_int(stmt, 0);
      snprintf(se.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(se.device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      se.monitored_every = sqlite3_column_int(stmt, 3);
      se.monitored_next = (time_t)sqlite3_column_int(stmt, 4);
      monitor_sensor(config->sqlite3_db, config->terminal, config->nb_terminal, se);
      row_result = sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);
  stmt = NULL;
  pthread_exit((void *)0);
  return NULL;
}

/**
 * Main thread launched periodically
 */
int run_scheduler(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, char * script_path) {
  // Look for every enabled scheduler in the database
  sqlite3_stmt *stmt;
  int sql_result, row_result, i;
  char sql_query[MSGLENGTH+1], buf[MSGLENGTH+1];
  schedule cur_schedule;
  
  // Send a heartbeat to all devices
  // If not responding, try to reconnect device
  for (i=0; i<nb_terminal; i++) {
    if (!send_heartbeat(terminal[i])) {
      log_message(LOG_INFO, "Connection attempt to %s, result: %s", terminal[i]->name, reconnect_device(terminal[i], terminal, nb_terminal)!=-1?"Success":"Error");
      if (terminal[i]->enabled) {
        log_message(LOG_INFO, "Initialization of %s, result: %s", terminal[i]->name, init_device_status(sqlite3_db, terminal[i])==1?"Success":"Error");
      }
    }
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh_id, sh_name, sh_next_time, sh_repeat_schedule, sh_repeat_schedule_value, sc_id, sh_enabled, sh_remove_after_done FROM an_scheduler WHERE sh_enabled = 1");
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (run_scheduler)");
    sqlite3_finalize(stmt);
    return 0;
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      // Set the schedule object
      cur_schedule.id = sqlite3_column_int(stmt, 0);
      snprintf(cur_schedule.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      cur_schedule.next_time = (time_t)sqlite3_column_int(stmt, 2);
      cur_schedule.repeat_schedule = sqlite3_column_int(stmt, 3);
      cur_schedule.repeat_schedule_value = sqlite3_column_int(stmt, 4);
      cur_schedule.script = sqlite3_column_int(stmt, 5);
      cur_schedule.enabled = sqlite3_column_int(stmt, 6);
      cur_schedule.remove_after_done = sqlite3_column_int(stmt, 7);
      
      if (is_scheduled_now(cur_schedule.next_time)) {
        // Run the specified script
        log_message(LOG_WARNING, "Scheduled script \"%s\", (id: %d)", cur_schedule.name, cur_schedule.id);
        snprintf(buf, WORDLENGTH, "%d", cur_schedule.script);
        if (!run_script(sqlite3_db, terminal, nb_terminal, script_path, buf)) {
          log_message(LOG_WARNING, "Script \"%s\" failed", cur_schedule.name);
        } else {
          snprintf(buf, MSGLENGTH, "run_script \"%s\", (id: %d) finished", cur_schedule.name, cur_schedule.id);
          journal(sqlite3_db, "scheduler", buf, "success");
        }
      }
      // Update the scheduler
      update_schedule(sqlite3_db, &cur_schedule);
      row_result = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return 1;
  }
}

/**
 * Evaluates if the schedule is due now (within a minute) or not
 */
int is_scheduled_now(time_t next_time) {
  time_t now;
  time(&now);
  
  if (next_time != 0) {
    return ((now - next_time)>=0 && (now - next_time) <= 60);
  }
  return 0;
}

/**
 * Updates the schedule
 * If next_time+60 sec is in the future, then do nothing
 * else if no repeat or duration, disable it
 * else if repeat and no duration, calculate the next occurence, then update next_time value with it
 * else if duration, calcultate if the schedule is outside of the duration, then update next_time if needed
 */
int update_schedule(sqlite3 * sqlite3_db, schedule * sc) {
  time_t now, next_time;
  time(&now);
  
  if (sc->next_time < now) {
    if (sc->repeat_schedule != REPEAT_NONE) {
      // Schedule is in the past, calculate new date
      do {
        next_time = calculate_next_time(sc->next_time, sc->repeat_schedule, sc->repeat_schedule_value);
        if (next_time) {
          // Set the new next_time
          sc->next_time = next_time;
        }
      } while (next_time < now);
      return update_schedule_db(sqlite3_db, *sc);
    } else if (sc->repeat_schedule == REPEAT_NONE) {
      // Schedule is done now
      if (sc->remove_after_done) {
        // Remove from the database
        return remove_schedule_db(sqlite3_db, *sc);
      } else {
        // Disable it in the database
        sc->next_time = 0;
        sc->enabled = 0;
        return update_schedule_db(sqlite3_db, *sc);
      }
    }
  }
  // Schedule is in the future, do nothing
  return 1;
}

/**
 * Calculate the next time
 */
time_t calculate_next_time(time_t from, int schedule_type, unsigned int schedule_value) {
  struct tm ts = *localtime(&from);
  time_t to_return;
  int isdst_from, isdst_to;
  
  isdst_from = ts.tm_isdst;
  
  switch (schedule_type) {
    case REPEAT_MINUTE:
      ts.tm_min += (schedule_value);
      to_return = mktime(&ts);
      isdst_to = ts.tm_isdst;
      break;
    case REPEAT_HOUR:
      ts.tm_hour += (schedule_value);
      to_return = mktime(&ts);
      isdst_to = ts.tm_isdst;
      break;
    case REPEAT_DAY:
      ts.tm_mday += (schedule_value);
      to_return = mktime(&ts);
      isdst_to = ts.tm_isdst;
      break;
    case REPEAT_DAY_OF_WEEK:
      if (schedule_value != 0) {
        do {
          ts.tm_wday++;
          ts.tm_mday++;
          ts.tm_wday%=7;
        } while (!((int)pow(2, (ts.tm_wday)) & schedule_value));
        to_return = mktime(&ts);
        isdst_to = ts.tm_isdst;
      } else {
        to_return = 0;
        isdst_to = isdst_from;
      }
      break;
    case REPEAT_MONTH:
      ts.tm_mon += schedule_value;
      to_return = mktime(&ts);
      isdst_to = ts.tm_isdst;
      break;
    case REPEAT_YEAR:
      ts.tm_year += schedule_value;
      to_return = mktime(&ts);
      isdst_to = ts.tm_isdst;
      break;
    default:
      to_return = 0;
      isdst_to = ts.tm_isdst;
      break;
  }
  
  // Adjusting next time with Daylight saving time if changed
  // when schedule_type is REPEAT_DAY, REPEAT_DAY_OF_WEEK, REPEAT_MONTH, REPEAT_YEAR
  if (schedule_type & (REPEAT_DAY|REPEAT_DAY_OF_WEEK|REPEAT_MONTH|REPEAT_YEAR)) {
    if (isdst_from < isdst_to) {
      ts.tm_hour--;
      to_return = mktime(&ts);
    } else if (isdst_from > isdst_to) {
      ts.tm_hour++;
      to_return = mktime(&ts);
    }
  }
  return (to_return);
}

/**
 * Update a scheduler in the database
 */
int update_schedule_db(sqlite3 * sqlite3_db, schedule sc) {
  char sql_query[MSGLENGTH+1];
  int sql_result;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, 
           "UPDATE an_scheduler SET sh_enabled='%d', sh_next_time='%ld' WHERE sh_id='%d'",
           sc.enabled,
           (long)sc.next_time,
           sc.id
  );
  sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
  return ( sql_result == SQLITE_OK );
}

/**
 * Remove a schedule already done without repeat
 */
int remove_schedule_db(sqlite3 * sqlite3_db, schedule sc) {
  char sql_query[MSGLENGTH+1];
  int sql_result;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, 
                   "DELETE FROM an_scheduler WHERE sh_id='%d'",
                   sc.id
  );
  sql_result = sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL);
  return ( sql_result == SQLITE_OK );
}

/**
 * Monitor one switch and update next time value with the every value
 */
int monitor_switch(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, switcher sw) {
  device * cur_terminal;
  char sw_state[WORDLENGTH+1] = {0};
  char sql_query[MSGLENGTH+1] = {0};
  time_t now, next_time;
  int was_ran = 0;
  int switch_value = 0;
  
  time(&now);
  if (is_scheduled_now(sw.monitored_next) || sw.monitored_next < now) {
    // Monitor switch state
    cur_terminal = get_device_from_name(sw.device, terminal, nb_terminal);
    was_ran = 1;
    switch_value = get_switch_state(cur_terminal, sw.name+3, 1);
    if (switch_value != ERROR_SWITCH) {
      snprintf(sw_state, WORDLENGTH, "%d", switch_value);
      if (!monitor_store(sqlite3_db, sw.device, sw.name, "", sw_state)) {
        log_message(LOG_WARNING, "Error storing switch state monitor value into database");
      }
    }
  }
  
  if (was_ran || (sw.monitored_next <= now && sw.monitored_every > 0)) {
    next_time = calculate_next_time(now, REPEAT_MINUTE, sw.monitored_every);
    sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_switch SET sw_monitored_next = '%ld' WHERE sw_id = '%d'", next_time, sw.id);
    return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
  } else {
    return 1;
  }
}

/**
 * Monitor one sensor and update next time value with the every value
 */
int monitor_sensor(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, sensor s) {
  device * cur_terminal;
  char se_value[WORDLENGTH+1] = {0};
  char sql_query[MSGLENGTH+1] = {0};
  time_t now, next_time;
  int was_ran=0;
  float sensor_value=0;
  
  time(&now);
  
  if (is_scheduled_now(s.monitored_next) || s.monitored_next < now) {
    // Monitor sensor data
    cur_terminal = get_device_from_name(s.device, terminal, nb_terminal);

    was_ran = 1;
    sensor_value = get_sensor_value(cur_terminal, s.name, 1);
    if (sensor_value != ERROR_SENSOR) {
      snprintf(se_value, WORDLENGTH, "%.2f", sensor_value);
      if (!monitor_store(sqlite3_db, s.device, "", s.name, se_value)) {
        log_message(LOG_WARNING, "Error storing sensor data monitor value into database");
      }
    }
  }
  
  if (was_ran || (s.monitored_next <= now && s.monitored_every > 0)) {
    next_time = calculate_next_time(now, REPEAT_MINUTE, s.monitored_every);
    sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_sensor SET se_monitored_next = '%ld' WHERE se_id = '%d'", next_time, s.id);
    return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
  } else {
    return 1;
  }
}

/**
 * Insert monitor value into database
 */
int monitor_store(sqlite3 * sqlite3_db, const char * device_name, const char * switch_name, const char * sensor_name, const char * value) {
  char sql_query[MSGLENGTH+1];
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_monitor (mo_date, de_id, sw_id, se_id, mo_result)\
                  VALUES (strftime('%%s','now'), (SELECT de_id FROM an_device WHERE de_name = '%q'),\
                  (SELECT sw_id FROM an_switch WHERE sw_name = '%q'\
                  AND de_id = (SELECT de_id FROM an_device WHERE de_name = '%q')),\
                  (SELECT se_id FROM an_sensor WHERE se_name = '%q' and de_id = (SELECT de_id FROM an_device WHERE de_name = '%q')), '%q')",
                  device_name, switch_name, device_name, sensor_name, device_name, value);
  
  return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
}

/**
 * Add a schedule into the database
 */
int add_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  
  if (0 == strcmp(cur_schedule.name, "") ||
      (cur_schedule.next_time == 0 && cur_schedule.repeat_schedule == -1) ||
      (cur_schedule.repeat_schedule > -1 && cur_schedule.repeat_schedule_value == 0) ||
      cur_schedule.script == 0) {
    log_message(LOG_WARNING, "Error inserting schedule, wrong params");
    return 0;
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_scheduler (sh_name, sh_enabled, sh_next_time, sh_repeat_schedule,\
                  sh_repeat_schedule_value, sh_remove_after_done, de_id, sc_id)\
                  VALUES ('%q', '%d', '%d', '%d', '%d', '%d', (SELECT de_id FROM an_device WHERE de_name='%q'), '%d')", 
                  cur_schedule.name, cur_schedule.enabled, cur_schedule.next_time, cur_schedule.repeat_schedule, 
                  cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, cur_schedule.device, cur_schedule.script);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    cur_schedule.id = (int)sqlite3_last_insert_rowid(sqlite3_db);
    snprintf(str_id, WORDLENGTH, "%d", cur_schedule.id);
    tags = build_tags_from_list(cur_schedule.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCHEDULE, str_id, tags);
    
    snprintf(command_result, (MSGLENGTH*2), json_template_scheduler_addschedule,
              cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time,
              cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done,
              cur_schedule.device, cur_schedule.script, tags_json);
    
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_WARNING, "Error inserting action");
    return 0;
  }
}

/**
 * Modify the specified schedule
 */
int set_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, str_id[WORDLENGTH+1];
  
  if (cur_schedule.id == 0 || 
      0 == strcmp(cur_schedule.name, "") || 
      (cur_schedule.next_time == 0 && cur_schedule.repeat_schedule == -1) || 
      (cur_schedule.repeat_schedule > -1 && cur_schedule.repeat_schedule_value == 0) || 
      cur_schedule.script == 0) {
    log_message(LOG_WARNING, "Error updating schedule, wrong params");
    return 0;
  }
  
  sqlite3_snprintf(MSGLENGTH, sql_query, 
                  "UPDATE an_scheduler SET sh_name='%q', sh_enabled='%d', sh_next_time='%d', sh_repeat_schedule='%d', sh_repeat_schedule_value='%d',\
                  sh_remove_after_done='%d', de_id=(SELECT de_id FROM an_device WHERE de_name='%q'),\
                  sc_id='%d' WHERE sh_id='%d'", 
                  cur_schedule.name, cur_schedule.enabled, cur_schedule.next_time, cur_schedule.repeat_schedule, 
                  cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, cur_schedule.device, cur_schedule.script, 
                  cur_schedule.id);
                  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    snprintf(str_id, WORDLENGTH, "%d", cur_schedule.id);
    tags = build_tags_from_list(cur_schedule.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_SCHEDULE, str_id, tags);
    snprintf(command_result, (MSGLENGTH*2), json_template_scheduler_setschedule, 
              cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time, 
              cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, cur_schedule.remove_after_done, 
              cur_schedule.device, cur_schedule.script, tags_json);
    free(tags_json);
    free_tags(tags);
    return 1;
  } else {
    log_message(LOG_WARNING, "Error updating action");
    return 0;
  }
}

/**
 * Delete the specified schedule
 */
int delete_schedule(sqlite3 * sqlite3_db, char * schedule_id) {
  char sql_query[MSGLENGTH+1];
  
  if (schedule_id == NULL || 0 == strcmp("", schedule_id)) {log_message(LOG_WARNING, "Error deleting schedule, wrong params"); return 0;}
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag_element WHERE sh_id='%q'", schedule_id);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_tag WHERE ta_id NOT IN (SELECT DISTINCT (ta_id) FROM an_tag)");
    if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
      sqlite3_snprintf(MSGLENGTH, sql_query, "DELETE FROM an_scheduler WHERE sh_id='%q'", schedule_id);
      return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
    } else {
      log_message(LOG_WARNING, "Error deleting tag");
      return 0;
    }
  } else {
    log_message(LOG_WARNING, "Error deleting tag_element");
    return 0;
  }
}

/**
 * Get all the schedules for the specified device
 * Or all the schedules if device is NULL
 */
char * get_schedules(sqlite3 * sqlite3_db, char * device) {
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  char sql_query[MSGLENGTH+1], * one_item = NULL, cur_name[WORDLENGTH+1], cur_device[WORDLENGTH+1],
  cur_schedule[WORDLENGTH+1], * tags = NULL, ** tags_array = NULL, script_id[WORDLENGTH+1], * scripts = malloc(2*sizeof(char)), * script = NULL;
  int cur_id, remove_after_done = 0, str_len;
  
  long next_time;
  int enabled, repeat_schedule, repeat_schedule_value;
  
  if (device == NULL) {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh.sh_id, sh.sh_name, sh.sh_enabled, sh.sh_next_time, sh.sh_repeat_schedule,\
                      sh.sh_repeat_schedule_value, sh.sc_id, de.de_name, sh.sh_remove_after_done FROM an_scheduler sh\
                      LEFT OUTER JOIN an_device de ON de.de_id = sh.de_id");
  } else {
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh.sh_id, sh.sh_name, sh.sh_enabled, sh.sh_next_time, sh.sh_repeat_schedule,\
                      sh.sh_repeat_schedule_value, sh.sc_id, de.de_name, sh.sh_remove_after_done FROM an_scheduler sh\
                      LEFT OUTER JOIN an_device de ON de.de_id = sh.de_id\
                      WHERE sh.de_id IN (SELECT de_id FROM an_device WHERE de_name = '%q')", device);
  }
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (get_schedules)");
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
        strcpy(script, json_template_scheduler_empty);
      }
      sanitize_json_string(cur_name, cur_name, WORDLENGTH);
      
      str_len = snprintf(NULL, 0, json_template_scheduler_getschedules,
              cur_id, cur_name, enabled?"true":"false", cur_device, next_time, repeat_schedule, repeat_schedule_value,
              remove_after_done, tags, script);
      one_item = malloc((str_len+1)*sizeof(char));
      
      snprintf(one_item, (str_len+1), json_template_scheduler_getschedules,
              cur_id, cur_name, enabled?"true":"false", cur_device, next_time, repeat_schedule, repeat_schedule_value,
              remove_after_done, tags, script);
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
    sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh_id, sh_name, sh_enabled, sh_next_time, sh_repeat_schedule,\
                      sh_repeat_schedule_value, sc_id FROM an_scheduler WHERE sh_id='%q'", schedule);
    sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
    if (sql_result != SQLITE_OK) {
      log_message(LOG_WARNING, "Error preparing sql query (enable_schedule)");
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
          log_message(LOG_WARNING, "Error updating schedule on database");
        }
        snprintf(script_id, WORDLENGTH, "%d", sqlite3_column_int(stmt, 6));
        script = get_script(sqlite3_db, script_id, 0);
        if (script == NULL) {
          script = malloc(3*sizeof(char));
          strcpy(script, json_template_scheduler_empty);
        }
        sanitize_json_string(cur_schedule.name, cur_schedule.name, WORDLENGTH);
        snprintf(command_result, MSGLENGTH, json_template_scheduler_enableschedule,
                cur_schedule.id, cur_schedule.name, cur_schedule.enabled?"true":"false", cur_schedule.next_time,
                cur_schedule.repeat_schedule, cur_schedule.repeat_schedule_value, script);
        sqlite3_finalize(stmt);
        free(script);
        return 1;
      } else {
        log_message(LOG_WARNING, "Error getting schedule data");
        sqlite3_finalize(stmt);
        return 0;
      }
    }
  } else {
    log_message(LOG_WARNING, "Error updating schedule");
    sqlite3_finalize(stmt);
    return 0;
  }
}
