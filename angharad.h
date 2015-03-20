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
 * declarations for constants and prototypes
 *
 */

#ifndef __ANGHARAD_H__
#define __ANGHARAD_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <string.h>
#include <microhttpd.h>
#include <syslog.h>
#include <libconfig.h>
#include <sqlite3.h>
#include <time.h>
#include <math.h>
#include <stddef.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "arduino-serial-lib.h"

#define MSGLENGTH   2047
#define WORDLENGTH  63

#define TEMPEXT "TEMPEXT"
#define TEMPINT "TEMPINT"
#define HUMINT  "HUMINT"

#define TYPE_NONE   0
#define TYPE_SERIAL 1
#define TYPE_ZWAVE  2
#define TYPE_NET    3

#define ACTION_SET_SWITCH     0
#define ACTION_TOGGLE_SWITCH  1
#define ACTION_DIMMER         2
#define ACTION_HEATER         3
#define ACTION_SCRIPT         77
#define ACTION_SLEEP          88
#define ACTION_SYSTEM         99
#define ACTION_NONE           999

#define REPEAT_NONE         -1
#define REPEAT_MINUTE       0
#define REPEAT_HOUR         1
#define REPEAT_DAY          2
#define REPEAT_DAY_OF_WEEK  3
#define REPEAT_MONTH        4
#define REPEAT_YEAR         5

#define DATA_DEVICE   0
#define DATA_SWITCH   1
#define DATA_SENSOR   2
#define DATA_HEATER   4
#define DATA_DIMMER   5
#define DATA_ACTION   6
#define DATA_SCRIPT   7
#define DATA_SCHEDULE 8

#define TIMEOUT             3000
#define POSTBUFFERSIZE      512
#define HTTPGET             0
#define HTTPPOST            1

#define ERROR_SENSOR  -999.
#define ERROR_SWITCH  -1
#define ERROR_DIMMER  -1
#define ERROR_HEATER  0

#define UNDEFINED_HOME_ID 0

#define ANGHARAD_RUNNING  0
#define ANGHARAD_STOP     1
#define ANGHARAD_ERROR    2

/**
 * Structures used to represent elements
 */
typedef struct _device {
  int             enabled;                    // Device enabled or not
  char            name[WORDLENGTH+1];         // Name of the device on the commands
  char            display[WORDLENGTH+1];      // Display name of the device
  int             type;                       // Device type (Serial, USB, network, etc.)
  void *          element;                    // Device pointer
  char            tags[MSGLENGTH+1];
  char            uri[WORDLENGTH+1];          // URI of the device ('/dev/ttyACM0', 'xxx:xxxx', '192.168.1.1:888', etc.)
  
  pthread_mutex_t lock;                       // Mutex lock to avoid simultaneous access of the device
} device;

typedef struct _arduino_device {
  char            serial_file[WORDLENGTH+1];  // filename of the device
  int             serial_fd;                  // file descriptor of the device if serial type
  int             serial_baud;                // baud speed of the device if serial type
} arduino_device;

typedef struct _zwave_device {
  unsigned int    home_id;
  int             init_failed;
  void *          nodes_list;
  char            usb_file[WORDLENGTH+1];     // filename of the usb dongle
  char            config_path[WORDLENGTH+1];
  char            user_path[WORDLENGTH+1];
  char            command_line[WORDLENGTH+1];
  char            log_path[WORDLENGTH+1];
} zwave_device;

typedef struct _switcher {
  unsigned int id;
  char device[WORDLENGTH+1];
  char name[WORDLENGTH+1];
  char display[WORDLENGTH+1];
  int type;
  int enabled;
  int status;
  int monitored;
  int monitored_every;
  time_t monitored_next;
  char tags[MSGLENGTH+1];
} switcher;

typedef struct _sensor {
  unsigned int id;
  char device[WORDLENGTH+1];
  char name[WORDLENGTH+1];
  char display[WORDLENGTH+1];
  char unit[WORDLENGTH+1];
  char value[WORDLENGTH+1];
  int enabled;
  int monitored;
  int monitored_every;
  time_t monitored_next;
  char tags[MSGLENGTH+1];
} sensor;

typedef struct _heater {
  unsigned int id;
  char name[WORDLENGTH+1];
  char display[WORDLENGTH+1];
  char device[WORDLENGTH+1];
  unsigned int enabled;
  unsigned int set;
  unsigned int on;
  float heat_max_value;
  char unit[WORDLENGTH+1];
  int monitored;
  int monitored_every;
  time_t monitored_next;
  char tags[MSGLENGTH+1];
} heater;

typedef struct _dimmer {
  unsigned int id;
  char name[WORDLENGTH+1];
  char display[WORDLENGTH+1];
  char device[WORDLENGTH+1];
  unsigned int enabled;
  unsigned int value;
  int monitored;
  int monitored_every;
  time_t monitored_next;
  char tags[MSGLENGTH+1];
} dimmer;

typedef struct _action {
  unsigned int id;
  char name[WORDLENGTH+1];
  unsigned int type;                  // See ACTION_* constants
  char device[WORDLENGTH+1];          // Device name it applies to (if applicable)
  char switcher[WORDLENGTH+1];        // Switch number it applies to (if applicable)
  char heater[WORDLENGTH+1];          // Heater name it applies to (if applicable)
  char dimmer[WORDLENGTH+1];          // Dimmer name it applies to (if applicable)
  char params[MSGLENGTH+1];           // Parameters to add to the action
  char tags[MSGLENGTH+1];
} action;

typedef struct _script {
  unsigned int id;
  char name[WORDLENGTH+1];
  char device[WORDLENGTH+1];         // Optional
  int enabled;
  char actions[MSGLENGTH+1];         // use the format action_id,condition_result,condition_value[[;action_id,condition_result,condition_value]*]
  char tags[MSGLENGTH+1];
} script;

typedef struct _schedule {
  unsigned int id;
  char name[WORDLENGTH+1];
  unsigned int enabled;
  time_t next_time;
  int repeat_schedule;
  unsigned int repeat_schedule_value;
  unsigned int remove_after_done;
  char device[WORDLENGTH+1];
  int script;
  char tags[MSGLENGTH+1];
} schedule;

typedef struct _monitor {
  unsigned int id;
  time_t date;
  char device[WORDLENGTH+1];
  char switcher[WORDLENGTH+1];
  char sensor[WORDLENGTH+1];
  char value[WORDLENGTH+1];
  char tags[MSGLENGTH+1];
} monitor;

/**
 * Structures used to facilitate data manipulations
 */
struct connection_info_struct {
  int connectiontype;
  unsigned int data_type;
  void * data;
  struct MHD_PostProcessor *postprocessor;
};

struct config_elements {
  int auto_restart;
  int tcp_port;
  char url_prefix[WORDLENGTH+1];
  struct MHD_Daemon *daemon;
  device ** terminal;
  unsigned int nb_terminal;
  sqlite3 * sqlite3_db;
  char db_archive_path[MSGLENGTH+1];
  char script_path[MSGLENGTH+1];
};

// angharad.c
int server(struct config_elements * config);
int build_config(char * config_file, struct config_elements * config);
void exit_handler(int);
void exit_server(struct config_elements ** config, int exit_value);
int global_handler_variable;


// General hardware interface
// control-meta.c
device * get_device_from_name(char * device_name, device ** terminal, unsigned int nb_terminal);
int send_heartbeat(device * terminal);
int is_connected(device * terminal);
int connect_device(device * terminal, device ** terminals, unsigned int nb_terminal);
int reconnect_device(device * terminal, device ** terminals, unsigned int nb_terminal);
int close_device(device * terminal);
int set_switch_state(device * terminal, char * switcher, int status);
int get_switch_state(device * terminal, char * switcher, int force);
int toggle_switch_state(device * terminal, char * switcher);
float get_sensor_value(device * terminal, char * sensor, int force);
char * get_overview(sqlite3 * sqlite3_db, device * terminal);
char * get_refresh(sqlite3 * sqlite3_db, device * terminal);
char * build_overview_output(sqlite3 * sqlite3_db, char * device_name, switcher * switchers, int nb_switchers, sensor * sensors, int nb_sensors, heater * heaters, int nb_heaters, dimmer * dimmers, int nb_dimmers);
int get_name(device * terminal, char * output);
char * get_devices(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal);
int get_heater(device * terminal, char * heat_id, char * buffer);
int set_heater(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * buffer);
int parse_heater(sqlite3 * sqlite3_db, char * device, char * heater_name, char * source, heater * cur_heater);
int get_dimmer_value(device * terminal, char * dimmer);
int set_dimmer_value(device * terminal, char * dimmer, int value);

// Interface with the Arduinos
// control-arduino.c
int is_connected_arduino(device * terminal);
int connect_device_arduino(device * terminal, device ** terminals, unsigned int nb_terminal);
int reconnect_device_arduino(device * terminal, device ** terminals, unsigned int nb_terminal);
int close_device_arduino(device * terminal);
int send_heartbeat_arduino(device * terminal);
int set_switch_state_arduino(device * terminal, char * switcher, int status);
int get_switch_state_arduino(device * terminal, char * switcher, int force);
int toggle_switch_state_arduino(device * terminal, char * switcher);
float get_sensor_value_arduino(device * terminal, char * sensor, int force);
char * get_overview_arduino(sqlite3 * sqlite3_db, device * terminal);
char * get_refresh_arduino(sqlite3 * sqlite3_db, device * terminal);
char * parse_overview_arduino(sqlite3 * sqlite3_db, char * overview_result);
int get_name_arduino(device * terminal, char * output);
int get_heater_arduino(device * terminal, char * heat_id, char * buffer);
int set_heater_arduino(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * buffer);
int is_file_opened_arduino(char * serial_file, device ** terminal, unsigned int nb_terminal);
int get_dimmer_value_arduino(device * terminal, char * dimmer);
int set_dimmer_value_arduino(device * terminal, char * dimmer, int value);

// Interface with the zwave network
// control-zwave.c
int is_connected_zwave(device * terminal);
int connect_device_zwave(device * terminal, device ** terminals, unsigned int nb_terminal);
int reconnect_device_zwave(device * terminal, device ** terminals, unsigned int nb_terminal);
int close_device_zwave(device * terminal);
int send_heartbeat_zwave(device * terminal);
int set_switch_state_zwave(device * terminal, char * switcher, int status);
int get_switch_state_zwave(device * terminal, char * switcher, int force);
int toggle_switch_state_zwave(device * terminal, char * switcher);
float get_sensor_value_zwave(device * terminal, char * sensor, int force);
char * get_overview_zwave(sqlite3 * sqlite3_db, device * terminal);
char * get_refresh_zwave(sqlite3 * sqlite3_db, device * terminal);
int get_name_zwave(device * terminal, char * output);
int get_heater_zwave(device * terminal, char * heat_id, char * buffer);
int set_heater_zwave(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * buffer);
int get_dimmer_value_zwave(device * terminal, char * dimmer);
int set_dimmer_value_zwave(device * terminal, char * dimmer, int value);

// System functions
// misc.c
void log_message(int type, const char * message, ...);
int str_replace(char * source, char * target, size_t len, char * needle, char * haystack);
int sanitize_json_string(char * source, char * target, size_t len);
int journal(sqlite3 * sqlite3_db, const char * origin, const char * command, const char * result);
int num_digits(int n);
int num_digits_l (long n);

// Actions and lists
char * get_scripts(sqlite3 * sqlite3_db, char * device);
char * get_script(sqlite3 * sqlite3_db, char * script_id, int with_tags);
char * get_action_script(sqlite3 * sqlite3_db, int script_id);
int run_script(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, char * script_path, char * script_id);
char * get_actions(sqlite3 * sqlite3_db, char * device);
int run_action(action ac, device ** terminal, unsigned int nb_terminal, sqlite3 * sqlite3_db, char * script_path);
char * get_schedules(sqlite3 * sqlite3_db, char * device);

// Scheduler
void * thread_scheduler_run(void * args);
int run_scheduler(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, char * script_path);
int is_scheduled_now(time_t next_time);
int update_schedule(sqlite3 * sqlite3_db, schedule * sc);
int update_schedule_db(sqlite3 * sqlite3_db, schedule sc);
int remove_schedule_db(sqlite3 * sqlite3_db, schedule sc);
time_t calculate_next_time(time_t from, int schedule_type, unsigned int schedule_value);
int enable_schedule(sqlite3 * sqlite3_db, char * schedule, char * status, char * command_result);

// Monitor
int monitor_switch(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, switcher p);
int monitor_sensor(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, sensor s);
int monitor_store(sqlite3 * sqlite3_db, const char * device_name, const char * switch_name, const char * sensor_name, const char * value);
char * get_monitor(sqlite3 * sqlite3_db, const char * device, const char * switcher, const char * sensor, const char * start_date);

// Archive
int archive_journal(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from);
int archive_monitor(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from);
int archive(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from);
unsigned int get_last_archive(char * db_archive_path);
int is_archive_running(char * db_archive_path);
void * thread_archive_run(void * args);
struct archive_args {
  sqlite3 * sqlite3_db;
  char db_archive_path[MSGLENGTH+1];
  unsigned int epoch_from;
};

// add/modify/remove elements
char * set_device_data(sqlite3 * sqlite3_db, device cur_device);
char * set_switch_data(sqlite3 * sqlite3_db, switcher cur_switch);
char * set_sensor_data(sqlite3 * sqlite3_db, sensor cur_sensor);
char * set_heater_data(sqlite3 * sqlite3_db, heater cur_heater);
char * set_dimmer_data(sqlite3 * sqlite3_db, dimmer cur_dimmer);

int add_action(sqlite3 * sqlite3_db, action cur_action, char * command_result);
int set_action(sqlite3 * sqlite3_db, action cur_action, char * command_result);
int delete_action(sqlite3 * sqlite3_db, char * action_id);
int add_script(sqlite3 * sqlite3_db, script cur_script, char * command_result);
int set_script(sqlite3 * sqlite3_db, script cur_script, char * command_result);
int delete_script(sqlite3 * sqlite3_db, char * script_id);
int add_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result);
int set_schedule(sqlite3 * sqlite3_db, schedule cur_schedule, char * command_result);
int delete_schedule(sqlite3 * sqlite3_db, char * schedule_id);

// Startup status functions
int init_device_status(sqlite3 * sqlite3_db, device * cur_device);
int save_startup_heater_status(sqlite3 * sqlite3_db, char * device, char * heater_name, int heat_enabled, float max_heat_value);
heater * get_startup_heater_status(sqlite3 * sqlite3_db, char * device);
int save_startup_switch_status(sqlite3 * sqlite3_db, char * device, char * switcher, int status);
int set_startup_all_switch(sqlite3 * sqlite3_db, device * cur_device);
int save_startup_dimmer_value(sqlite3 * sqlite3_db, char * device, char * dimmer, int value);
int set_startup_all_dimmer_value(sqlite3 * sqlite3_db, device * cur_device);

// libmicrohttpd functions
int iterate_post_data (void *coninfo_cls, enum MHD_ValueKind kind,
                         const char *key, const char *filename,
                         const char *content_type,
                         const char *transfer_encoding,
                         const char *data,
                         uint64_t off,
                         size_t size);
int angharad_rest_webservice (void *cls, struct MHD_Connection *connection,
                                     const char *url, const char *method,
                                     const char *version, const char *upload_data,
                                     size_t *upload_data_size, void **con_cls);
void request_completed (void *cls, struct MHD_Connection *connection,
                               void **con_cls, enum MHD_RequestTerminationCode toe);

// tags functions
char ** get_tags(sqlite3 * sqlite3_db, char * device_name, unsigned int element_type, char * element);
char * build_json_tags(char ** tags);
char ** build_tags_from_list(char * tags);
int set_tags(sqlite3 * sqlite3_db, char * device_name, unsigned int element_type, char * element, char ** tags);
int get_or_create_tag_id(sqlite3 * sqlite3_db, char * tag);
int free_tags(char ** tags);

#endif //__ANGHARAD_H__
