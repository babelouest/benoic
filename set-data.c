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
 * Entry point file
 *
 */

#include "angharad.h"

static const char json_template_set_data_setdimmerdata[] = "{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"tags\":%s}";
static const char json_template_set_data_setheaterdata[] = "{\"name\":\"%s\",\"display\":\"%s\",\"unit\":\"%s\",\"enabled\":%s,\"tags\":%s}";
static const char json_template_set_data_setsensordata[] = "{\"name\":\"%s\",\"display\":\"%s\",\"unit\":\"%s\",\"enabled\":%s,\"tags\":%s}";
static const char json_template_set_data_setswitchdata[] = "{\"name\":\"%s\",\"display\":\"%s\",\"type\":%d,\"enabled\":%s,\"tags\":%s}";
static const char json_template_set_data_setdevicedata[] = "{\"name\":\"%s\",\"display\":\"%s\",\"enabled\":%s,\"tags\":%s}";

/**
 * Change the display name and the enable settings for a device
 */
char * set_device_data(sqlite3 * sqlite3_db, device cur_device) {
  char sql_query[MSGLENGTH+1], ** tags = NULL, * tags_json = NULL, * to_return = NULL;
  int str_len=0;
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_device (de_id, de_name, de_display, de_active)\
                    VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d')",
                    cur_device.name, cur_device.name, cur_device.display, cur_device.enabled);
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_device.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, NULL, DATA_DEVICE, cur_device.name, tags);
    sanitize_json_string(cur_device.name, cur_device.name, WORDLENGTH);
    sanitize_json_string(cur_device.display, cur_device.display, WORDLENGTH);
    str_len = snprintf(NULL, 0, json_template_set_data_setdevicedata, cur_device.name, cur_device.display, cur_device.enabled?"true":"false", tags_json);
    to_return = malloc((str_len+1)*sizeof(char));
    snprintf(to_return, (str_len+1), json_template_set_data_setdevicedata, cur_device.name, cur_device.display, cur_device.enabled?"true":"false", tags_json);
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
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_switch\
                    (sw_id, de_id, sw_name, sw_display, sw_type, sw_active, sw_status, sw_monitored, sw_monitored_every, sw_monitored_next)\
                    VALUES ((SELECT sw_id FROM an_switch WHERE sw_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d', '%d',\
                    (SELECT sw_status FROM an_switch WHERE sw_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    '%d', '%d', 0)", cur_switch.name, cur_switch.device, cur_switch.device, cur_switch.name, cur_switch.display,
                    cur_switch.type, cur_switch.enabled, cur_switch.name, cur_switch.device, cur_switch.monitored, cur_switch.monitored_every);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_switch.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_switch.device, DATA_SWITCH, cur_switch.name, tags);
    sanitize_json_string(cur_switch.name, cur_switch.name, WORDLENGTH);
    sanitize_json_string(cur_switch.display, cur_switch.display, WORDLENGTH);
    str_len = snprintf(NULL, 0, json_template_set_data_setswitchdata, cur_switch.name, cur_switch.display, cur_switch.type, cur_switch.enabled?"true":"false", tags_json);
    to_return = malloc((str_len+1)*sizeof(char));
    snprintf(to_return, (str_len+1), json_template_set_data_setswitchdata, cur_switch.name, cur_switch.display, cur_switch.type, cur_switch.enabled?"true":"false", tags_json);
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
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_sensor (se_id, de_id, se_name, se_display, se_unit, se_active,\
                    se_monitored, se_monitored_every, se_monitored_next) VALUES\
                    ((SELECT se_id FROM an_sensor WHERE se_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%q', '%d', '%d', '%d', 0)",
                    cur_sensor.name, cur_sensor.device, cur_sensor.device, cur_sensor.name, cur_sensor.display, cur_sensor.unit,
                    cur_sensor.enabled, cur_sensor.monitored, cur_sensor.monitored_every);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_sensor.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_sensor.device, DATA_SENSOR, cur_sensor.name, tags);
    sanitize_json_string(cur_sensor.name, cur_sensor.name, WORDLENGTH);
    sanitize_json_string(cur_sensor.display, cur_sensor.display, WORDLENGTH);
    sanitize_json_string(cur_sensor.unit, cur_sensor.unit, WORDLENGTH);
    str_len = snprintf(NULL, 0, json_template_set_data_setsensordata, cur_sensor.name, cur_sensor.display, cur_sensor.unit, cur_sensor.enabled?"true":"false", tags_json);
    to_return = malloc((str_len+1)*sizeof(char));
    snprintf(to_return, (str_len+1), json_template_set_data_setsensordata, cur_sensor.name, cur_sensor.display, cur_sensor.unit, cur_sensor.enabled?"true":"false", tags_json);
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
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_heater (he_id, de_id, he_name, he_display, he_unit, he_enabled)\
                    VALUES ((SELECT he_id FROM an_heater WHERE he_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%q', '%d')",
                    cur_heater.name, cur_heater.device, cur_heater.device, cur_heater.name, cur_heater.display, cur_heater.unit,
                    cur_heater.enabled);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_heater.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_heater.device, DATA_HEATER, cur_heater.name, tags);
    sanitize_json_string(cur_heater.name, cur_heater.name, WORDLENGTH);
    sanitize_json_string(cur_heater.display, cur_heater.display, WORDLENGTH);
    sanitize_json_string(cur_heater.unit, cur_heater.unit, WORDLENGTH);
    str_len = snprintf(NULL, 0, json_template_set_data_setheaterdata, cur_heater.name, cur_heater.display, cur_heater.unit, cur_heater.enabled?"true":"false", tags_json);
    to_return = malloc((str_len+1)*sizeof(char));
    snprintf(to_return, (str_len+1), json_template_set_data_setheaterdata, cur_heater.name, cur_heater.display, cur_heater.unit, cur_heater.enabled?"true":"false", tags_json);
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
  
  sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT OR REPLACE INTO an_dimmer (di_id, de_id, di_name, di_display, di_active)\
                    VALUES ((SELECT di_id FROM an_dimmer WHERE di_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')),\
                    (SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d')",
                    cur_dimmer.name, cur_dimmer.device, cur_dimmer.device, cur_dimmer.name, cur_dimmer.display, cur_dimmer.enabled);
  
  if ( sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
    tags = build_tags_from_list(cur_dimmer.tags);
    tags_json = build_json_tags(tags);
    set_tags(sqlite3_db, cur_dimmer.device, DATA_DIMMER, cur_dimmer.name, tags);
    sanitize_json_string(cur_dimmer.name, cur_dimmer.name, WORDLENGTH);
    sanitize_json_string(cur_dimmer.display, cur_dimmer.display, WORDLENGTH);
    str_len = snprintf(NULL, 0, json_template_set_data_setdimmerdata, cur_dimmer.name, cur_dimmer.display, cur_dimmer.enabled?"true":"false", tags_json);
    to_return = malloc((str_len+1)*sizeof(char));
    snprintf(to_return, (str_len+1), json_template_set_data_setdimmerdata, cur_dimmer.name, cur_dimmer.display, cur_dimmer.enabled?"true":"false", tags_json);
    free(tags_json);
    free_tags(tags);
  }
  return to_return;
}
