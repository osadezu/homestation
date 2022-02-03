-- Setup database for homestation humidity and temperature logger

CREATE TABLE conditions (
  id SERIAL PRIMARY KEY,
  room VARCHAR NOT NULL,
  sensor VARCHAR NOT NULL,
  temperature REAL,
  humidity REAL,
  reading_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);