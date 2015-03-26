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
-- archive database creation script
-- will store the archived journal and monitored entries
--

BEGIN;

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

CREATE TABLE an_archive_list(
  la_id INTEGER PRIMARY KEY AUTOINCREMENT,
  la_date_begin TEXT NOT NULL,
  la_date_end TEXT, la_status INT
);
CREATE INDEX iarchive_list ON an_archive_list(la_id);
CREATE INDEX iarchive_list_end ON an_archive_list(la_date_end); 

COMMIT;
