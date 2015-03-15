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
 * ZWave devices calls
 *
 */

/**
 * List of Command classes id
 * (0x20) COMMAND_CLASS_BASIC
 * (0x21) COMMAND_CLASS_CONTROLLER_REPLICATION
 * (0x25) COMMAND_CLASS_SWITCH_BINARY
 * (0x26) COMMAND_CLASS_SWITCH_MULTILEVEL
 * (0x27) COMMAND_CLASS_SWITCH_ALL
 * (0x28) COMMAND_CLASS_SWITCH_TOGGLE_BINARY
 * (0x29) COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL
 * (0x30) COMMAND_CLASS_SENSOR_BINARY
 * (0x31) COMMAND_CLASS_SENSOR_MULTILEVEL
 * (0x32) COMMAND_CLASS_METER
 * (0x35) COMMAND_CLASS_METER_PULSE
 * (0x40) COMMAND_CLASS_THERMOSTAT_MODE
 * (0x42) COMMAND_CLASS_THERMOSTAT_OPERATING_STATE
 * (0x43) COMMAND_CLASS_THERMOSTAT_SETPOINT
 * (0x44) COMMAND_CLASS_THERMOSTAT_FAN_MODE
 * (0x45) COMMAND_CLASS_THERMOSTAT_FAN_STATE
 * (0x46) COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE
 * (0x4C) COMMAND_CLASS_DOOR_LOCK_LOGGING
 * (0x50) COMMAND_CLASS_BASIC_WINDOW_COVERING
 * (0x5B) COMMAND_CLASS_CENTRAL_SCENE
 * (0x62) COMMAND_CLASS_DOOR_LOCK
 * (0x63) COMMAND_CLASS_USER_CODE
 * (0x71) COMMAND_CLASS_ALARM
 * (0x73) COMMAND_CLASS_POWERLEVEL
 * (0x75) COMMAND_CLASS_PROTECTION
 * (0x76) COMMAND_CLASS_LOCK
 * (0x80) COMMAND_CLASS_BATTERY
 * (0x81) COMMAND_CLASS_CLOCK
 * (0x84) COMMAND_CLASS_WAKE_UP
 * (0x86) COMMAND_CLASS_VERSION
 * (0x87) COMMAND_CLASS_INDICATOR
 * (0x89) COMMAND_CLASS_LANGUAGE
 * (0x8B) COMMAND_CLASS_TIME_PARAMETERS
 * (0x90) COMMAND_CLASS_ENERGY_PRODUCTION
 * (0x9b) COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION
 */

#include "angharad.h"
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "ValueStore.h"
#include "Value.h"
#include "ValueBool.h"
#include "Log.h"

/**
 * COMMAND_CLASS used
 */
#define COMMAND_CLASS_SWITCH_BINARY 0x25
#define COMMAND_CLASS_SWITCH_MULTILEVEL 0x26

using namespace OpenZWave;
using namespace std;

/**
 * node structure
 * identity a zwave connected device
 */
typedef struct _node {
	uint32        home_id;
	uint8		      node_id;
	bool		      polled;
	list<ValueID>	values;
} node;

/**
 * return the node in this device identified by it id
 */
node * get_device_node(device * terminal, uint8 node_id) {
  
  if (terminal == NULL || terminal->element == NULL || ((struct _zwave_device *) terminal->element)->nodes_list == NULL) {
    return NULL;
  } else {
    list<node*> * nodes_list = (list<node*> *) ((struct _zwave_device *) terminal->element)->nodes_list;
    for( list<node*>::iterator it = nodes_list->begin(); it != nodes_list->end(); ++it ) {
      node* cur_node = *it;
      
      if (cur_node->home_id == ((struct _zwave_device *)terminal->element)->home_id && node_id == cur_node->node_id) {
        return cur_node;
      }
      
    }
  }
  return NULL;
}

/**
 * return the ValueID of the node identified by the command_class value
 */
ValueID * get_device_value_id(node * cur_node, uint8 command_class) {
  if (cur_node != NULL) {
    for( list<ValueID>::iterator it = cur_node->values.begin(); it != cur_node->values.end(); ++it ) {
      ValueID * v = &(*it);
      if ( v->GetCommandClassId() == command_class ) {
        return v;
      }
    }
  }
  return NULL;
}

/**
 * on_notification_zwave
 * Callback that is triggered when a value, group or node changes
*/
void on_notification_zwave ( Notification const * _notification, void * _context ) {
  
  device * terminal = (device *)_context;
  zwave_device * zwave_terminal = (struct _zwave_device *) terminal->element;
  list<node*> * nodes_list = (list<node*> *) zwave_terminal->nodes_list;
  
  pthread_mutex_lock(&terminal->lock);

  node * cur_node;
  
	switch( _notification->GetType() ) {
  
    // Add the new value to our list if it doesn't already exists
		case Notification::Type_ValueAdded: {
      cur_node = get_device_node( terminal, _notification->GetNodeId() );
			if( cur_node != NULL ) {
        if (get_device_value_id(cur_node, _notification->GetValueID().GetCommandClassId()) == NULL) {
          cur_node->values.push_back( _notification->GetValueID() );
          log_message(LOG_INFO, "Adding ValueID type %x to node %d", _notification->GetValueID().GetCommandClassId(), cur_node->node_id);
        }
			}
			break;
    }

    // Remove the value from out list if it exists
		case Notification::Type_ValueRemoved: {
      cur_node = get_device_node( terminal, _notification->GetNodeId() );
			if( cur_node != NULL ) {
				for( list<ValueID>::iterator it = cur_node->values.begin(); it != cur_node->values.end(); ++it ) {
					if( (*it) == _notification->GetValueID() ) {
						cur_node->values.erase( it );
            log_message(LOG_INFO, "Removing ValueID type %x from node %d", _notification->GetValueID().GetCommandClassId(), cur_node->node_id);
						break;
					}
				}
			}
			break;
    }

		case Notification::Type_ValueChanged: {
			// One of the node values has changed
      // Nothing there
			break;
    }

		case Notification::Type_Group: {
			// One of the node's association groups has changed
      // Nothing there
			break;
    }

		case Notification::Type_NodeAdded: {
			// Add the new node to the device's nodes list
			node * cur_node = new node();
			cur_node->home_id = _notification->GetHomeId();
			cur_node->node_id = _notification->GetNodeId();
			cur_node->polled = false;
			nodes_list->push_back( cur_node );
      log_message(LOG_INFO, "Adding Node %d", cur_node->node_id);
			break;
    }

		case Notification::Type_NodeRemoved: {
			// Remove the node from the device's nodes list
			for( list<node*>::iterator it = nodes_list->begin(); it != nodes_list->end(); ++it ) {
				node* cur_node = *it;
				if( get_device_node( terminal, cur_node->node_id != NULL ) ) {
          log_message(LOG_INFO, "Removing Node %d", cur_node->node_id);
					nodes_list->erase( it );
					delete cur_node;
					break;
				}
			}
			break;
    }

		case Notification::Type_NodeEvent: {
			// Event received
      // Not used yet
			break;
    }

		case Notification::Type_PollingDisabled: {
      // Polling is disabled for this node
			cur_node = get_device_node( terminal, _notification->GetNodeId() );
			if( cur_node != NULL ) {
        log_message(LOG_INFO, "Disabling polling for Node %d", cur_node->node_id);
				cur_node->polled = false;
			}
			break;
    }

		case Notification::Type_PollingEnabled: {
      // Polling is enabled for this node
			cur_node = get_device_node( terminal, _notification->GetNodeId() );
			if( cur_node != NULL ) {
        log_message(LOG_INFO, "Enabling polling for Node %d", cur_node->node_id);
				cur_node->polled = true;
			}
			break;
    }

		case Notification::Type_DriverReady: {
      log_message(LOG_INFO, "Driver ready");
			zwave_terminal->home_id = _notification->GetHomeId();
			zwave_terminal->init_failed = 0;
			break;
    }

		case Notification::Type_DriverFailed: {
      log_message(LOG_INFO, "Driver failed");
			zwave_terminal->home_id = UNDEFINED_HOME_ID;
			zwave_terminal->init_failed = 1;
			break;
    }

    case Notification::Type_DriverRemoved: {
      break;
    }
    
		case Notification::Type_DriverReset: {
      // Remove all nodes for the device
			/*for( list<node*>::iterator it = nodes_list->begin(); it != nodes_list->end(); ++it ) {
				node* cur_node = *it;
        nodes_list->erase( it );
        delete cur_node;
			}
			zwave_terminal->home_id = UNDEFINED_HOME_ID;*/
      break;
    }
      
		case Notification::Type_Notification:
      // An error has occured that we need to report.
		case Notification::Type_NodeNaming:
		case Notification::Type_NodeProtocolInfo:
		default: {
      break;
    }
	}

	pthread_mutex_unlock( &terminal->lock );
}

/**
 * get the connection status
 */
int is_connected_zwave(device * terminal) {
  return 1;
  //return (((struct _zwave_device *) terminal->element)->home_id != UNDEFINED_HOME_ID);
}

/**
 * connect the zwave dongle
 * creates the Manager openzwave object
 */
int connect_device_zwave(device * terminal, device ** terminals, unsigned int nb_terminal) {
  int i;
  char filename[WORDLENGTH+1];
  
  ((struct _zwave_device *) terminal->element)->nodes_list = new list<node*>();
  
  Options::Create( ((struct _zwave_device *) terminal->element)->config_path, ((struct _zwave_device *) terminal->element)->user_path, ((struct _zwave_device *) terminal->element)->command_line );
  Options::Get()->AddOptionString( "LogFileName", ((struct _zwave_device *) terminal->element)->log_path, false );
#ifdef DEBUG
	Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
	Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
	Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
#else
  Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Warning );
  Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Warning  );
  Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Warning  );
#endif
  Options::Get()->AddOptionInt( "PollInterval", 500 );

  Options::Get()->AddOptionBool( "ConsoleOutput", false );

  Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
  Options::Get()->AddOptionBool( "ValidateValueChanges", true);
  Options::Get()->Lock();

  Manager::Create();
  Manager::Get()->AddWatcher( on_notification_zwave, terminal );
  
  // Loo into uris to find the good one
  for (i=0; i<128; i++) {
    snprintf(filename, WORDLENGTH, "%s%d", terminal->uri, i);
    if (Manager::Get()->AddDriver( filename )) {
      terminal->enabled=1;
      snprintf(((struct _zwave_device *)terminal->element)->usb_file, WORDLENGTH, "%s", filename);
      return 1;
    }
  }
  log_message(LOG_INFO, "Error adding zwave dongle");
  return -1;
}

/**
 * Reconnects the zwave dongle
 * Not implemented yet
 */
int reconnect_device_zwave(device * terminal, device ** terminals, unsigned int nb_terminal) {
  return 1;
}

/**
 * Diconnects the driver, then remove all nodes
 */
int close_device_zwave(device * terminal) {
  Manager::Get()->RemoveDriver(((struct _zwave_device *)terminal->element)->usb_file);
  return 1;
}

/**
 * check if the zwave device is alive
 */
int send_heartbeat_zwave(device * terminal) {
  return is_connected_zwave(terminal);
}

/**
 * Returns an overview of all zwave devices connected and their last status
 */
char * get_overview_zwave(sqlite3 * sqlite3_db, device * terminal) {
  char * str_switches = NULL, * str_dimmers = NULL, * str_sensors = NULL, * str_heaters = NULL;
  int nb_switches = 0, nb_dimmers = 0;
  switcher * switches = NULL;
  dimmer * dimmers = NULL;
  
  char * output  = NULL, * tags = NULL, ** tags_array = NULL, one_element[MSGLENGTH+1], sanitized[WORDLENGTH+1]={0}, tmp_value[WORDLENGTH+1]={0};
  int output_len = 0, i = 0;

  char sql_query[MSGLENGTH+1];
  sqlite3_stmt *stmt;
  int sql_result, row_result;

  list<node*> * nodes_list = (list<node*> *) ((struct _zwave_device *) terminal->element)->nodes_list;
  for( list<node*>::iterator it = nodes_list->begin(); it != nodes_list->end(); ++it ) {
    node* node = *it;
    for( list<ValueID>::iterator it2 = node->values.begin(); it2 != node->values.end(); ++it2 ) {
      ValueID v = *it2;
      // getting switches (COMMAND_CLASS_SWITCH_BINARY)
      
      if ( v.GetCommandClassId() == COMMAND_CLASS_SWITCH_BINARY ) { //COMMAND_CLASS_SWITCH_BINARY
        switches = (switcher *) realloc(switches, (nb_switches+1)*sizeof(struct _switcher));
        snprintf(switches[nb_switches].name, WORDLENGTH, "%d", node->node_id);
        switches[nb_switches].status = get_switch_state_zwave(terminal, switches[nb_switches].name, 1);
        
        // Default values
        switches[nb_switches].type = 0;
        switches[nb_switches].monitored = 0;
        switches[nb_switches].monitored_every = 0;
        switches[nb_switches].monitored_next = 0;
        sqlite3_snprintf(MSGLENGTH, 
          sql_query, 
          "SELECT sw_display, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next FROM an_switch WHERE sw_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", 
          switches[nb_switches].name, 
          terminal->name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_INFO, "Error preparing sql query switch fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            sanitize_json_string((char*)sqlite3_column_text(stmt, 0), switches[nb_switches].display, WORDLENGTH);
            switches[nb_switches].enabled = sqlite3_column_int(stmt, 1);
            switches[nb_switches].type = sqlite3_column_int(stmt, 2);
            switches[nb_switches].monitored = sqlite3_column_int(stmt, 3);
            switches[nb_switches].monitored_every = sqlite3_column_int(stmt, 4);
            switches[nb_switches].monitored_next = sqlite3_column_int(stmt, 5);
          } else {
            // No result, default value
            snprintf(switches[nb_switches].display, WORDLENGTH, "%s", switches[nb_switches].name);
            switches[nb_switches].enabled = 1;
            
            // Creating data in database
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_switch (de_id, sw_name, sw_display, sw_status, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next) VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d', 1, 0, 0, 0, 0)", terminal->name, switches[nb_switches].name, switches[nb_switches].name, switches[nb_switches].status);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_INFO, "Error inserting an_switch %s", sql_query);
            }
          }
        }
        sqlite3_finalize(stmt);
        nb_switches++;
      } else if ( v.GetCommandClassId() == 0x26 ) { // COMMAND_CLASS_SWITCH_MULTILEVEL - Dimmer
        dimmers = (dimmer *) realloc(dimmers, (nb_dimmers+1)*sizeof(struct _dimmer));
        snprintf(dimmers[nb_dimmers].name, WORDLENGTH, "%d", node->node_id);
        dimmers[nb_dimmers].value = get_dimmer_value_zwave(terminal, dimmers[nb_dimmers].name);
        
        // Default values
        dimmers[nb_dimmers].monitored = 0;
        dimmers[nb_dimmers].monitored_every = 0;
        dimmers[nb_dimmers].monitored_next = 0;
        sqlite3_snprintf(MSGLENGTH, 
          sql_query, 
          "SELECT di_display, di_active, di_monitored, di_monitored_every, di_monitored_next FROM an_dimmer WHERE di_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", 
          dimmers[nb_dimmers].name,
          terminal->name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_INFO, "Error preparing sql query dimmer fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            sanitize_json_string((char*)sqlite3_column_text(stmt, 0), dimmers[nb_dimmers].display, WORDLENGTH);
            dimmers[nb_dimmers].enabled = sqlite3_column_int(stmt, 1);
            dimmers[nb_dimmers].monitored = sqlite3_column_int(stmt, 2);
            dimmers[nb_dimmers].monitored_every = sqlite3_column_int(stmt, 3);
            dimmers[nb_dimmers].monitored_next = sqlite3_column_int(stmt, 4);
          } else {
            // No result, default value
            snprintf(dimmers[nb_dimmers].display, WORDLENGTH, "%s", dimmers[nb_dimmers].name);
            dimmers[nb_dimmers].enabled = 1;
            
            // Creating data in database
            sqlite3_snprintf(MSGLENGTH, sql_query, "INSERT INTO an_dimmer (de_id, di_name, di_display, di_value, di_monitored, di_monitored_every, di_monitored_next) VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d', 0, 0, 0)", terminal->name, dimmers[nb_dimmers].name, dimmers[nb_dimmers].name, dimmers[nb_dimmers].value);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_INFO, "Error inserting an_dimmer %s", sql_query);
            }
          }
        }
        sqlite3_finalize(stmt);
        nb_dimmers++;
      }
    }
  }

  // Constructs output string
  str_switches = (char *)malloc(2*sizeof(char));
  strcpy(str_switches, "[");
  for (i=0; i<nb_switches; i++) {
    tags_array = get_tags(sqlite3_db, terminal->name, DATA_SWITCH, switches[i].name);
    tags = build_json_tags(tags_array);
    strcpy(one_element, "");
    if (i>0) {
      strncat(one_element, ",", MSGLENGTH);
    }
    strncat(one_element, "{\"name\":\"", MSGLENGTH);
    sanitize_json_string(switches[i].name, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"display\":\"", MSGLENGTH);
    sanitize_json_string(switches[i].display, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"status\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", switches[i].status);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"type\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", switches[i].type);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"enabled\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", switches[i].enabled?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", switches[i].monitored?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_every\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", switches[i].monitored_every);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_next\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%ld", switches[i].monitored_next);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"tags\":", MSGLENGTH);
    str_switches = (char *) realloc(str_switches, strlen(str_switches)+strlen(one_element)+strlen(tags)+2);
    strcat(str_switches, one_element);
    strcat(str_switches, tags);
    strcat(str_switches, "}");
    free(tags);
    free_tags(tags_array);
  }
  str_switches = (char *) realloc(str_switches, strlen(str_switches)+2);
  strcat(str_switches, "]");

  str_dimmers = (char *)malloc(2*sizeof(char));
  strcpy(str_dimmers, "[");
  for (i=0; i<nb_dimmers; i++) {
    tags_array = get_tags(sqlite3_db, terminal->name, DATA_SWITCH, dimmers[i].name);
    tags = build_json_tags(tags_array);
    strcpy(one_element, "");
    if (i>0) {
      strncat(one_element, ",", MSGLENGTH);
    }
    strncat(one_element, "{\"name\":\"", MSGLENGTH);
    sanitize_json_string(dimmers[i].name, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"display\":\"", MSGLENGTH);
    sanitize_json_string(dimmers[i].display, sanitized, WORDLENGTH);
    strncat(one_element, sanitized, MSGLENGTH);
    strncat(one_element, "\",\"value\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", dimmers[i].value);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"enabled\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", dimmers[i].enabled?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%s", dimmers[i].monitored?"true":"false");
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_every\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%d", dimmers[i].monitored_every);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"monitored_next\":", MSGLENGTH);
    snprintf(tmp_value, WORDLENGTH, "%ld", dimmers[i].monitored_next);
    strncat(one_element, tmp_value, MSGLENGTH);
    strncat(one_element, ",\"tags\":", MSGLENGTH);
    str_dimmers = (char *) realloc(str_dimmers, strlen(str_dimmers)+strlen(one_element)+strlen(tags)+2);
    strcat(str_dimmers, one_element);
    strcat(str_dimmers, tags);
    strcat(str_dimmers, "}");
    free(tags);
    free_tags(tags_array);
  }
  str_dimmers = (char *) realloc(str_dimmers, strlen(str_dimmers)+2);
  strcat(str_dimmers, "]");

  // TODO
  str_sensors = (char *) malloc(3*sizeof(char));
  strcpy(str_sensors, "[]");
  str_heaters = (char *) malloc(3*sizeof(char));
  strcpy(str_heaters, "[]");
  
  output_len = 59+strlen(terminal->name)+strlen(str_switches)+strlen(str_dimmers)+strlen(str_sensors)+strlen(str_heaters);
  output = (char *) malloc(output_len*sizeof(char));
  snprintf(output, output_len-1, "{\"name\":\"%s\",\"switches\":%s,\"dimmers\":%s,\"sensors\":%s,\"heaters\":%s}", terminal->name, str_switches, str_dimmers, str_sensors, str_heaters);
  
  // Free all allocated pointers before return
  free(switches);
  switches = NULL;
  free(str_switches);
  str_switches = NULL;
  free(dimmers);
  dimmers = NULL;
  free(str_dimmers);
  str_dimmers = NULL;
  free(str_sensors);
  str_sensors = NULL;
  free(str_heaters);
  str_heaters = NULL;
  return output;
}

/**
 * Refresh the zwave devices values
 */
char * get_refresh_zwave(sqlite3 * sqlite3_db, device * terminal) {
  return get_overview_zwave(sqlite3_db, terminal);
}

/**
 * get the home_id of the zwave
 */
int get_name_zwave(device * terminal, char * output) {
  return ((zwave_device *) terminal->element)->home_id;
}

/**
 * get the status of a switch
 */
int get_switch_state_zwave(device * terminal, char * switcher, int force) {
  ValueID * v = NULL;
  bool b_status;
  uint8 node_id;
  char * end_ptr;
  
  node_id = strtol(switcher, &end_ptr, 10);
  if (switcher == end_ptr) {
    return ERROR_SWITCH;
  } else {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY);
    if (v != NULL) {
      if (force) {
        Manager::Get()->RefreshValue((*get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY)));
      }
      if (Manager::Get()->GetValueAsBool((*get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY)), &b_status)) {
        return (b_status?1:0);
      }
    }
  }
  return ERROR_SWITCH;
}

/**
 * set the status of a switch
 */
int set_switch_state_zwave(device * terminal, char * switcher, int status) {
  ValueID * v = NULL;
  uint8 node_id;
  char * end_ptr;
  
  node_id = strtol(switcher, &end_ptr, 10);
  if (switcher == end_ptr) {
    return ERROR_SWITCH;
  } else {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY);
    if (v != NULL && Manager::Get()->SetValue((*v), (status?true:false))) {
      log_message(LOG_INFO, "setting node %s to status %d", switcher, status);
      return get_switch_state_zwave(terminal, switcher, 1);
    }
  }
  return ERROR_SWITCH;
}

/**
 * toggle the status of a switch
 */
int toggle_switch_state_zwave(device * terminal, char * switcher) {
  return set_switch_state_zwave(terminal, switcher, !get_switch_state_zwave(terminal, switcher, 1));
}

/**
 * get the value of a dimmer
 */
int get_dimmer_value_zwave(device * terminal, char * dimmer) {
  ValueID * v = NULL;
  string * s_status = NULL;
  int i_status = ERROR_DIMMER;
  uint8 node_id;
  char * end_ptr;
  
  node_id = strtol(dimmer, &end_ptr, 10);
  if (dimmer != end_ptr) {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_MULTILEVEL);
    if (v != NULL) {
      Manager::Get()->RefreshValue((*v));
      s_status = new string();
      if (Manager::Get()->GetValueAsString((*v), s_status)) {
        i_status = strtol(s_status->c_str(), NULL, 10);
      }
      delete s_status;
    }
  }
  return i_status;
}

/**
 * set the value of a dimmer
 */
int set_dimmer_value_zwave(device * terminal, char * dimmer, int value) {
  ValueID * v = NULL;
  char val[4];
  uint8 node_id;
  char * end_ptr;
  
  node_id = strtol(dimmer, &end_ptr, 10);
  if (dimmer == end_ptr) {
    return ERROR_DIMMER;
  } else {
    snprintf(val, 3, "%d", value);
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_MULTILEVEL);
    if (v != NULL && Manager::Get()->SetValue((*v), string(val)) ) {
      log_message(LOG_INFO, "setting dimmer %s to value %d", dimmer, value);
      Manager::Get()->RefreshValue((*get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_MULTILEVEL)));
      return value;
    }
  }
  return ERROR_DIMMER;
}

/**
 * get the value of a sensor
 */
float get_sensor_value_zwave(device * terminal, char * sensor, int force) {
  return ERROR_SENSOR;
}

/**
 * get the status of a heater
 */
int get_heater_zwave(device * terminal, char * heat_id, char * buffer) {
  return ERROR_HEATER;
}

/**
 * set the status of a heater
 */
int set_heater_zwave(device * terminal, char * heat_id, int heat_enabled, float max_heat_value, char * buffer) {
  return ERROR_HEATER;
}
