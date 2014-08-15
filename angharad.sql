BEGIN;

CREATE TABLE an_action(
  ac_id INTEGER PRIMARY KEY AUTOINCREMENT,
  ac_name TEXT NOT NULL,
  ac_type INT NOT NULL, -- 0: DEVICES, 1: OVERVIEW, 2: REFRESH, 3: GET, 4: SET, 5: SENSOR, 6: HEATER, 99: SYSTEM
  de_id INT,
  sw_id INT,
  se_id INT,
  he_id INT,
  ac_params TEXT NOT NULL,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sw_id) REFERENCES an_switch(sw_id),
  FOREIGN KEY(se_id) REFERENCES an_sensor(se_id),
  FOREIGN KEY(he_id) REFERENCES an_heater(he_id)
);

CREATE TABLE an_script(
  sc_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sc_name TEXT NOT NULL,
  de_id INT,
  sc_enabled INT,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)
);

CREATE TABLE an_action_script(
  as_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sc_id INT NOT NULL,
  ac_id INT NOT NULL,
  as_rank INT NOT NULL,
  as_result_condition INT NOT NULL, -- Condition to evaluate for next action.
                    -- 0: None, 1: Equals, 2: Lower to, 3: Lower or equal to, 4: Higher or equal to, 5: Higher to, 6: Not equal, 7: Contains
  as_value_condition TEXT,
  FOREIGN KEY(ac_id) REFERENCES an_action(ac_id),
  FOREIGN KEY(sc_id) REFERENCES an_script(sc_id)
);

CREATE TABLE an_scheduler(
  sh_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sh_name TEXT,
  sh_enabled INT,
  sh_next_time INT,
  sh_repeat_schedule INT, -- -1: None, 0: minute, 1: hours, 2: days, 3: day of week, 4: month, 5: year
  sh_repeat_schedule_value INT,
  de_id INT,
  sc_id INT, -- script to run if scheduler condition is met
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sc_id) REFERENCES an_script(sc_id)
);

CREATE TABLE an_room(
  ro_id INTEGER PRIMARY KEY AUTOINCREMENT,
  ro_name TEXT NOT NULL;
);

CREATE TABLE an_device(
  de_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_name TEXT unique NOT NULL,
  de_display TEXT,
  de_active INT
);

CREATE TABLE an_switch(
  sw_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  --ro_id INT NOT NULL,
  sw_name TEXT NOT NULL,
  sw_display TEXT,
  sw_type INT DEFAULT 0, -- 0 Normally Open, 1 Normally Closed, 2 Three-way
  sw_active INT,
  sw_status INT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)--,
  --FOREIGN KEY(ro_id) REFERENCES an_room(ro_id)
);

CREATE TABLE an_sensor(
  se_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  --ro_id INT NOT NULL,
  se_name TEXT NOT NULL,
  se_display TEXT,
  se_unit TEXT,
  se_active INT,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)--,
  --FOREIGN KEY(ro_id) REFERENCES an_room(ro_id)
);

CREATE TABLE an_heater(
  he_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  --ro_id INT NOT NULL,
  he_name TEXT NOT NULL,
  he_display TEXT,
  he_enabled INT DEFAULT 1,
  he_set INT DEFAULT 0,
  he_max_heat_value FLOAT DEFAULT 0,
  he_unit TEXT,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)--,
  --FOREIGN KEY(ro_id) REFERENCES an_room(ro_id)
);

CREATE TABLE an_light(
  li_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  --ro_id INT NOT NULL,
  li_name TEXT NOT NULL,
  li_display TEXT,
  li_enabled INT NOT NULL,
  li_set INT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)--,
  --FOREIGN KEY(ro_id) REFERENCES an_room(ro_id)
);

CREATE TABLE an_journal(
  jo_id INTEGER PRIMARY KEY AUTOINCREMENT,
  jo_date TEXT NOT NULL,
  jo_origin TEXT NOT NULL,
  jo_command TEXT NOT NULL,
  jo_result TEXT NOT NULL
);

CREATE INDEX iaction ON an_action(ac_id);
CREATE INDEX iscript ON an_script(sc_id);
CREATE INDEX iaction_script ON an_action_script(ac_id, sc_id);
CREATE INDEX idevice ON an_device(de_id);
CREATE INDEX iswitch ON an_switch(sw_id);
CREATE INDEX isensor ON an_sensor(se_id);

COMMIT;
