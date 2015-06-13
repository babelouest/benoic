/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * ZWave devices calls
 *
 * Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
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

#ifdef __cplusplus
extern "C" {
#endif

#include "angharad.h"

#ifdef __cplusplus
}
#endif

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
#define COMMAND_CLASS_SWITCH_BINARY               0x25
#define COMMAND_CLASS_SWITCH_MULTILEVEL           0x26
#define COMMAND_CLASS_SENSOR_MULTILEVEL           0x31
#define COMMAND_CLASS_THERMOSTAT_MODE             0x40
#define COMMAND_CLASS_THERMOSTAT_OPERATING_STATE  0x42
#define COMMAND_CLASS_THERMOSTAT_SETPOINT         0x43

#define COMMAND_CLASS_THERMOSTAT_OPERATING_STATE_IDLE     "Idle"
#define COMMAND_CLASS_THERMOSTAT_OPERATING_STATE_HEATING  "Heating"
#define COMMAND_CLASS_THERMOSTAT_MODE_HEAT                "Heat"
#define COMMAND_CLASS_THERMOSTAT_MODE_COOL                "Cool"
#define COMMAND_CLASS_THERMOSTAT_MODE_OFF                 "Off"

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
          log_message(LOG_LEVEL_DEBUG, "Adding ValueID type %x to node %d", _notification->GetValueID().GetCommandClassId(), cur_node->node_id);
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
            log_message(LOG_LEVEL_DEBUG, "Removing ValueID type %x from node %d", _notification->GetValueID().GetCommandClassId(), cur_node->node_id);
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
      log_message(LOG_LEVEL_DEBUG, "Adding Node %d", cur_node->node_id);
			break;
    }

		case Notification::Type_NodeRemoved: {
			// Remove the node from the device's nodes list
			for( list<node*>::iterator it = nodes_list->begin(); it != nodes_list->end(); ++it ) {
				node* cur_node = *it;
				if( get_device_node( terminal, cur_node->node_id != NULL ) ) {
          log_message(LOG_LEVEL_DEBUG, "Removing Node %d", cur_node->node_id);
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
        log_message(LOG_LEVEL_DEBUG, "Disabling polling for Node %d", cur_node->node_id);
				cur_node->polled = false;
			}
			break;
    }

		case Notification::Type_PollingEnabled: {
      // Polling is enabled for this node
			cur_node = get_device_node( terminal, _notification->GetNodeId() );
			if( cur_node != NULL ) {
        log_message(LOG_LEVEL_DEBUG, "Enabling polling for Node %d", cur_node->node_id);
				cur_node->polled = true;
			}
			break;
    }

		case Notification::Type_DriverReady: {
      log_message(LOG_LEVEL_INFO, "Driver ready");
			zwave_terminal->home_id = _notification->GetHomeId();
			zwave_terminal->init_failed = 0;
			break;
    }

		case Notification::Type_DriverFailed: {
      log_message(LOG_LEVEL_WARNING, "Driver failed");
			zwave_terminal->home_id = UNDEFINED_HOME_ID;
			zwave_terminal->init_failed = 1;
			break;
    }

    case Notification::Type_DriverRemoved: {
      break;
    }
    
		case Notification::Type_DriverReset: {
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
    snprintf(filename, WORDLENGTH*sizeof(char), "%s%d", terminal->uri, i);
    if (Manager::Get()->AddDriver( filename )) {
      terminal->enabled=1;
      snprintf(((struct _zwave_device *)terminal->element)->usb_file, WORDLENGTH*sizeof(char), "%s", filename);
      return 1;
    }
  }
  log_message(LOG_LEVEL_WARNING, "Error adding zwave dongle");
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
  int nb_switchers = 0, nb_dimmers = 0, nb_sensors = 0, nb_heaters = 0;
  switcher * switchers = NULL;
  dimmer * dimmers = NULL;
  sensor * sensors = NULL;
  heater * heaters = NULL;
  char * to_return = NULL;
  
  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;
  
  
  sql_query = sqlite3_mprintf("SELECT de_id FROM an_device WHERE de_name='%q'", terminal->name);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query (parse_overview_arduino)");
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result != SQLITE_ROW) {
      sql_query = sqlite3_mprintf("INSERT INTO an_device (de_name, de_display, de_active) VALUES ('%q', '%q', 1)", terminal->name, terminal->name);
      if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
        log_message(LOG_LEVEL_WARNING, "Error inserting Device");
      }
      sqlite3_free(sql_query);
    }
  }
  sqlite3_finalize(stmt);

  list<node*> * nodes_list = (list<node*> *) ((struct _zwave_device *) terminal->element)->nodes_list;
  for( list<node*>::iterator it = nodes_list->begin(); it != nodes_list->end(); ++it ) {
    node* node = *it;
    for( list<ValueID>::iterator it2 = node->values.begin(); it2 != node->values.end(); ++it2 ) {
      ValueID v = *it2;
      // getting switchers (COMMAND_CLASS_SWITCH_BINARY)
      
      if ( v.GetCommandClassId() == COMMAND_CLASS_SWITCH_BINARY ) { //COMMAND_CLASS_SWITCH_BINARY
        switchers = (switcher *) realloc(switchers, (nb_switchers+1)*sizeof(struct _switcher));
        snprintf(switchers[nb_switchers].name, WORDLENGTH*sizeof(char), "%d", node->node_id);
        switchers[nb_switchers].status = get_switch_state_zwave(terminal, switchers[nb_switchers].name, 1);
        
        // Default values
        switchers[nb_switchers].type = 0;
        switchers[nb_switchers].monitored = 0;
        switchers[nb_switchers].monitored_every = 0;
        switchers[nb_switchers].monitored_next = 0;
        sql_query = sqlite3_mprintf("SELECT sw_display, sw_active, sw_type, sw_monitored, sw_monitored_every, sw_monitored_next FROM an_switch\
          WHERE sw_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", 
          switchers[nb_switchers].name, 
          terminal->name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        sqlite3_free(sql_query);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_LEVEL_WARNING, "Error preparing sql query switch fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            strncpy(switchers[nb_switchers].display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
            switchers[nb_switchers].enabled = sqlite3_column_int(stmt, 1);
            switchers[nb_switchers].type = sqlite3_column_int(stmt, 2);
            switchers[nb_switchers].monitored = sqlite3_column_int(stmt, 3);
            switchers[nb_switchers].monitored_every = sqlite3_column_int(stmt, 4);
            switchers[nb_switchers].monitored_next = sqlite3_column_int(stmt, 5);
          } else {
            // No result, default value
            snprintf(switchers[nb_switchers].display, WORDLENGTH*sizeof(char), "%s", switchers[nb_switchers].name);
            switchers[nb_switchers].enabled = 1;
            
            // Creating data in database
            sql_query = sqlite3_mprintf("INSERT INTO an_switch (de_id, sw_name, sw_display, sw_status, sw_active, sw_type,\
                              sw_monitored, sw_monitored_every, sw_monitored_next) VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'),\
                              '%q', '%q', '%d', 1, 0, 0, 0, 0)",
                              terminal->name, switchers[nb_switchers].name, switchers[nb_switchers].name, switchers[nb_switchers].status);
            
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_LEVEL_WARNING, "Error inserting an_switch %s", sql_query);
            }
            sqlite3_free(sql_query);
          }
        }
        sqlite3_finalize(stmt);
        nb_switchers++;
      } else if ( v.GetCommandClassId() == COMMAND_CLASS_SWITCH_MULTILEVEL ) { // COMMAND_CLASS_SWITCH_MULTILEVEL - Dimmer
        dimmers = (dimmer *) realloc(dimmers, (nb_dimmers+1)*sizeof(struct _dimmer));
        snprintf(dimmers[nb_dimmers].name, WORDLENGTH*sizeof(char), "%d", node->node_id);
        dimmers[nb_dimmers].value = get_dimmer_value_zwave(terminal, dimmers[nb_dimmers].name);
        
        // Default values
        dimmers[nb_dimmers].monitored = 0;
        dimmers[nb_dimmers].monitored_every = 0;
        dimmers[nb_dimmers].monitored_next = 0;
        sql_query = sqlite3_mprintf("SELECT di_display, di_active, di_monitored, di_monitored_every, di_monitored_next FROM an_dimmer\
          WHERE di_name='%q' AND de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", 
          dimmers[nb_dimmers].name,
          terminal->name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        sqlite3_free(sql_query);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_LEVEL_WARNING, "Error preparing sql query dimmer fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            strncpy(dimmers[nb_dimmers].display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
            dimmers[nb_dimmers].enabled = sqlite3_column_int(stmt, 1);
            dimmers[nb_dimmers].monitored = sqlite3_column_int(stmt, 2);
            dimmers[nb_dimmers].monitored_every = sqlite3_column_int(stmt, 3);
            dimmers[nb_dimmers].monitored_next = sqlite3_column_int(stmt, 4);
          } else {
            // No result, default value
            snprintf(dimmers[nb_dimmers].display, WORDLENGTH*sizeof(char), "%s", dimmers[nb_dimmers].name);
            dimmers[nb_dimmers].enabled = 1;
            
            // Creating data in database
            sql_query = sqlite3_mprintf("INSERT INTO an_dimmer\
                              (de_id, di_name, di_display, di_value, di_monitored, di_monitored_every, di_monitored_next)\
                              VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', '%d', 0, 0, 0)",
                              terminal->name, dimmers[nb_dimmers].name, dimmers[nb_dimmers].name, dimmers[nb_dimmers].value);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_LEVEL_WARNING, "Error inserting an_dimmer");
            }
            sqlite3_free(sql_query);
          }
        }
        sqlite3_finalize(stmt);
        nb_dimmers++;
      } else if ( v.GetCommandClassId() == COMMAND_CLASS_SENSOR_MULTILEVEL ) { // COMMAND_CLASS_SENSOR_MULTILEVEL - sensor
        sensors = (sensor *) realloc(sensors, (nb_sensors+1)*sizeof(struct _sensor));
        snprintf(sensors[nb_sensors].name, WORDLENGTH*sizeof(char), "%d", node->node_id);
        snprintf(sensors[nb_sensors].display, WORDLENGTH*sizeof(char), "%s", sensors[nb_sensors].name);
        strcpy(sensors[nb_sensors].unit, "");
        sensors[nb_sensors].value_type = VALUE_TYPE_NONE;
        sensors[nb_sensors].enabled = 1;
        sensors[nb_sensors].monitored = 0;
        sensors[nb_sensors].monitored_every = 0;
        sensors[nb_sensors].monitored_next = 0;
        sql_query = sqlite3_mprintf("SELECT se_display, se_unit, se_value_type, se_active, se_monitored, se_monitored_every, se_monitored_next\
                        FROM an_sensor WHERE se_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", sensors[nb_sensors].name, terminal->name);
        sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
        sqlite3_free(sql_query);
        if (sql_result != SQLITE_OK) {
          log_message(LOG_LEVEL_WARNING, "Error preparing sql query sensor fetch");
        } else {
          row_result = sqlite3_step(stmt);
          if (row_result == SQLITE_ROW) {
            strncpy(sensors[nb_sensors].display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
            strncpy(sensors[nb_sensors].unit, (char*)sqlite3_column_text(stmt, 1), WORDLENGTH);
            sensors[nb_sensors].value_type = sqlite3_column_int(stmt, 2);
            sensors[nb_sensors].enabled = sqlite3_column_int(stmt, 3);
            sensors[nb_sensors].monitored = sqlite3_column_int(stmt, 4);
            sensors[nb_sensors].monitored_every = sqlite3_column_int(stmt, 5);
            sensors[nb_sensors].monitored_next = sqlite3_column_int(stmt, 6);
          } else {
            // Creating data in database
            sql_query = sqlite3_mprintf("INSERT INTO an_sensor\
                              (de_id, se_name, se_display, se_active, se_unit, se_monitored, se_monitored_every, se_monitored_next)\
                              VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', 1, '', 0, 0, 0)",
                              terminal->name, sensors[nb_sensors].name, sensors[nb_sensors].name);
            if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
              log_message(LOG_LEVEL_WARNING, "Error inserting an_sensor %s", sql_query);
            }
            sqlite3_free(sql_query);
          }
        }
        sqlite3_finalize(stmt);
        if (sensors[nb_sensors].value_type == VALUE_TYPE_FAHRENHEIT) {
          snprintf(sensors[nb_sensors].value, WORDLENGTH*sizeof(char), "%.2f", fahrenheit_to_celsius(get_sensor_value_zwave(terminal, sensors[nb_sensors].name, 0)));
        } else {
          snprintf(sensors[nb_sensors].value, WORDLENGTH*sizeof(char), "%.2f", get_sensor_value_zwave(terminal, sensors[nb_sensors].name, 0));
        }
        nb_sensors++;
      } else if ( v.GetCommandClassId() == COMMAND_CLASS_THERMOSTAT_OPERATING_STATE
                || v.GetCommandClassId() == COMMAND_CLASS_THERMOSTAT_MODE
                || v.GetCommandClassId() == COMMAND_CLASS_THERMOSTAT_SETPOINT ) {
        heater * cur_heater = NULL;
        int i;
        char node_id[WORDLENGTH+1];
        
        snprintf(node_id, WORDLENGTH, "%d", node->node_id);
        for (i=0; i < nb_heaters; i++) {
          if (strncmp(heaters[i].name, node_id, WORDLENGTH) == 0) {
            // Heater found
            cur_heater = &heaters[i];
          }
        }
        
        if (cur_heater == NULL) {
          heaters = (heater *) realloc(heaters, (nb_heaters+1)*sizeof(heater));
          cur_heater = &heaters[nb_heaters];
          snprintf(cur_heater->name, WORDLENGTH*sizeof(char), "%d", node->node_id);
          snprintf(cur_heater->display, WORDLENGTH*sizeof(char), "%d", node->node_id);
          strcpy(cur_heater->unit, "");
          cur_heater->value_type = VALUE_TYPE_NONE;
          cur_heater->enabled = 1;
          cur_heater->monitored = 0;
          cur_heater->monitored_every = 0;
          cur_heater->monitored_next = 0;
          sql_query = sqlite3_mprintf("SELECT he_display, he_enabled, he_unit, he_value_type, he_active, he_monitored, he_monitored_every, he_monitored_next\
                          FROM an_heater WHERE he_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", cur_heater->name, terminal->name);
          sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
          sqlite3_free(sql_query);
          if (sql_result != SQLITE_OK) {
            log_message(LOG_LEVEL_WARNING, "Error preparing sql query heater fetch");
          } else {
            row_result = sqlite3_step(stmt);
            if (row_result == SQLITE_ROW) {
              strncpy(cur_heater->display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
              cur_heater->enabled = sqlite3_column_int(stmt, 1);
              strncpy(cur_heater->unit, (char*)sqlite3_column_text(stmt, 2), WORDLENGTH);
              cur_heater->value_type = sqlite3_column_int(stmt, 3);
              cur_heater->monitored = sqlite3_column_int(stmt, 4);
              cur_heater->monitored_every = sqlite3_column_int(stmt, 5);
              cur_heater->monitored_next = sqlite3_column_int(stmt, 6);
            } else {
              // Creating data in database
              sql_query = sqlite3_mprintf("INSERT INTO an_heater\
                                (de_id, he_name, he_display, he_active, he_unit, he_monitored, he_monitored_every, he_monitored_next)\
                                VALUES ((SELECT de_id FROM an_device WHERE de_name='%q'), '%q', '%q', 1, '', 0, 0, 0)",
                                terminal->name, cur_heater->name, cur_heater->name);
              if ( !sqlite3_exec(sqlite3_db, sql_query, NULL, NULL, NULL) == SQLITE_OK ) {
                log_message(LOG_LEVEL_WARNING, "Error inserting an_heater %s", sql_query);
              }
              sqlite3_free(sql_query);
            }
          }
          sqlite3_finalize(stmt);
          nb_heaters++;
        }
        
        // Processing CommandClass
        Manager::Get()->RefreshValue(v);
        if ( v.GetCommandClassId() == COMMAND_CLASS_THERMOSTAT_OPERATING_STATE ) {
          string * s_status = new string();
          Manager::Get()->GetValueAsString(v, s_status);
          if (strcmp(s_status->c_str(), COMMAND_CLASS_THERMOSTAT_OPERATING_STATE_HEATING) == 0) {
            cur_heater->on = 1;
          } else {
            cur_heater->on = 0;
          }
          delete(s_status);
        } else if ( v.GetCommandClassId() == COMMAND_CLASS_THERMOSTAT_MODE ) {
          string * s_status = new string();
          Manager::Get()->GetValueAsString(v, s_status);
          if (strcmp(s_status->c_str(), COMMAND_CLASS_THERMOSTAT_MODE_HEAT) == 0) {
            cur_heater->set = 1;
          } else {
            cur_heater->set = 0;
          }
          delete(s_status);
        } else if ( v.GetCommandClassId() == COMMAND_CLASS_THERMOSTAT_SETPOINT ) {
          string * s_status = new string();
          Manager::Get()->GetValueAsString(v, s_status);
          cur_heater->heat_max_value = strtof(s_status->c_str(), NULL);
          delete(s_status);
          if (cur_heater->value_type == VALUE_TYPE_FAHRENHEIT) {
            cur_heater->heat_max_value = fahrenheit_to_celsius(cur_heater->heat_max_value);
          }
        }
      }
    }
  }

  to_return = build_overview_output(sqlite3_db, terminal->name, switchers, nb_switchers, sensors, nb_sensors, heaters, nb_heaters, dimmers, nb_dimmers);
  free(switchers);
  free(dimmers);
  free(heaters);
  free(sensors);
  return to_return;
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
  int result = ERROR_SWITCH;
  char * end_ptr;
  
  node_id = strtol(switcher, &end_ptr, 10);
  if (switcher == end_ptr) {
    result = ERROR_SWITCH;
  } else {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY);
    if (v != NULL) {
      if (force) {
        Manager::Get()->RefreshValue(*v);
      }
      if (Manager::Get()->GetValueAsBool((*get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY)), &b_status)) {
        result = (b_status?1:0);
      }
    }
  }
  return result;
}

/**
 * set the status of a switch
 */
int set_switch_state_zwave(device * terminal, char * switcher, int status) {
  ValueID * v = NULL;
  uint8 node_id;
  char * end_ptr;
  int result = ERROR_SWITCH;
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  node_id = strtol(switcher, &end_ptr, 10);
  if (switcher == end_ptr) {
    result =  ERROR_SWITCH;
  } else {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_BINARY);
    if (v != NULL && Manager::Get()->SetValue((*v), (status?true:false))) {
      result =  get_switch_state_zwave(terminal, switcher, 1);
    }
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
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
int get_dimmer_value_zwave(device * terminal, char * dimmer_name) {
  ValueID * v = NULL;
  string * s_status = NULL;
  int result = ERROR_DIMMER;
  uint8 node_id;
  char * end_ptr;
  
  node_id = strtol(dimmer_name, &end_ptr, 10);
  if (dimmer_name != end_ptr) {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_MULTILEVEL);
    if (v != NULL) {
      Manager::Get()->RefreshValue(*v);
      s_status = new string();
      if (Manager::Get()->GetValueAsString((*v), s_status)) {
        result = strtol(s_status->c_str(), NULL, 10);
      }
      delete s_status;
    }
  }
  return result;
}

/**
 * set the value of a dimmer
 */
int set_dimmer_value_zwave(device * terminal, char * dimmer_name, int value) {
  ValueID * v = NULL;
  char val[4];
  uint8 node_id;
  char * end_ptr;
  int result = ERROR_DIMMER;
  
  if (pthread_mutex_lock(&terminal->lock)) {
    return result;
  }
  node_id = strtol(dimmer_name, &end_ptr, 10);
  if (dimmer_name != end_ptr) {
    if (value < 0) value = 0;
    if (value > 99) value = 99;
    snprintf(val, 3*sizeof(char), "%d", value);
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SWITCH_MULTILEVEL);
    if (v != NULL && Manager::Get()->SetValue((*v), string(val)) ) {
      result =  get_dimmer_value_zwave(terminal, dimmer_name);
    }
  }
  pthread_mutex_unlock(&terminal->lock);
  return result;
}

/**
 * get the value of a sensor
 */
float get_sensor_value_zwave(device * terminal, char * sensor_name, int force) {
  ValueID * v = NULL;
  string * s_status = NULL;
  float result = ERROR_SENSOR;
  uint8 node_id;
  char * end_ptr;
  
  node_id = strtol(sensor_name, &end_ptr, 10);
  if (sensor_name != end_ptr) {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_SENSOR_MULTILEVEL);
    if (v != NULL) {
      if (force) {
        Manager::Get()->RefreshValue(*v);
      }
      s_status = new string();
      if (Manager::Get()->GetValueAsString((*v), s_status)) {
        result = strtof(s_status->c_str(), NULL);
      }
      delete s_status;
    }
  }
  return result;
}

/**
 * get the status of a heater
 */
heater * get_heater_zwave(sqlite3 * sqlite3_db, device * terminal, char * heat_id) {
  ValueID * v = NULL;
  string * s_status = NULL;
  uint8 node_id;
  char * end_ptr;
  heater * cur_heater = (heater *)malloc(sizeof(heater));
  strncpy(cur_heater->name, heat_id, WORDLENGTH);
  cur_heater->value_type = VALUE_TYPE_NONE;
  strncpy(cur_heater->display, heat_id, WORDLENGTH);

  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;

  sql_query = sqlite3_mprintf("SELECT he_display, he_enabled, he_unit, he_value_type, he_monitored, he_monitored_every, he_monitored_next FROM an_heater\
                                WHERE he_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", heat_id, terminal->name);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query get_heater");
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      if (sqlite3_column_text(stmt, 0) != NULL) {
        strncpy(cur_heater->display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
      }
      cur_heater->enabled = sqlite3_column_int(stmt, 1);
      if (sqlite3_column_text(stmt, 2) != NULL) {
        strncpy(cur_heater->unit, (char*)sqlite3_column_text(stmt, 2), WORDLENGTH);
      }
      cur_heater->value_type = sqlite3_column_int(stmt, 3);
      cur_heater->monitored = sqlite3_column_int(stmt, 4);
      cur_heater->monitored_every = sqlite3_column_int(stmt, 5);
      cur_heater->monitored_next = sqlite3_column_int(stmt, 6);
    }
  }

  node_id = strtol(heat_id, &end_ptr, 10);
  if (heat_id != end_ptr) {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_THERMOSTAT_MODE);
    Manager::Get()->RefreshValue(*v);
    if (v != NULL) {
      s_status = new string();
      Manager::Get()->GetValueAsString(*v, s_status);
      if (strcmp(s_status->c_str(), COMMAND_CLASS_THERMOSTAT_MODE_HEAT) == 0) {
        cur_heater->set = 1;
      } else {
        cur_heater->set = 0;
      }
      delete(s_status);
    }
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_THERMOSTAT_OPERATING_STATE);
    Manager::Get()->RefreshValue(*v);
    if (v != NULL) {
      s_status = new string();
      Manager::Get()->GetValueAsString(*v, s_status);
      if (strcmp(s_status->c_str(), COMMAND_CLASS_THERMOSTAT_OPERATING_STATE_HEATING) == 0) {
        cur_heater->on = 1;
      } else {
        cur_heater->on = 0;
      }
      delete(s_status);
    }
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_THERMOSTAT_SETPOINT);
    Manager::Get()->RefreshValue(*v);
    if (v != NULL) {
      s_status = new string();
      Manager::Get()->GetValueAsString(*v, s_status);
      cur_heater->heat_max_value = strtof(s_status->c_str(), NULL);
      delete(s_status);
      if (cur_heater->value_type == VALUE_TYPE_FAHRENHEIT) {
        cur_heater->heat_max_value = fahrenheit_to_celsius(cur_heater->heat_max_value);
      }
    }
    return cur_heater;
  } else {
    return NULL;
  }
}

/**
 * set the status of a heater
 */
heater * set_heater_zwave(sqlite3 * sqlite3_db, device * terminal, char * heat_id, int heat_enabled, float max_heat_value) {
  ValueID * v = NULL;
  string * s_status = NULL;
  uint8 node_id;
  char * end_ptr;
  char val[4];
  
  heater * cur_heater = (heater *)malloc(sizeof(heater));
  strncpy(cur_heater->name, heat_id, WORDLENGTH);
  cur_heater->value_type = VALUE_TYPE_NONE;
  strncpy(cur_heater->display, heat_id, WORDLENGTH);
  cur_heater->set = heat_enabled;
  cur_heater->heat_max_value = max_heat_value;

  char * sql_query = NULL;
  sqlite3_stmt *stmt;
  int sql_result, row_result;

  sql_query = sqlite3_mprintf("SELECT he_display, he_enabled, he_unit, he_value_type, he_monitored, he_monitored_every, he_monitored_next FROM an_heater\
                                WHERE he_name='%q' and de_id IN (SELECT de_id FROM an_device WHERE de_name='%q')", heat_id, terminal->name);
  sql_result = sqlite3_prepare_v2(sqlite3_db, sql_query, strlen(sql_query)+1, &stmt, NULL);
  sqlite3_free(sql_query);
  if (sql_result != SQLITE_OK) {
    log_message(LOG_LEVEL_WARNING, "Error preparing sql query get_heater");
  } else {
    row_result = sqlite3_step(stmt);
    if (row_result == SQLITE_ROW) {
      if (sqlite3_column_text(stmt, 0) != NULL) {
        strncpy(cur_heater->display, (char*)sqlite3_column_text(stmt, 0), WORDLENGTH);
      }
      cur_heater->enabled = sqlite3_column_int(stmt, 1);
      if (sqlite3_column_text(stmt, 2) != NULL) {
        strncpy(cur_heater->unit, (char*)sqlite3_column_text(stmt, 2), WORDLENGTH);
      }
      cur_heater->value_type = sqlite3_column_int(stmt, 3);
      cur_heater->monitored = sqlite3_column_int(stmt, 4);
      cur_heater->monitored_every = sqlite3_column_int(stmt, 5);
      cur_heater->monitored_next = sqlite3_column_int(stmt, 6);
    }
  }

  node_id = strtol(heat_id, &end_ptr, 10);
  if (heat_id != end_ptr) {
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_THERMOSTAT_MODE);
    if (v != NULL) {
      if (cur_heater->set) {
        Manager::Get()->SetValue(*v, string(COMMAND_CLASS_THERMOSTAT_MODE_HEAT));
      } else {
        Manager::Get()->SetValue(*v, string(COMMAND_CLASS_THERMOSTAT_MODE_OFF));
      }
    }

    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_THERMOSTAT_SETPOINT);
    if (v != NULL) {
      if (cur_heater->value_type == VALUE_TYPE_FAHRENHEIT) {
        snprintf(val, 3, "%.0f", celsius_to_fahrenheit( cur_heater->heat_max_value) );
      } else {
        snprintf(val, 3, "%.0f", cur_heater->heat_max_value);
      }
      if (!Manager::Get()->SetValue(*v, string(val))) {
        free(cur_heater);
        return NULL;
      }
    }
    v = get_device_value_id(get_device_node(terminal, node_id), COMMAND_CLASS_THERMOSTAT_OPERATING_STATE);
    Manager::Get()->RefreshValue(*v);
    if (v != NULL) {
      s_status = new string();
      Manager::Get()->GetValueAsString(*v, s_status);
      if (strcmp(s_status->c_str(), COMMAND_CLASS_THERMOSTAT_OPERATING_STATE_HEATING) == 0) {
        cur_heater->on = 1;
      } else {
        cur_heater->on = 0;
      }
      delete(s_status);
    }
    return cur_heater;
  } else {
    return NULL;
  }
}
