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

#define NB_SECONDS_PER_DAY 86400

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
    printf("device_ptr for %s is allocated\n", json_string_value(json_object_get(device, "name")));
    *device_ptr = msprintf("I already said to %s that I'm Batman!", json_string_value(json_object_get(device, "name")));
  }
  
  if (strstr(json_string_value(json_object_get(json_object_get(device, "options"), "device_specified")), "batman") == NULL) {
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
    printf("device_ptr for %s is free\n", json_string_value(json_object_get(device, "name")));
    free(device_ptr);
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
  return json_pack("{sis{sfsf}s{sisi}s{sisi}s{s{sssfso}s{sssfso}}}",
                   "result", RESULT_OK,
                   "sensors", "se1", get_sensor_value("se1"), "se2", get_sensor_value("se2"),
                   "switches", "sw1", 0, "sw2", 1,
                   "dimmers", "di1", 42, "di2", 5,
                   "heaters", 
                     "he1", "mode", "auto", "command", 18.0, "on", json_true(),
                     "he2", "mode", "manual", "command", 20.0, "on", json_false());
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
    return json_pack("{sisi}", "result", RESULT_OK, "value", 0);
  } else if (0 == nstrcmp(switch_name, "sw2")) {
    return json_pack("{sisi}", "result", RESULT_OK, "value", 1);
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
    return json_pack("{sisi}", "result", RESULT_OK, "value", 42);
  } else if (0 == nstrcmp(dimmer_name, "di2")) {
    return json_pack("{sisi}", "result", RESULT_OK, "value", 5);
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
    return json_pack("{sisssfso}", "result", RESULT_OK, "mode", "auto", "value", 18.0, "on", json_true());
  } else if (0 == nstrcmp(heater_name, "he2")) {
    return json_pack("{sisssfso}", "result", RESULT_OK, "mode", "manual", "value", 20.0, "on", json_false());
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
