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
  mo_result TEXT NOT NULL,
  FOREIGN KEY(de_id) REFERENCES an_device(de_id),
  FOREIGN KEY(sw_id) REFERENCES an_switch(sw_id),
  FOREIGN KEY(se_id) REFERENCES an_sensor(se_id)
);
CREATE INDEX imonitor ON an_monitor(mo_id);
CREATE INDEX imonitordate ON an_monitor(mo_date);

COMMIT;
