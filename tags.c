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
      sqlite3_snprintf(MSGLENGTH, where_element, "sw_id = (SELECT sw_id FROM an_switch WHERE sw_name = '%q'\
                        AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_SENSOR:
      sqlite3_snprintf(MSGLENGTH, where_element, "se_id = (SELECT se_id FROM an_sensor WHERE se_name = '%q'\
                        AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_HEATER:
      sqlite3_snprintf(MSGLENGTH, where_element, "he_id = (SELECT he_id FROM an_heater WHERE he_name = '%q'\
                        AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
      break;
    case DATA_DIMMER:
      sqlite3_snprintf(MSGLENGTH, where_element, "di_id = (SELECT di_id FROM an_dimmer WHERE di_name = '%q'\
                        AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
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
  sqlite3_snprintf(MSGLENGTH, sql_query, "SELECT ta_name FROM an_tag WHERE ta_id IN (SELECT ta_id FROM an_tag_element WHERE %s)", where_element);
  
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  to_return = malloc(sizeof(char *));
  to_return[0] = NULL;
  if (sql_result != SQLITE_OK) {
    log_message(LOG_WARNING, "Error preparing sql query (get_tags)");
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
  char sql_query[MSGLENGTH+1] = {0}, where_element[MSGLENGTH+1] = {0}, element_row[WORDLENGTH+1] = {0};
  int counter = 0, cur_tag = -1;
  
  if (tags != NULL) {
    switch (element_type) {
      case DATA_DEVICE:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT de_id FROM an_device WHERE de_name = '%q')", element);
        strcpy(element_row, "de_id");
        break;
      case DATA_SWITCH:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT sw_id FROM an_switch WHERE sw_name = '%q'\
                          AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "sw_id");
        break;
      case DATA_SENSOR:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT se_id FROM an_sensor WHERE se_name = '%q'\
                          AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "se_id");
        break;
      case DATA_HEATER:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT he_id FROM an_heater WHERE he_name = '%q'\
                          AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
        strcpy(element_row, "he_id");
        break;
      case DATA_DIMMER:
        sqlite3_snprintf(MSGLENGTH, where_element, "(SELECT di_id FROM an_dimmer WHERE di_name = '%q'\
                          AND de_id = (SELECT de_id FROM an_device WHERE de_name='%q'))", element, device_name);
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
      log_message(LOG_WARNING, "Error deleting old tags (%s)", sql_query);
      return -1;
    } else {
      while (tags[counter] != NULL) {
        cur_tag = get_or_create_tag_id(sqlite3_db, tags[counter]);
        sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_tag_element (%s, ta_id) VALUES (%s, %d)", element_row, where_element, cur_tag);
        if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) != SQLITE_OK ) {
          log_message(LOG_WARNING, "Error inserting tag_element %s (%d)", tags[counter], cur_tag);
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
      log_message(LOG_WARNING, "Error preparing sql query (get_or_create_tag_id)");
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
