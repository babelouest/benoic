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

#define SERVER_URL_PREFIX "http://localhost:2474/benoic"

#define AUTH_SERVER_URI "http://localhost:4593/glewlwyd"
#define USER_LOGIN "user1"
#define USER_PASSWORD "MyUser1Password!"
#define USER_SCOPE_LIST "angharad"

char * token = NULL;

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
  if (response != NULL) {
    printf("Status: %ld\n\n", response->status);
    printf("Body:\n%.*s\n\n", (int)response->binary_body_length, (char *)response->binary_body);
  }
}

int test_request_status(struct _u_request * req, long int expected_status, json_t * expected_contains) {
  int res, to_return = 0;
  struct _u_response response;
  
  ulfius_init_response(&response);
  res = ulfius_send_http_request(req, &response);
  if (res == U_OK) {
    json_t * json_body = ulfius_get_json_body_response(&response, NULL);
    if (response.status != expected_status) {
      printf("##########################\nError status (%s %s %ld)\n", req->http_verb, req->http_url, expected_status);
      print_response(&response);
      printf("##########################\n\n");
    } else if (expected_contains != NULL && (json_body == NULL || json_search(json_body, expected_contains) == NULL)) {
      char * dump_expected = json_dumps(expected_contains, JSON_ENCODE_ANY), * dump_response = json_dumps(json_body, JSON_ENCODE_ANY);
      printf("##########################\nError json (%s %s)\n", req->http_verb, req->http_url);
      printf("Expected result in response:\n%s\nWhile response is:\n%s\n", dump_expected, dump_response);
      printf("##########################\n\n");
      free(dump_expected);
      free(dump_response);
    } else {
      printf("Success (%s %s %ld)\n\n", req->http_verb, req->http_url, expected_status);
      to_return = 1;
    }
    json_decref(json_body);
  } else {
    printf("Error in http request: %d\n", res);
  }
  ulfius_clean_response(&response);
  return to_return;
}

void run_simple_test(const char * method, const char * url, json_t * request_body, int expected_status, json_t * expected_body) {
  struct _u_request request;
  ulfius_init_request(&request);
  request.http_verb = strdup(method);
  request.http_url = strdup(url);
  if (token != NULL) {
    u_map_put(request.map_header, "Authorization", token);
  }
  ulfius_set_json_body_request(&request, json_copy(request_body));
  
  test_request_status(&request, expected_status, expected_body);
  
  ulfius_clean_request(&request);
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
  
  run_simple_test("GET", SERVER_URL_PREFIX "/deviceTypes/", NULL, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/deviceTypes/reload", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/", NULL, 200, NULL);
  run_simple_test("POST", SERVER_URL_PREFIX "/device/", device_valid1, 200, NULL);
  run_simple_test("POST", SERVER_URL_PREFIX "/device/", device_invalid1, 400, NULL);
  run_simple_test("POST", SERVER_URL_PREFIX "/device/", device_invalid2, 400, NULL);
  run_simple_test("POST", SERVER_URL_PREFIX "/device/", device_invalid3, 400, NULL);
  run_simple_test("POST", SERVER_URL_PREFIX "/device/", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/", NULL, 200, device_valid1);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1", NULL, 200, device_valid1);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1", device_valid2, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev2", device_valid2, 404, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1", device_invalid1, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1", NULL, 200, device_valid2);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/connect", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/ping", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/disconnect", NULL, 200, NULL);
  run_simple_test("DELETE", SERVER_URL_PREFIX "/device/dev2", NULL, 404, NULL);
  run_simple_test("DELETE", SERVER_URL_PREFIX "/device/dev1", NULL, 200, NULL);
  
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

  run_simple_test("POST", SERVER_URL_PREFIX "/device/", device_valid, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/sensor/se1", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/connect", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/overview", NULL, 200, NULL);
  
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/sensor/se1", NULL, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", sensor_valid, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", sensor_invalid1, 400, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", sensor_invalid2, 400, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/sensor/se1", NULL, 200, sensor_valid);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/sensor/se3", NULL, 404, NULL);
  
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1", NULL, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/switch/sw1", switch_valid, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1", NULL, 200, switch_valid);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1/1", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/switch/sw1/error", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/switch/sw3/1", NULL, 404, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/switch/sw3", NULL, 404, NULL);
  
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", NULL, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", dimmer_valid, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", NULL, 200, dimmer_valid);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1/12", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di1/error", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di3/1", NULL, 404, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/dimmer/di3", NULL, 404, NULL);
  
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he1", NULL, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/heater/he1", heater_valid, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he1", NULL, 200, heater_valid);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/20?mode=manual", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/21", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/error", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he1/20?mode=error", NULL, 400, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he3", NULL, 404, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/heater/he3/20?mode=manual", NULL, 404, NULL);
  
  run_simple_test("DELETE", SERVER_URL_PREFIX "/device/dev1", NULL, 200, NULL);
  
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

  run_simple_test("POST", SERVER_URL_PREFIX "/device/", device_valid, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/device/dev1/connect", NULL, 200, NULL);
  
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/sensor/se1", sensor_valid, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/switch/sw1", switch_valid, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/dimmer/di1", dimmer_valid, 200, NULL);
  run_simple_test("PUT", SERVER_URL_PREFIX "/device/dev1/heater/he1", heater_valid, 200, NULL);
  
  printf("Press <enter> to run monitor get tests\n");
  getchar();

  run_simple_test("GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/se1/", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/sw1/", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/di1/", NULL, 200, NULL);
  run_simple_test("GET", SERVER_URL_PREFIX "/monitor/dev1/sensor/he1/", NULL, 200, NULL);
  
  run_simple_test("DELETE", SERVER_URL_PREFIX "/device/dev1", NULL, 200, NULL);
  
  json_decref(device_valid);
  json_decref(sensor_valid);
  json_decref(switch_valid);
  json_decref(dimmer_valid);
  json_decref(heater_valid);
}

int main(int argc, char ** argv) {
  struct _u_request auth_req;
  struct _u_response auth_resp;
  int res;

  ulfius_init_request(&auth_req);
  ulfius_init_response(&auth_resp);
  auth_req.http_verb = strdup("POST");
  auth_req.http_url = msprintf("%s/token/", argc>4?argv[4]:AUTH_SERVER_URI);
  u_map_put(auth_req.map_post_body, "grant_type", "password");
  u_map_put(auth_req.map_post_body, "username", argc>1?argv[1]:USER_LOGIN);
  u_map_put(auth_req.map_post_body, "password", argc>2?argv[2]:USER_PASSWORD);
  u_map_put(auth_req.map_post_body, "scope", argc>3?argv[3]:USER_SCOPE_LIST);
  res = ulfius_send_http_request(&auth_req, &auth_resp);
  if (res == U_OK && auth_resp.status == 200) {
    json_t * json_body = ulfius_get_json_body_response(&auth_resp, NULL);
    token = msprintf("Bearer %s", (json_string_value(json_object_get(json_body, "access_token"))));
    printf("User %s authenticated\n", USER_LOGIN);
    json_decref(json_body);
  } else {
    printf("Error authentication user %s\n", USER_LOGIN);
  }
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
