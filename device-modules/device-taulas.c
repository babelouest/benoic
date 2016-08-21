/**
 *
 * Benoic House Automation service
 *
 * Command house automation devices via an HTTP REST interface
 *
 * Taulas device module
 * Provides all the commands for a Taulas device
 * 
 * Copyright 2016 Nicolas Mora <mail@babelouest.org>
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

#include <jansson.h>
#include <yder.h>
#include <orcania.h>
#include <ulfius.h>

#define WEBSERVICE_RESULT_ERROR     0
#define WEBSERVICE_RESULT_OK        1
#define WEBSERVICE_RESULT_NOT_FOUND 2
#define WEBSERVICE_RESULT_TIMEOUT   3
#define WEBSERVICE_RESULT_PARAM     4

#define ELEMENT_TYPE_NONE   0
#define ELEMENT_TYPE_SENSOR 1
#define ELEMENT_TYPE_SWITCH 2
#define ELEMENT_TYPE_DIMMER 3
#define ELEMENT_TYPE_HEATER 4

#define BENOIC_ELEMENT_HEATER_MODE_OFF     "off"
#define BENOIC_ELEMENT_HEATER_MODE_MANUAL  "manual"
#define BENOIC_ELEMENT_HEATER_MODE_AUTO    "auto"

#define NB_SECONDS_PER_DAY 86400

json_t * b_device_get_switch (json_t * device, const char * switch_name, void * device_ptr);
json_t * b_device_get_dimmer (json_t * device, const char * dimmer_name, void * device_ptr);
json_t * b_device_get_heater (json_t * device, const char * heater_name, void * device_ptr);

void init_request_for_device(struct _u_request * req, json_t * device, const char * command) {
  ulfius_init_request(req);
  if (json_object_get(json_object_get(device, "options"), "do_not_check_certificate") == json_true()) {
    req->check_server_certificate = 0;
  }
  if (json_object_get(json_object_get(device, "options"), "user") != NULL && json_string_length(json_object_get(json_object_get(device, "options"), "user")) > 0) {
    req->auth_basic_user = nstrdup(json_string_value(json_object_get(json_object_get(device, "options"), "user")));
  }
  if (json_object_get(json_object_get(device, "options"), "user") != NULL && json_string_length(json_object_get(json_object_get(device, "options"), "user")) > 0) {
    req->auth_basic_user = nstrdup(json_string_value(json_object_get(json_object_get(device, "options"), "user")));
  }
  req->http_url = msprintf("%s/%s", json_string_value(json_object_get(json_object_get(device, "options"), "uri")), command);
}

/**
 * Initializes the device type by getting its uid, name and description
 */
json_t * b_device_type_init () {
  json_t * options = json_array();
  json_array_append_new(options, json_pack("{ssssssso}", "name", "uri", "type", "string", "description", "uri to connect to the device", "optional", json_false()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "do_not_check_certificate", "type", "boolean", "description", "check the certificate of the device if needed", "optional", json_true()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "old_version", "type", "boolean", "description", "Is the device an old Taulas device or a new one?", "optional", json_true()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "user", "type", "string", "description", "Username to connect to the device", "optional", json_true()));
  json_array_append_new(options, json_pack("{ssssssso}", "name", "password", "type", "string", "description", "Password to connect to the device", "optional", json_true()));
  return json_pack("{sissssssso}", 
                    "result", WEBSERVICE_RESULT_OK,
                    "uid", "24-67-85", 
                    "name", "Taulas Device", 
                    "description", "Connect to a Taulas device", 
                    "options", options);
}

/**
 * connects the device
 */
json_t * b_device_connect (json_t * device, void ** device_ptr) {
  struct _u_request req;
  int res;
  json_t * j_param;
  
  * device_ptr = malloc(sizeof(struct _u_map));
  u_map_init((struct _u_map *)*device_ptr);
  
  init_request_for_device(&req, device, "MARCO");
  
  res = ulfius_send_http_request(&req, NULL);
  if (res == U_OK) {
    j_param = json_pack("{si}", "result", WEBSERVICE_RESULT_OK);
  } else {
    j_param = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  ulfius_clean_request(&req);

  return j_param;
}

/**
 * disconnects the device
 */
json_t * b_device_disconnect (json_t * device, void * device_ptr) {
  u_map_clean_full((struct _u_map *)device_ptr);
  return json_pack("{si}", "result", WEBSERVICE_RESULT_OK);
}

/**
 * Ping the device type
 */
json_t * b_device_ping (json_t * device, void * device_ptr) {
  struct _u_request req;
  struct _u_response resp;
  int res;
  json_t * j_param;
  
  init_request_for_device(&req, device, "MARCO");
  ulfius_init_response(&resp);
  
  res = ulfius_send_http_request(&req, &resp);
  if (res == U_OK && nstrcmp("POLO", resp.string_body)) {
    j_param = json_pack("{si}", "result", WEBSERVICE_RESULT_OK);
  } else {
    j_param = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  ulfius_clean_request(&req);
  ulfius_clean_response(&resp);

  return j_param;
}

/**
 * decode a u_map into a string
 */
char * print_map(const struct _u_map * map) {
  char * line, * to_return = NULL;
  const char **keys;
  int len, i;
  if (map != NULL) {
    keys = u_map_enum_keys(map);
    for (i=0; keys[i] != NULL; i++) {
      len = snprintf(NULL, 0, "key is %s, value is %s\n", keys[i], u_map_get(map, keys[i]));
      line = malloc((len+1)*sizeof(char));
      snprintf(line, (len+1), "key is %s, value is %s\n", keys[i], u_map_get(map, keys[i]));
      if (to_return != NULL) {
        len = strlen(to_return) + strlen(line) + 1;
        to_return = realloc(to_return, (len+1)*sizeof(char));
      } else {
        to_return = malloc((strlen(line) + 1)*sizeof(char));
        to_return[0] = 0;
      }
      strcat(to_return, line);
      free(line);
    }
    return to_return;
  } else {
    return NULL;
  }
}

/**
 * Get the device overview
 * Returns a mocked overview with 2 sensors, 2 switches, 2 dimmers and 2 heaters
 */
json_t * b_device_overview (json_t * device, void * device_ptr) {
  struct _u_request req;
  struct _u_response resp;
  int res;
  json_t * overview;
  char * saved_body, * str, * token, * token_dup, * saveptr, * element, * saveptr2, * name, * value, * saveptr3, * endptr;
  json_int_t i_value;
  double d_value;
  struct _u_map * elements = (struct _u_map *)device_ptr;
  
  init_request_for_device(&req, device, "OVERVIEW");
  ulfius_init_response(&resp);
  
  res = ulfius_send_http_request(&req, &resp);
  if (res == U_OK) {
    overview = json_object();
    
    saved_body = nstrdup(resp.string_body);
    saved_body[strlen(saved_body) - 1] = '\0';
    str = saved_body;
    token = strtok_r(str + sizeof(char), ";", &saveptr);
    while (token != NULL) {
      token_dup = nstrdup(token);
      if (nstrncmp("SWITCHES", token_dup, strlen("SWITCHES")) == 0 && nstrstr(token_dup, ",") != NULL) {
        json_object_set_new(overview, "switches", json_object());
        element = strtok_r(token_dup + strlen("SWITCHES,"), ",", &saveptr2);
        while (element != NULL) {
          name = strtok_r(element, ":", &saveptr3);
          value = strtok_r(NULL, ":", &saveptr3);
          if (name != NULL && value != NULL) {
            i_value = strtol(value, &endptr, 10);
            if (value != endptr) {
              json_object_set_new(json_object_get(overview, "switches"), name, json_integer(i_value));
              u_map_put(elements, name, "");
            }
          }
          element = strtok_r(NULL, ",", &saveptr2);
        }
      } else if (nstrncmp("SENSORS", token_dup, strlen("SENSORS")) == 0 && nstrstr(token_dup, ",") != NULL) {
        json_object_set_new(overview, "sensors", json_object());
        element = strtok_r(token_dup + strlen("SENSORS,"), ",", &saveptr2);
        while (element != NULL) {
          name = strtok_r(element, ":", &saveptr3);
          value = strtok_r(NULL, ":", &saveptr3);
          if (name != NULL && value != NULL) {
            d_value = strtod(value, &endptr);
            if (value == endptr) {
              i_value = strtol(value, &endptr, 10);
              if (value == endptr) {
                json_object_set_new(json_object_get(overview, "sensors"), name, json_string(value));
                u_map_put(elements, name, "");
              } else {
                json_object_set_new(json_object_get(overview, "sensors"), name, json_integer(i_value));
                u_map_put(elements, name, "");
              }
            } else {
              json_object_set_new(json_object_get(overview, "sensors"), name, json_real(d_value));
              u_map_put(elements, name, "");
            }
          }
          element = strtok_r(NULL, ",", &saveptr2);
        }
      } else if (nstrncmp("DIMMERS", token_dup, strlen("DIMMERS")) == 0 && nstrstr(token_dup, ",") != NULL) {
        json_object_set_new(overview, "dimmers", json_object());
        element = strtok_r(token_dup + strlen("DIMMERS,"), ",", &saveptr2);
        while (element != NULL) {
          name = strtok_r(element, ":", &saveptr3);
          value = strtok_r(NULL, ":", &saveptr3);
          if (name != NULL && value != NULL) {
            i_value = strtol(value, &endptr, 10);
            if (value != endptr) {
              json_object_set_new(json_object_get(overview, "dimmers"), name, json_integer(i_value));
              u_map_put(elements, name, "");
            }
          }
          element = strtok_r(NULL, ",", &saveptr2);
        }
      }
      free(token_dup);
      token = strtok_r(NULL, ";", &saveptr);
    }
    free(saved_body);
    json_object_set_new(overview, "result", json_integer(WEBSERVICE_RESULT_OK));
  } else {
    overview = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  ulfius_clean_request(&req);
  ulfius_clean_response(&resp);

  return overview;
}

/**
 * Get the sensor value
 */
json_t * b_device_get_sensor (json_t * device, const char * sensor_name, void * device_ptr) {
  struct _u_request req;
  struct _u_response resp;
  int res;
  char * path, * endptr, * value;
  json_t * j_result = NULL;
  json_int_t i_value;
  double d_value;
  
  path = msprintf("%s/%s", "SENSOR", sensor_name);
  init_request_for_device(&req, device, path);
  ulfius_init_response(&resp);
  
  res = ulfius_send_http_request(&req, &resp);
  if (res == U_OK) {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_OK);
    value = resp.string_body + sizeof(char);
    value[strlen(value) - 1] = '\0';
    d_value = strtod(value, &endptr);
    if (resp.string_body == endptr) {
      i_value = strtol(value, &endptr, 10);
      if (resp.string_body == endptr) {
        json_object_set_new(j_result, "value", json_string(value));
      } else {
        json_object_set_new(j_result, "value", json_integer(i_value));
      }
    } else {
      json_object_set_new(j_result, "value", json_real(d_value));
    }
  } else {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  free(path);
  ulfius_clean_request(&req);
  ulfius_clean_response(&resp);
  return j_result;
}

/**
 * Get the switch value
 */
json_t * b_device_get_switch (json_t * device, const char * switch_name, void * device_ptr) {
  struct _u_request req;
  struct _u_response resp;
  int res;
  char * path, * endptr, * value;
  json_t * j_result = NULL;
  json_int_t i_value;
  
  path = msprintf("%s/%s", "GETSWITCH", switch_name);
  init_request_for_device(&req, device, path);
  ulfius_init_response(&resp);
  
  res = ulfius_send_http_request(&req, &resp);
  if (res == U_OK) {
    value = resp.string_body + sizeof(char);
    value[strlen(value) - 1] = '\0';
      i_value = strtol(value, &endptr, 10);
      if (resp.string_body == endptr) {
        j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
      } else {
        j_result = json_pack("{sisI}", "result", WEBSERVICE_RESULT_OK, "value", i_value);
      }
  } else {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  free(path);
  ulfius_clean_request(&req);
  ulfius_clean_response(&resp);
  return j_result;
}

/**
 * Set the switch command
 */
json_t * b_device_set_switch (json_t * device, const char * switch_name, const int command, void * device_ptr) {
  struct _u_request req;
  int res;
  char * path;
  json_t * j_result = NULL;
  
  path = msprintf("%s/%s/%d", "SETSWITCH", switch_name, command);
  init_request_for_device(&req, device, path);
  
  res = ulfius_send_http_request(&req, NULL);
  if (res == U_OK) {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_OK);
  } else {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  free(path);
  ulfius_clean_request(&req);
  return j_result;
}

/**
 * Get the dimmer value
 */
json_t * b_device_get_dimmer (json_t * device, const char * dimmer_name, void * device_ptr) {
  struct _u_request req;
  struct _u_response resp;
  int res;
  char * path, * endptr, * value;
  json_t * j_result = NULL;
  json_int_t i_value;
  
  path = msprintf("%s/%s", "GETDIMMER", dimmer_name);
  init_request_for_device(&req, device, path);
  ulfius_init_response(&resp);
  
  res = ulfius_send_http_request(&req, &resp);
  if (res == U_OK) {
    value = resp.string_body + sizeof(char);
    value[strlen(value) - 1] = '\0';
      i_value = strtol(value, &endptr, 10);
      if (resp.string_body == endptr) {
        j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
      } else {
        j_result = json_pack("{sisI}", "result", WEBSERVICE_RESULT_OK, "value", i_value);
      }
  } else {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  free(path);
  ulfius_clean_request(&req);
  ulfius_clean_response(&resp);
  return j_result;
}

/**
 * Set the dimmer command
 */
json_t * b_device_set_dimmer (json_t * device, const char * dimmer_name, const int command, void * device_ptr) {
  struct _u_request req;
  int res;
  char * path;
  json_t * j_result = NULL;
  
  path = msprintf("%s/%s/%d", "SETDIMMER", dimmer_name, command);
  init_request_for_device(&req, device, path);
  
  res = ulfius_send_http_request(&req, NULL);
  if (res == U_OK) {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_OK);
  } else {
    j_result = json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
  }
  free(path);
  ulfius_clean_request(&req);
  return j_result;
}

/**
 * Get the heater value
 */
json_t * b_device_get_heater (json_t * device, const char * heater_name, void * device_ptr) {
  return json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
}

/**
 * Set the heater command
 */
json_t * b_device_set_heater (json_t * device, const char * heater_name, const char * mode, const float command, void * device_ptr) {
  return json_pack("{si}", "result", WEBSERVICE_RESULT_ERROR);
}

/**
 * Return true if an element with the specified name and the specified type exist in this device
 */
int b_device_has_element (json_t * device, int element_type, const char * element_name, void * device_ptr) {
  return u_map_has_key((struct _u_map *)device_ptr, element_name);
}
