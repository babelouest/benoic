#include "angharad.h"

/**
 * Thread function for the scheduler
 * So if the last scheduled scripts are not finished to run
 * (because of a long sleep function for example),
 * The next scheduler doesn't have to wait for the previous to finish.
 */
void * thread_scheduler_run(void * args) {
  char sql_query[MSGLENGTH+1];
  int sql_result, row_result;
  pin p;
  sensor s;
  sqlite3_stmt *stmt;

  // Get configuration variables
  struct config_elements * config = (struct config_elements *) args;
  
  // Run scheduler manager
  run_scheduler(config->sqlite3_db, config->terminal, config->nb_terminal);
  
  // Monitor switches
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sw.sw_id, sw.sw_name, de.de_name, sw.sw_monitored_every, sw.sw_monitored_next FROM an_switch sw, an_device de WHERE sw_monitored=1 AND de.de_id = sw.de_id");
  sql_result = sqlite3_prepare_v2(config->sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query (switches monitored)");
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      p.id = sqlite3_column_int(stmt, 0);
      snprintf(p.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(p.device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      p.monitored_every = sqlite3_column_int(stmt, 3);
      p.monitored_next = (time_t)sqlite3_column_int(stmt, 4);
      monitor_switch(config->sqlite3_db, config->terminal, config->nb_terminal, p);
      row_result = sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);
  stmt = NULL;
  
  // Monitor sensors
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT se.se_id, se.se_name, de.de_name, se.se_monitored_every, se.se_monitored_next FROM an_sensor se, an_device de WHERE se_monitored=1 AND de.de_id = se.de_id");
  sql_result = sqlite3_prepare_v2(config->sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query (sensors monitored)");
  } else {
    row_result = sqlite3_step(stmt);
    while (row_result == SQLITE_ROW) {
      s.id = sqlite3_column_int(stmt, 0);
      snprintf(s.name, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 1));
      snprintf(s.device, WORDLENGTH, "%s", (char*)sqlite3_column_text(stmt, 2));
      s.monitored_every = sqlite3_column_int(stmt, 3);
      s.monitored_next = (time_t)sqlite3_column_int(stmt, 4);
      monitor_sensor(config->sqlite3_db, config->terminal, config->nb_terminal, s);
      row_result = sqlite3_step(stmt);
    }
  }
  sqlite3_finalize(stmt);
  pthread_exit((void *)0);
  return NULL;
}

/**
 * Main thread launched periodically
 */
int run_scheduler(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal) {
  // Look for every enabled scheduler in the database
  sqlite3_stmt *stmt;
  int sql_result, row_result, i;
  char sql_query[MSGLENGTH+1], buf[MSGLENGTH+1];
  schedule cur_schedule;
  
  // Send a heartbeat to all devices
  // If not responding, try to reconnect device
  for (i=0; i<nb_terminal; i++) {
    if (!send_heartbeat(terminal[i])) {
      snprintf(buf, MSGLENGTH, "Connection attempt to %s, result: %s", terminal[i]->name, reconnect_device(terminal[i], terminal, nb_terminal)!=-1?"Success":"Error");
      log_message(LOG_INFO, buf);
      if (terminal[i]->enabled) {
        snprintf(buf, MSGLENGTH, "Initialization of %s, result: %s", terminal[i]->name, init_device_status(sqlite3_db, terminal[i])==1?"Success":"Error");
        log_message(LOG_INFO, buf);
      }
    }
  }
  
  //struct tm ts;
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT sh_id, sh_name, sh_next_time, sh_repeat_schedule, sh_repeat_schedule_value, sc_id, sh_enabled FROM an_scheduler WHERE sh_enabled = 1");
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_INFO, "Error preparing sql query");
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
      
      if (is_scheduled_now(cur_schedule.next_time)) {
        // Run the specified script
        snprintf(buf, MSGLENGTH, "Scheduled script \"%s\", (id: %d)", cur_schedule.name, cur_schedule.id);
        log_message(LOG_INFO, buf);
        snprintf(buf, WORDLENGTH, "%d", cur_schedule.script);
        if (!run_script(sqlite3_db, terminal, nb_terminal, buf)) {
          snprintf(buf, MSGLENGTH, "Script \"%s\" failed", cur_schedule.name);
          log_message(LOG_INFO, buf);
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
      // Disable it in the database
      sc->next_time = 0;
      sc->enabled = 0;
      return update_schedule_db(sqlite3_db, *sc);
    }
  }
  // Schedule is in the future, do nothing
  return 1;
}

/**
 * Calculate the next time
 */
time_t calculate_next_time(time_t from, int schedule_type, unsigned int schedule_value) {
  struct tm ts;
  
  switch (schedule_type) {
    case REPEAT_MINUTE:
      return (from+60*schedule_value);
      break;
    case REPEAT_HOUR:
      return (from+60*60*schedule_value);
      break;
    case REPEAT_DAY:
      return (from+60*60*24*schedule_value);
      break;
    case REPEAT_DAY_OF_WEEK:
      if (schedule_value != 0) {
        ts = *localtime(&from);
        do {
          ts.tm_wday++;
          ts.tm_wday%=7;
          from += 60*60*24;
        } while (!((int)pow(2, (ts.tm_wday)) & schedule_value));
        return from;
      } else {
        return 0;
      }
      break;
    case REPEAT_MONTH:
      ts = *localtime(&from);
      ts.tm_mon += schedule_value;
      return mktime(&ts);
      break;
    case REPEAT_YEAR:
      ts = *localtime(&from);
      ts.tm_year += schedule_value;
      return mktime(&ts);
      break;
    default:
      return 0;
      break;
  }
  return 0;
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
 * Monitor one switch and update next time value with the every value
 */
int monitor_switch(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, pin p) {
  device * cur_terminal;
  char sw_state[WORDLENGTH+1];
  char sql_query[MSGLENGTH+1];
  time_t now, next_time;
  int was_ran = 0;
  
  time(&now);
  
  if (is_scheduled_now(p.monitored_next) || p.monitored_next < now) {
    // Monitor switch state
    cur_terminal = get_device_from_name(p.device, terminal, nb_terminal);
    was_ran = 1;
    snprintf(sw_state, WORDLENGTH, "%d", get_switch_state(cur_terminal, p.name+3, 1));
    if (!monitor_store(sqlite3_db, p.device, p.name, "", sw_state)) {
      log_message(LOG_INFO, "Error storing switch state monitor value into database");
    }
  }
  
  if (was_ran || (p.monitored_next <= now && p.monitored_every > 0)) {
    next_time = calculate_next_time(now, REPEAT_MINUTE, p.monitored_every);
    sqlite3_snprintf(MSGLENGTH, sql_query, "UPDATE an_switch SET sw_monitored_next = '%ld' WHERE sw_id = '%d'", next_time, p.id);
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
  char se_value[WORDLENGTH+1];
  char sql_query[MSGLENGTH+1];
  time_t now, next_time;
  int was_ran=0;
  
  time(&now);
  
  if (is_scheduled_now(s.monitored_next) || s.monitored_next < now) {
    // Monitor sensor data
    cur_terminal = get_device_from_name(s.device, terminal, nb_terminal);

    was_ran = 1;
    snprintf(se_value, WORDLENGTH, "%.2f", get_sensor_value(cur_terminal, s.name, 1));
    if (!monitor_store(sqlite3_db, s.device, "", s.name, se_value)) {
      log_message(LOG_INFO, "Error storing sensor data monitor value into database");
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
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_monitor (mo_date, de_id, sw_id, se_id, mo_result) VALUES (strftime('%%s','now'), (SELECT de_id FROM an_device WHERE de_name = '%q'), (SELECT sw_id FROM an_switch WHERE sw_name = '%q'), (SELECT se_id FROM an_sensor WHERE se_name = '%q'), '%q')", device_name, switch_name, sensor_name, value);
  return ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK );
}