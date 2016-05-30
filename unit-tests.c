/**
 *
 * Benoic House Automation service
 *
 * Command house automation devices via an HTTP REST interface
 *
 * Unit tests
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

#include <string.h>
#include <jansson.h>
#include <ulfius.h>
#include <orcania.h>

//#define SERVER_URL_PREFIX "http://localhost:2642/benoic"
#define SERVER_URL_PREFIX "http://localhost:2473/benoic"

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
 * Developper-friendly response print
 */
void print_response(struct _u_response * response) {
  char * dump_json = NULL;
  if (response != NULL) {
    printf("Status: %ld\n\n", response->status);
    if (response->json_body != NULL) {
      dump_json = json_dumps(response->json_body, JSON_INDENT(2));
      printf("Json body:\n%s\n\n", dump_json);
      free(dump_json);
    } else if (response->string_body != NULL) {
      printf("String body: %s\n\n", response->string_body);
    }
  }
}


int test_request_status(struct _u_request * req, long int expected_status, json_t * expected_contains) {
  int res, to_return = 0;
  struct _u_response response;
  
  ulfius_init_response(&response);
  res = ulfius_send_http_request(req, &response);
  if (res == U_OK) {
    if (response.status != expected_status) {
      printf("##########################\nError status (%s %s %ld)\n", req->http_verb, req->http_url, expected_status);
      print_response(&response);
      printf("##########################\n\n");
    } else if (expected_contains != NULL && (response.json_body == NULL || json_search(response.json_body, expected_contains) == NULL)) {
      char * dump_expected = json_dumps(expected_contains, JSON_ENCODE_ANY), * dump_response = json_dumps(response.json_body, JSON_ENCODE_ANY);
      printf("##########################\nError json (%s %s)\n", req->http_verb, req->http_url);
      printf("Expected result in response:\n%s\nWhile response is:\n%s\n", dump_expected, dump_response);
      printf("##########################\n\n");
      free(dump_expected);
      free(dump_response);
    } else {
      printf("Success (%s %s %ld)\n\n", req->http_verb, req->http_url, expected_status);
      to_return = 1;
    }
  } else {
    printf("Error in http request: %d\n", res);
  }
  ulfius_clean_response(&response);
  return to_return;
}

void run_device_tests() {
  json_t * device_valid1 = json_loads("{\
    \"name\":\"dev1\",\
    \"description\":\"first test\",\
    \"type_uid\":\"00-00-00\",\
    \"enabled\":true,\
    \"options\":{\
      \"uri\":\"dev1\",\
      \"baud\":6900,\
      \"do_not_check_certificate\":false,\
      \"device_specified\":\"TBD\"\
    }\
  }", JSON_DECODE_ANY, NULL);
  json_t * device_valid2 = json_loads("{\
    \"description\":\"second test\",\
    \"type_uid\":\"00-00-00\",\
    \"enabled\":true,\
    \"options\":{\
      \"uri\":\"dev1\",\
      \"baud\":6900,\
      \"do_not_check_certificate\":false,\
      \"device_specified\":\"TBD\"\
    }\
  }", JSON_DECODE_ANY, NULL);
  json_t * device_invalid1 = json_loads("{\
    \"name\":\"dev2\",\
    \"description\":\"first test\",\
    \"type_uid\":\"00-00-01\",\
    \"options\":{\
      \"uri\":\"dev1\",\
      \"baud\":6900,\
      \"do_not_check_certificate\":false\
    }\
  }", JSON_DECODE_ANY, NULL);
  json_t * device_invalid2 = json_loads("{\
    \"name\":\"dev2\",\
    \"description\":\"first test\",\
    \"type_uid\":\"00-00-00\",\
    \"options\":{\
      \"baud\":6900,\
      \"do_not_check_certificate\":false\
    }\
  }", JSON_DECODE_ANY, NULL);
  json_t * device_invalid3 = json_loads("{\
    \"name\":\"dev2\",\
    \"description\":\"first test\",\
    \"type_uid\":\"00-00-00\"\
    \"options\":{\
      \"baud\":6900,\
      \"do_not_check_certificate\":false\
    }\
  }", JSON_DECODE_ANY, NULL);
  
  struct _u_request req_list[] = {
    {"GET", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_valid1, NULL, 0, NULL, 0}, // 200
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_invalid1, NULL, 0, NULL, 0}, // 400
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_invalid2, NULL, 0, NULL, 0}, // 400
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_invalid3, NULL, 0, NULL, 0}, // 400
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_valid2, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev2", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_valid2, NULL, 0, NULL, 0}, // 404
    {"PUT", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_invalid1, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/connect", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/ping", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/disconnect", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"DELETE", SERVER_URL_PREFIX "/device/dev2", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    {"DELETE", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
  };

  test_request_status(&req_list[0], 200, NULL);
  test_request_status(&req_list[1], 200, NULL);
  test_request_status(&req_list[2], 400, NULL);
  test_request_status(&req_list[3], 400, NULL);
  test_request_status(&req_list[4], 400, NULL);
  test_request_status(&req_list[5], 400, NULL);
  test_request_status(&req_list[6], 200, device_valid1);
  test_request_status(&req_list[7], 200, device_valid1);
  test_request_status(&req_list[8], 200, NULL);
  test_request_status(&req_list[9], 404, NULL);
  test_request_status(&req_list[10], 400, NULL);
  test_request_status(&req_list[11], 200, device_valid2);
  test_request_status(&req_list[12], 200, NULL);
  test_request_status(&req_list[13], 200, NULL);
  test_request_status(&req_list[14], 200, NULL);
  test_request_status(&req_list[15], 404, NULL);
  test_request_status(&req_list[16], 200, NULL);
  
  json_decref(device_valid1);
  json_decref(device_valid2);
  json_decref(device_invalid1);
  json_decref(device_invalid2);
}

void run_device_element_tests() {
  json_t * device_valid = json_loads("{\
    \"name\":\"dev1\",\
    \"description\":\"first test\",\
    \"type_uid\":\"00-00-00\",\
    \"enabled\":true,\
    \"options\":{\
      \"uri\":\"dev1\",\
      \"baud\":6900,\
      \"do_not_check_certificate\":false,\
      \"device_specified\":\"TBD\"\
    }\
  }", JSON_DECODE_ANY, NULL);
  json_t * sensor_valid = json_loads("\
{\
  \"display\": \"Sensor one\",\
  \"description\": \"First sensor\",\
  \"enabled\": true,\
  \"monitored_every\": 0,\
  \"options\":\
  {\
    \"unit\": \"°C\"\
  },\
  \"monitored\": false\
}", JSON_DECODE_ANY, NULL);
  json_t * switch_valid = json_loads("\
{\
  \"value\": 0,\
  \"display\": \"Switch one\",\
  \"enabled\": true,\
  \"monitored_every\": 0,\
  \"description\": \"First switch\",\
  \"options\":\
  {\
  },\
  \"monitored\": false\
}", JSON_DECODE_ANY, NULL);
  json_t * dimmer_valid = json_loads("\
{\
  \"value\": 42,\
  \"display\": \"Dimmer one\",\
  \"description\": \"First dimmer\",\
  \"enabled\": true,\
  \"options\":\
  {\
  },\
  \"monitored_every\": 0,\
  \"monitored\": false\
}", JSON_DECODE_ANY, NULL);
  json_t * heater_valid = json_loads("\
{\
  \"display\": \"Heater one\",\
  \"enabled\": true,\
  \"monitored_every\": 0,\
  \"description\": \"First heater\",\
  \"command\": 18.0,\
  \"monitored\": false,\
  \"options\":\
  {\
    \"unit\": \"°C\"\
  },\
  \"mode\": \"auto\"\
}", JSON_DECODE_ANY, NULL);
  json_t * sensor_invalid1 = json_loads("\
{\
  \"display\": \"Sensor one\",\
  \"description\": \"First sensor\",\
  \"enabled\": \"error\",\
  \"monitored_every\": 0,\
  \"options\":\
  {\
    \"unit\": \"°C\"\
  },\
  \"monitored\": false\
}", JSON_DECODE_ANY, NULL);
  json_t * sensor_invalid2 = json_loads("\
{\
  \"display\": \"Sensor one\",\
  \"description\": \"First sensor\",\
  \"enabled\": true,\
  \"monitored_every\": 0\
  \"options\":\
  {\
    \"unit\": \"°C\"\
  },\
  \"monitored\": false\
}", JSON_DECODE_ANY, NULL);

  struct _u_request req_list[] = {
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_valid, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/connect", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/overview", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    
    {"GET", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200/400
    {"PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, sensor_valid, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, sensor_invalid1, NULL, 0, NULL, 0}, // 400
    {"PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, sensor_invalid2, NULL, 0, NULL, 0}, // 400
    {"PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/sensor/se3", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    
    {"GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/switch/sw1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, switch_valid, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1/1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1/error", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/dev1/switch/sw3/1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    {"GET", SERVER_URL_PREFIX "/device/dev1/switch/sw3", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    
    {"GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, dimmer_valid, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1/12", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1/error", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di3/1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    {"GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di3", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/heater/he1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, heater_valid, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/20?mode=manual", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/21", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/error", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/20?mode=error", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 400
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he3", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    {"GET", SERVER_URL_PREFIX "/device/dev1/heater/he3/20?mode=manual", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 404
    
    {"DELETE", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
  };

  test_request_status(&req_list[0], 200, NULL);
  test_request_status(&req_list[3], 400, NULL);
  test_request_status(&req_list[1], 200, NULL);
  test_request_status(&req_list[2], 200, NULL);
  
  test_request_status(&req_list[3], 200, NULL);
  test_request_status(&req_list[4], 200, NULL);
  test_request_status(&req_list[5], 400, NULL);
  test_request_status(&req_list[6], 400, NULL);
  test_request_status(&req_list[7], 400, NULL);
  test_request_status(&req_list[8], 200, sensor_valid);
  test_request_status(&req_list[9], 404, NULL);

  test_request_status(&req_list[10], 200, NULL);
  test_request_status(&req_list[11], 200, NULL);
  test_request_status(&req_list[12], 200, switch_valid);
  test_request_status(&req_list[13], 200, NULL);
  test_request_status(&req_list[14], 400, NULL);
  test_request_status(&req_list[15], 404, NULL);
  test_request_status(&req_list[16], 404, NULL);
  
  test_request_status(&req_list[17], 200, NULL);
  test_request_status(&req_list[18], 200, NULL);
  test_request_status(&req_list[19], 200, dimmer_valid);
  test_request_status(&req_list[20], 200, NULL);
  test_request_status(&req_list[21], 400, NULL);
  test_request_status(&req_list[22], 404, NULL);
  test_request_status(&req_list[23], 404, NULL);
  
  test_request_status(&req_list[24], 200, NULL);
  test_request_status(&req_list[25], 200, NULL);
  test_request_status(&req_list[26], 200, heater_valid);
  test_request_status(&req_list[27], 200, NULL);
  test_request_status(&req_list[28], 200, NULL);
  test_request_status(&req_list[29], 400, NULL);
  test_request_status(&req_list[30], 400, NULL);
  test_request_status(&req_list[31], 404, NULL);
  test_request_status(&req_list[32], 404, NULL);
  test_request_status(&req_list[33], 200, NULL);
  
  json_decref(device_valid);
}

void run_monitor_tests() {
  json_t * device_valid = json_loads("{\
    \"name\":\"dev1\",\
    \"description\":\"first test\",\
    \"type_uid\":\"00-00-00\",\
    \"enabled\":true,\
    \"options\":{\
      \"uri\":\"dev1\",\
      \"baud\":6900,\
      \"do_not_check_certificate\":false,\
      \"device_specified\":\"TBD\"\
    }\
  }", JSON_DECODE_ANY, NULL);
  json_t * sensor_valid = json_loads("\
{\
  \"display\": \"Sensor one\",\
  \"description\": \"First sensor\",\
  \"enabled\": true,\
  \"monitored_every\": 120,\
  \"options\":\
  {\
    \"unit\": \"°C\"\
  },\
  \"monitored\": true\
}", JSON_DECODE_ANY, NULL);
  json_t * switch_valid = json_loads("\
{\
  \"value\": 0,\
  \"display\": \"Switch one\",\
  \"enabled\": true,\
  \"monitored_every\": 120,\
  \"description\": \"First switch\",\
  \"options\":\
  {\
  },\
  \"monitored\": true\
}", JSON_DECODE_ANY, NULL);
  json_t * dimmer_valid = json_loads("\
{\
  \"value\": 42,\
  \"display\": \"Dimmer one\",\
  \"description\": \"First dimmer\",\
  \"enabled\": true,\
  \"options\":\
  {\
  },\
  \"monitored_every\": 120,\
  \"monitored\": true\
}", JSON_DECODE_ANY, NULL);
  json_t * heater_valid = json_loads("\
{\
  \"display\": \"Heater one\",\
  \"enabled\": true,\
  \"monitored_every\": 120,\
  \"description\": \"First heater\",\
  \"command\": 18.0,\
  \"monitored\": true,\
  \"options\":\
  {\
    \"unit\": \"°C\"\
  },\
  \"mode\": \"auto\"\
}", JSON_DECODE_ANY, NULL);

  struct _u_request req_list[] = {
    {"POST", SERVER_URL_PREFIX "/device/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, device_valid, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/device/dev1/connect", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    
    {"PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, sensor_valid, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/switch/sw1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, switch_valid, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, dimmer_valid, NULL, 0, NULL, 0}, // 200
    {"PUT", SERVER_URL_PREFIX "/device/dev1/heater/he1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, heater_valid, NULL, 0, NULL, 0}, // 200
    
    {"GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/se1/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/sw1/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/di1/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    {"GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/he1/", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
    
    {"DELETE", SERVER_URL_PREFIX "/device/dev1", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0}, // 200
  };

  test_request_status(&req_list[0], 200, NULL);
  test_request_status(&req_list[1], 200, NULL);
  
  test_request_status(&req_list[2], 200, NULL);
  test_request_status(&req_list[3], 200, NULL);
  test_request_status(&req_list[4], 200, NULL);
  test_request_status(&req_list[5], 200, NULL);
  
  printf("Press <enter> to run monitor get tests\n");
  getchar();

  test_request_status(&req_list[6], 200, NULL);
  test_request_status(&req_list[7], 200, NULL);
  test_request_status(&req_list[8], 200, NULL);
  test_request_status(&req_list[9], 200, NULL);
  
  test_request_status(&req_list[10], 200, NULL);
  json_decref(device_valid);
}

int main(void) {
  printf("Press <enter> to run device tests\n");
  getchar();
  run_device_tests();
  printf("Press <enter> to run device elements tests\n");
  getchar();
  run_device_element_tests();
  printf("Press <enter> to run monitor tests\n");
  getchar();
  run_monitor_tests();
  return 0;
}
