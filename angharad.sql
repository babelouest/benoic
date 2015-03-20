--
--
-- Angharad server
--
-- Environment used to control home devices (switches, sensors, heaters, etc)
-- Using different protocols and controllers:
-- - Arduino UNO
-- - ZWave
--
-- Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
-- Gnu Public License V3 <http://fsf.org/>
--
-- main database creation script
--

BEGIN;

CREATE TABLE an_device(
  de_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_name TEXT unique NOT NULL,
  de_display TEXT,
  de_active INT
);
CREATE INDEX idevice ON an_device(de_id);

CREATE TABLE an_switch(
  sw_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  sw_name TEXT NOT NULL,
  sw_display TEXT,
  sw_type INT DEFAULT 0, -- 0 Normally Open, 1 Normally Closed, 2 Three-way
  sw_active INT DEFAULT 1,
  sw_status INT DEFAULT 0,
  sw_monitored INT DEFAULT 0,
  sw_monitored_every INT DEFAULT 0,
  sw_monitored_next TEXT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)
);
CREATE INDEX iswitch ON an_switch(sw_id);

CREATE TABLE an_sensor(
  se_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  se_name TEXT NOT NULL,
  se_display TEXT,
  se_unit TEXT,
  se_active INT DEFAULT 1,
  se_monitored INT DEFAULT 0,
  se_monitored_every INT DEFAULT 0,
  se_monitored_next TEXT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)
);
CREATE INDEX isensor ON an_sensor(se_id);

CREATE TABLE an_heater(
  he_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  he_name TEXT NOT NULL,
  he_display TEXT,
  he_enabled INT DEFAULT 1,
  he_set INT DEFAULT 0,
  he_max_heat_value FLOAT DEFAULT 0,
  he_unit TEXT,
  he_active DEFAULT 1,
  he_monitored INT DEFAULT 0,
  he_monitored_every INT DEFAULT 0,
  he_monitored_next TEXT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)
);
CREATE INDEX iheater ON an_heater(he_id);

CREATE TABLE an_dimmer(
  di_id INTEGER PRIMARY KEY AUTOINCREMENT,
  de_id INT NOT NULL,
  di_name TEXT NOT NULL,
  di_display TEXT,
  di_active INT DEFAULT 1,
  di_value INT DEFAULT 0,
  di_monitored INT DEFAULT 0,
  di_monitored_every INT DEFAULT 0,
  di_monitored_next TEXT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)
);
CREATE INDEX idimmer ON an_dimmer(di_id);

CREATE TABLE an_action(
  ac_id INTEGER PRIMARY KEY AUTOINCREMENT,
  ac_name TEXT NOT NULL,
  ac_type INT NOT NULL, -- 0: SET, 1: TOGGLE, 2: DIMMER, 3: HEATER, 77: SCRIPT, 88: SLEEP, 99: SYSTEM
  de_id INT,
  sw_id INT,
  se_id INT,
  he_id INT,
  di_id INT,
  ac_params TEXT,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sw_id) REFERENCES an_switch(sw_id),
  FOREIGN KEY(se_id) REFERENCES an_sensor(se_id),
  FOREIGN KEY(he_id) REFERENCES an_heater(he_id)
  FOREIGN KEY(di_id) REFERENCES an_dimmer(di_id)
);
CREATE INDEX iaction ON an_action(ac_id);

CREATE TABLE an_script(
  sc_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sc_name TEXT NOT NULL,
  de_id INT,
  sc_enabled INT,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id)
);
CREATE INDEX iscript ON an_script(sc_id);

CREATE TABLE an_action_script(
  as_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sc_id INT NOT NULL,
  ac_id INT NOT NULL,
  as_rank INT NOT NULL,
  as_enabled INT DEFAULT 1,
  FOREIGN KEY(ac_id) REFERENCES an_action(ac_id),
  FOREIGN KEY(sc_id) REFERENCES an_script(sc_id)
);
CREATE INDEX iaction_script ON an_action_script(ac_id, sc_id);

CREATE TABLE an_scheduler(
  sh_id INTEGER PRIMARY KEY AUTOINCREMENT,
  sh_name TEXT,
  sh_enabled INT,
  sh_next_time INT,
  sh_repeat_schedule INT, -- -1: None, 0: minute, 1: hours, 2: days, 3: day of week, 4: month, 5: year
  sh_repeat_schedule_value INT,
  sh_remove_after_done INT,
  de_id INT,
  sc_id INT, -- script to run if scheduler condition is met
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sc_id) REFERENCES an_script(sc_id)
);
CREATE INDEX ischeduler ON an_scheduler(sh_id);
CREATE INDEX ischedulernexttime ON an_scheduler(sh_next_time);

CREATE TABLE an_journal(
  jo_id INTEGER PRIMARY KEY AUTOINCREMENT,
  jo_date TEXT NOT NULL,
  jo_origin TEXT NOT NULL,
  jo_command TEXT NOT NULL,
  jo_result TEXT NOT NULL
);
CREATE INDEX ijournal ON an_journal(jo_id);
CREATE INDEX ijournaldate ON an_journal(jo_date);

CREATE TABLE an_monitor(
  mo_id INTEGER PRIMARY KEY AUTOINCREMENT,
  mo_date TEXT NOT NULL,
  de_id INT NOT NULL,
  sw_id INT,
  se_id INT,
  he_id INT,
  di_id INT,
  mo_result TEXT NOT NULL,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sw_id) REFERENCES an_switch(sw_id),
  FOREIGN KEY(se_id) REFERENCES an_sensor(se_id)
  FOREIGN KEY(he_id) REFERENCES an_heater(he_id)
  FOREIGN KEY(di_id) REFERENCES an_dimmer(di_id)
);
CREATE INDEX imonitor ON an_monitor(mo_id);
CREATE INDEX imonitordate ON an_monitor(mo_date);

CREATE TABLE an_tag(
  ta_id INTEGER PRIMARY KEY AUTOINCREMENT,
  ta_name TEXT NOT NULL
);
CREATE INDEX itag ON an_tag(ta_id);

CREATE TABLE an_tag_element(
  te_id INTEGER PRIMARY KEY AUTOINCREMENT,
  ta_id INT NOT NULL,
  de_id INT DEFAULT 0,
  sw_id INT DEFAULT 0,
  se_id INT DEFAULT 0,
  he_id INT DEFAULT 0,
  di_id INT DEFAULT 0,
  ac_id INT DEFAULT 0,
  sc_id INT DEFAULT 0,
  sh_id INT DEFAULT 0,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sw_id) REFERENCES an_switch(sw_id),
  FOREIGN KEY(se_id) REFERENCES an_sensor(se_id),
  FOREIGN KEY(he_id) REFERENCES an_heater(he_id),
  FOREIGN KEY(di_id) REFERENCES an_dimmer(di_id),
  FOREIGN KEY(ac_id) REFERENCES an_action(ac_id),
  FOREIGN KEY(sc_id) REFERENCES an_script(sc_id),
  FOREIGN KEY(sh_id) REFERENCES an_scheduler(sh_id)
);
CREATE INDEX itagelement ON an_tag_element(te_id);

COMMIT;
