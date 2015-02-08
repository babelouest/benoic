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

#define MARCO 0
#define TEMP  1
#define GET   2
#define SET   3

#define TEMPEXT "TEMPEXT"
#define TEMPINT "TEMPINT"
#define HUMINT "HUMINT"

#define MSGLENGTH 2047
#define WORDLENGTH 64

#define TYPE_NONE   0
#define TYPE_SERIAL 1
#define TYPE_USB    2
#define TYPE_NET    3

#define ACTION_DEVICE   0
#define ACTION_OVERVIEW 1
#define ACTION_REFRESH  2
#define ACTION_GET      3
#define ACTION_SET      4
#define ACTION_SENSOR   5
#define ACTION_HEATER   6
#define ACTION_TOGGLE   7
#define ACTION_SCRIPT   77
#define ACTION_SLEEP    88
#define ACTION_SYSTEM   99
#define ACTION_NONE     999

#define REPEAT_NONE         -1
#define REPEAT_MINUTE       0
#define REPEAT_HOUR         1
#define REPEAT_DAY          2
#define REPEAT_DAY_OF_WEEK  3
#define REPEAT_MONTH        4
#define REPEAT_YEAR         5

#define DATA_DEVICE   0
#define DATA_PIN      1
#define DATA_SENSOR   2
#define DATA_LIGHT    3
#define DATA_HEATER   4
#define DATA_ACTION   5
#define DATA_SCRIPT   6
#define DATA_SCHEDULE 7

#define TIMEOUT             3000
#define POSTBUFFERSIZE      512
#define HTTPGET             0
#define HTTPPOST            1

#define ERROR_SENSOR -999.
#define ERROR_SWITCH -1

typedef struct _device {
  unsigned int id;
  int enabled;                    // Device enabled or not
  char name[WORDLENGTH+1];        // Name of the device on the commands
  char display[WORDLENGTH+1];     // Display name of the device
  int type;                       // Device type (Serial, USB, network, etc.)
  char uri[WORDLENGTH+1];         // URI of the device ('/dev/ttyACM0', 'xxx:xxxx', '192.168.1.1:888', etc.)
  pthread_mutex_t lock;           // Mutex lock to avoid simultaneous access of the device
  
  char serial_file[WORDLENGTH+1]; // filename of the device
  int serial_fd;                  // file descriptor of the device if serial type
  int serial_baud;                // baud speed of the device if serial type
} device;

typedef struct _pin {
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
} pin;

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
} sensor;

typedef struct _action {
  unsigned int id;
  char name[WORDLENGTH+1];
  unsigned int type;                  // See ACTION_* constants
  char device[WORDLENGTH+1];          // Device name it applies to (if applicable)
  char pin[WORDLENGTH+1];             // Pin number it applies to (if applicable)
  char sensor[WORDLENGTH+1];          // Sensor name it applies to (if applicable)
  char heater[WORDLENGTH+1];          // Heater name it applies to (if applicable)
  char params[MSGLENGTH+1];           // Parameters to add to the action
} action;

typedef struct _script {
  unsigned int id;
  char name[WORDLENGTH+1];
  char device[WORDLENGTH+1];          // Optional
  int enabled;
  char actions[MSGLENGTH+1];         // use the format action_id,condition_result,condition_value[[;action_id,condition_result,condition_value]*]
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
} schedule;

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
} heater;

typedef struct _light {
  unsigned int id;
  char name[WORDLENGTH+1];
  char display[WORDLENGTH+1];
  char device[WORDLENGTH+1];
  unsigned int enabled;
  unsigned int on;
} light;

typedef struct _monitor {
  unsigned int id;
  time_t date;
  char device[WORDLENGTH+1];
  char pin[WORDLENGTH+1];
  char sensor[WORDLENGTH+1];
  char value[WORDLENGTH+1];
} monitor;

struct connection_info_struct {
  int connectiontype;
  unsigned int data_type;
  void * data;
  struct MHD_PostProcessor *postprocessor;
};

struct config_elements {
  int tcp_port;
  char url_prefix[WORDLENGTH+1];
  device ** terminal;
  unsigned int nb_terminal;
  sqlite3 * sqlite3_db;
  char db_archive_path[MSGLENGTH+1];
  char script_path[MSGLENGTH+1];
};

// Init function
int server(char * config_file);
int initialize(char * config_file, char * message, struct config_elements * config);
device * get_device_from_name(char * device_name, device ** terminal, unsigned int nb_terminal);

// Interface with the Arduinos
int is_connected(device * terminal);
int connect_device(device * terminal, device ** terminals, unsigned int nb_terminal);
int reconnect_device(device * terminal, device ** terminals, unsigned int nb_terminal);
int close_device(device * terminal);
int send_heartbeat(device * terminal);
int set_switch_state(device * terminal, char * pin, int status);
int get_switch_state(device * terminal, char * pin, int force);
int toggle_switch_state(device * terminal, char * pin);
float get_sensor_value(device * terminal, char * sensor, int force);
int get_overview(device * terminal, char * output);
int get_refresh(device * terminal, char * output);
char * parse_overview(sqlite3 * sqlite3_db, char * overview_result);
int get_name(device * terminal, char * output);
char * get_devices(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal);
int get_heater(device * terminal, char * heat_id, char * output);
int set_heater(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * output);
int parse_heater(sqlite3 * sqlite3_db, char * device, char * heater_name, char * source, heater * cur_heater);
int get_light(device * terminal, char * light);
int set_light(device * terminal, char * light, unsigned int status);
int is_file_opened(char * serial_file, device ** terminal, unsigned int nb_terminal);

// System functions
void log_message(int type, const char * message);
int str_replace(char * source, char * target, size_t len, char * needle, char * haystack);
int sanitize_json_string(char * source, char * target, size_t len);
int journal(sqlite3 * sqlite3_db, const char * origin, const char * command, const char * result);
int num_digits(int n);
int num_digits_l (long n);

// Actions and lists
char * get_scripts(sqlite3 * sqlite3_db, char * device);
int get_script(sqlite3 * sqlite3_db, char * script_id, char * overview);
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
int monitor_switch(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, pin p);
int monitor_sensor(sqlite3 * sqlite3_db, device ** terminal, unsigned int nb_terminal, sensor s);
int monitor_store(sqlite3 * sqlite3_db, const char * device_name, const char * switch_name, const char * sensor_name, const char * value);
char * get_monitor(sqlite3 * sqlite3_db, const char * device, const char * pin, const char * sensor, const char * start_date);

// Archive
int archive_journal(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from);
int archive_monitor(sqlite3 * sqlite3_db, sqlite3 * sqlite3_archive_db, unsigned int epoch_from);
int archive(sqlite3 * sqlite3_db, char * db_archive_path, unsigned int epoch_from);

// add/modify/remove elements
int set_device_data(sqlite3 * sqlite3_db, device cur_device, char * command_result);
int set_pin_data(sqlite3 * sqlite3_db, pin cur_pin, char * command_result);
int set_sensor_data(sqlite3 * sqlite3_db, sensor cur_sensor, char * command_result);
int set_light_data(sqlite3 * sqlite3_db, light cur_light, char * command_result);
int set_heater_data(sqlite3 * sqlite3_db, heater cur_heater, char * command_result);

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
int set_startup_heater_status(sqlite3 * sqlite3_db, char * device, char * heater_name, int heat_enabled, float max_heat_value);
heater * get_startup_heater_status(sqlite3 * sqlite3_db, char * device);
int set_startup_pin_status(sqlite3 * sqlite3_db, char * device, char * pin, int status);
int set_startup_pin_on(sqlite3 * sqlite3_db, device * cur_device);

// libmicrohttpd functions
int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind,
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


#endif //__ANGHARAD_H__
