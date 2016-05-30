/**
 *
 * Benoic House Automation service
 *
 * Command house automation devices via an HTTP REST interface
 *
 * Mock device module
 * Provides all the commands for a fake device
 * Used to develop and validate benoic infrastructure above
 *
 * Copyright 2016 Nicolas Mora <mail@babelouest.org>
 *
 * Licence MIT
 *
 */

#include <string.h>
#include <jansson.h>
#include <math.h>
#include <time.h>
#include <yder.h>
#include <orcania.h>

#define RESULT_ERROR     0
#define RESULT_OK        1
#define RESULT_NOT_FOUND 2
#define RESULT_TIMEOUT   3

#define ELEMENT_TYPE_NONE   0
#define ELEMENT_TYPE_SENSOR 1
#define ELEMENT_TYPE_SWITCH 2
#define ELEMENT_TYPE_DIMMER 3
#define ELEMENT_TYPE_HEATER 4

#define BENOIC_ELEMENT_HEATER_MODE_CURRENT -1
#define BENOIC_ELEMENT_HEATER_MODE_OFF     0
#define BENOIC_ELEMENT_HEATER_MODE_MANUAL  1
#define BENOIC_ELEMENT_HEATER_MODE_AUTO    2
#define BENOIC_ELEMENT_HEATER_MODE_ERROR   3

#define NB_SECONDS_PER_DAY 86400

json_t * b_device_get_switch (json_t * device, const char * switch_name, void * device_ptr);
json_t * b_device_get_dimmer (json_t * device, const char * dimmer_name, void * device_ptr);
json_t * b_device_get_heater (json_t * device, const char * heater_name, void * device_ptr);

/**
 * Get the value sensor of a sensor using a sinus model
 */
double get_sensor_value(const char * sensor_name) {
  time_t now = time(0);
  struct tm * local = localtime(&now);
  int current_nb_seconds = local->tm_sec + (60 * local->tm_min) + (60 * 60 * local->tm_hour);
  if (0 == nstrcmp(sensor_name, "se1")) {
    return sin((double)current_nb_seconds / (double)NB_SECONDS_PER_DAY);
  } else {
    return ((sin(((double)((current_nb_seconds + (NB_SECONDS_PER_DAY / 2)) % NB_SECONDS_PER_DAY)) / (double)NB_SECONDS_PER_DAY) * 5.0) + 15.0);
  }
}

/**
 * Initializes the device type by getting its uid, name and description
 */
json_t * b_device_type_init () {
  json_t * options = json_array();
  json_array_append_new(options, json_pack("{ssssssso}", "name", "uri", "type", "string", "description", "uri to connect to the device", "optional", json_false()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "baud", "type", "numeric", "description", "speed of the device communication", "optional", json_false()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "do_not_check_certificate", "type", "boolean", "description", "check the certificate of the device if needed", "optional", json_true()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "device_specified", "type", "string", "description", "specified by the device when connected for the first time, then must be sent back at every other connection", "optional", json_true()));
  return json_pack("{sissssssso}", 
                    "result", RESULT_OK,
                    "uid", "00-00-00", 
                    "name", "Another Mock Device", 
                    "description", "This is another mock device, for development and debug purposes", 
                    "options", options);
}

/**
 * connects the device
 */
json_t * b_device_connect (json_t * device, void ** device_ptr) {
  char * param;
  json_t * j_param;
  
  if (device_ptr != NULL) {
    // Allocating *device_ptr for further use
    *device_ptr = (json_t *)json_pack("{s{sisi}s{sisi}s{s{sssfso}s{sssfso}}}",
                             "switches", "sw1", 0, "sw2", 1,
                             "dimmers", "di1", 42, "di2", 5,
                             "heaters", 
                               "he1", "mode", "auto", "command", 18.0, "on", json_true(),
                               "he2", "mode", "manual", "command", 20.0, "on", json_false());
  }
  
  if (nstrstr(json_string_value(json_object_get(json_object_get(device, "options"), "device_specified")), "batman") == NULL) {
    param = msprintf("%s says I'm batman", json_string_value(json_object_get(device, "name")));
    j_param = json_pack("{sis{ss}}", "result", RESULT_OK, "options", "device_specified", param);
    free(param);
  } else {
    j_param = json_pack("{si}", "result", RESULT_OK);
  }
  return j_param;
}

/**
 * disconnects the device
 */
json_t * b_device_disconnect (json_t * device, void * device_ptr) {
  if (device_ptr != NULL) {
    // Free device_ptr
    json_decref((json_t *)device_ptr);
  }
  return json_pack("{si}", "result", RESULT_OK);
}

/**
 * Ping the device type
 */
json_t * b_device_ping (json_t * device, void * device_ptr) {
  return json_pack("{si}", "result", RESULT_OK);
}

/**
 * Get the device overview
 * Returns a mocked overview with 2 sensors, 2 switches, 2 dimmers and 2 heaters
 */
json_t * b_device_overview (json_t * device, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command overview for device %s", json_string_value(json_object_get(device, "name")));
  json_t * sw1 = b_device_get_switch(device, "sw1", device_ptr),
         * sw2 = b_device_get_switch(device, "sw2", device_ptr),
         * di1 = b_device_get_dimmer(device, "di1", device_ptr),
         * di2 = b_device_get_dimmer(device, "di1", device_ptr),
         * he1 = b_device_get_heater(device, "he1", device_ptr),
         * he2 = b_device_get_heater(device, "he2", device_ptr);
  json_object_del(he1, "result");
  json_object_del(he2, "result");
    
  json_t * result = json_pack("{sis{sfsf}s{sIsI}s{sIsI}s{soso}}",
                             "result", 
                             RESULT_OK,
                             "sensors", 
                                "se1", get_sensor_value("se1"), 
                                "se2", get_sensor_value("se2"),
                             "switches", 
                                "sw1", json_integer_value(json_object_get(sw1, "value")), 
                                "sw2", json_integer_value(json_object_get(sw2, "value")),
                             "dimmers", 
                                "di1", json_integer_value(json_object_get(di1, "value")), 
                                "di2", json_integer_value(json_object_get(di2, "value")),
                             "heaters", 
                               "he1", he1,
                               "he2", he2);
  json_decref(sw1);
  json_decref(sw2);
  json_decref(di1);
  json_decref(di2);
  return result;
}

/**
 * Get the sensor value
 */
json_t * b_device_get_sensor (json_t * device, const char * sensor_name, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command sensor for sensor %s on device %s", sensor_name, json_string_value(json_object_get(device, "name")));
  if (0 == nstrcmp(sensor_name, "se1") || 0 == nstrcmp(sensor_name, "se2")) {
    return json_pack("{sisf}", "result", RESULT_OK, "value", get_sensor_value(sensor_name));
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Get the switch value
 */
json_t * b_device_get_switch (json_t * device, const char * switch_name, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command get_switch for switch %s on device %s", switch_name, json_string_value(json_object_get(device, "name")));
  if (0 == nstrcmp(switch_name, "sw1")) {
    return json_pack("{sisi}", "result", RESULT_OK, "value", json_integer_value(json_object_get(json_object_get((json_t *)device_ptr, "switches"), "sw1")));
  } else if (0 == nstrcmp(switch_name, "sw2")) {
    return json_pack("{sisi}", "result", RESULT_OK, "value", json_integer_value(json_object_get(json_object_get((json_t *)device_ptr, "switches"), "sw2")));
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Set the switch command
 */
json_t * b_device_set_switch (json_t * device, const char * switch_name, const int command, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command set_switch for switch %s on device %s with the value %d", switch_name, json_string_value(json_object_get(device, "name")), command);
  if (0 == nstrcmp(switch_name, "sw1") || 0 == nstrcmp(switch_name, "sw2")) {
    json_object_del(json_object_get((json_t *)device_ptr, "switches"), switch_name);
    json_object_set_new(json_object_get((json_t *)device_ptr, "switches"), switch_name, json_integer(command));
    return json_pack("{si}", "result", RESULT_OK);
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Get the dimmer value
 */
json_t * b_device_get_dimmer (json_t * device, const char * dimmer_name, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command get_dimmer for dimmer %s on device %s", dimmer_name, json_string_value(json_object_get(device, "name")));
  if (0 == nstrcmp(dimmer_name, "di1")) {
    return json_pack("{sisi}", "result", RESULT_OK, "value", json_integer_value(json_object_get(json_object_get((json_t *)device_ptr, "dimmers"), dimmer_name)));
  } else if (0 == nstrcmp(dimmer_name, "di2")) {
    return json_pack("{sisi}", "result", RESULT_OK, "value", json_integer_value(json_object_get(json_object_get((json_t *)device_ptr, "dimmers"), dimmer_name)));
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Set the dimmer command
 */
json_t * b_device_set_dimmer (json_t * device, const char * dimmer_name, const int command, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command set_dimmer for dimmer %s on device %s with the value %d", dimmer_name, json_string_value(json_object_get(device, "name")), command);
  if (0 == nstrcmp(dimmer_name, "di1") || 0 == nstrcmp(dimmer_name, "di2")) {
    json_object_del(json_object_get((json_t *)device_ptr, "dimmers"), dimmer_name);
    json_object_set_new(json_object_get((json_t *)device_ptr, "dimmers"), dimmer_name, json_integer(command));
    return json_pack("{si}", "result", RESULT_OK);
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Get the heater value
 */
json_t * b_device_get_heater (json_t * device, const char * heater_name, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command get_heater for heater %s on device %s", heater_name, json_string_value(json_object_get(device, "name")));
  if (0 == nstrcmp(heater_name, "he1")) {
    json_t * heater = json_copy(json_object_get(json_object_get((json_t *)device_ptr, "heaters"), heater_name));
    json_object_set_new(heater, "result", json_integer(RESULT_OK));
    return heater;
  } else if (0 == nstrcmp(heater_name, "he2")) {
    json_t * heater = json_copy(json_object_get(json_object_get((json_t *)device_ptr, "heaters"), heater_name));
    json_object_set_new(heater, "result", json_integer(RESULT_OK));
    return heater;
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Set the heater command
 */
json_t * b_device_set_heater (json_t * device, const char * heater_name, const int mode, const float command, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Running command set_heater for heater %s on device %s with the value %f and the mode %d", heater_name, json_string_value(json_object_get(device, "name")), command, mode);
  if (0 == nstrcmp(heater_name, "he1") || 0 == nstrcmp(heater_name, "he2")) {
    json_t * heater = json_object_get(json_object_get((json_t *)device_ptr, "heaters"), heater_name);
    json_object_del(heater, "mode");
    switch (mode) {
      case BENOIC_ELEMENT_HEATER_MODE_MANUAL:
        json_object_set_new(heater, "mode", json_string("manual"));
        break;
      case BENOIC_ELEMENT_HEATER_MODE_AUTO:
        json_object_set_new(heater, "mode", json_string("auto"));
        break;
      default:
        json_object_set_new(heater, "mode", json_string("off"));
        break;
    }
    json_object_del(heater, "command");
    json_object_set_new(heater, "command", json_real(command));
    return json_pack("{si}", "result", RESULT_OK);
  } else {
    return json_pack("{si}", "result", RESULT_NOT_FOUND);
  }
}

/**
 * Return true if an element with the specified name and the specified type exist in this device
 */
int b_device_has_element (json_t * device, int element_type, const char * element_name, void * device_ptr) {
  y_log_message(Y_LOG_LEVEL_INFO, "device-mock - Checking if element '%s' of type %d exists in device %s", element_name, element_type, json_string_value(json_object_get(device, "name")));
  switch (element_type) {
    case ELEMENT_TYPE_SENSOR:
      return (0 == nstrcmp(element_name, "se1") || 0 == nstrcmp(element_name, "se2"));
      break;
    case ELEMENT_TYPE_SWITCH:
      return (0 == nstrcmp(element_name, "sw1") || 0 == nstrcmp(element_name, "sw2"));
      break;
    case ELEMENT_TYPE_DIMMER:
      return (0 == nstrcmp(element_name, "di1") || 0 == nstrcmp(element_name, "di2"));
      break;
    case ELEMENT_TYPE_HEATER:
      return (0 == nstrcmp(element_name, "he1") || 0 == nstrcmp(element_name, "he2"));
      break;
    default:
      return 0;
      break;
  }
}
