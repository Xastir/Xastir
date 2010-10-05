-- MYSQL SPATIAL (>4.1)
-- Updated by Dan Zubey N7NMD dzubey@openincident.com 03Oct2010
-- 
-- See the last four lines to set permissions for a user
-- to access this database.


CREATE DATABASE IF NOT EXISTS xastir;
  
USE xastir;

CREATE TABLE version (
     version_number INT,
     compatable_series INT
);

INSERT INTO version (version_number,compatable_series) VALUES (1,1);

-- MySQL spatial table using geometry POINT rather than lat/long
-- 

CREATE TABLE simpleStationSpatial (
     simpleStationId INT PRIMARY KEY NOT NULL AUTO_INCREMENT,
     station VARCHAR(9) NOT NULL, 
     symbol VARCHAR(1),   
     overlay VARCHAR(1),   
     aprstype VARCHAR(1),   
     transmit_time DATETIME,
     position POINT,
     origin VARCHAR(9) NOT NULL DEFAULT '',
     record_type VARCHAR(1),
     node_path VARCHAR(56)
);

-- Note: Use the asText() function to retrieve data from the position field.
-- In a shell, the return values from the raw position field will often 
-- change the character set in the shell, to avoid this, use asText(position).
-- Example query returning the position in WKT format: 
-- select station, asText(position), transmit_time from simpleStationSpatial; 


-- MYSQL (default table)

CREATE TABLE simpleStation (
     simpleStationId INT PRIMARY KEY NOT NULL AUTO_INCREMENT,
     station VARCHAR(9) NOT NULL, 
     symbol VARCHAR(1),   
     overlay VARCHAR(1),   
     aprstype VARCHAR(1),   
     transmit_time DATETIME,
     latitude FLOAT,
     longitude FLOAT,
     origin VARCHAR(9) NOT NULL DEFAULT '',
     record_type VARCHAR(1),
     node_path VARCHAR(56)
);


-- Example query to retieve symbol as aprsworld icon filename.
-- select count(*), concat(lpad(ascii(aprstype),3,'0'), '_', lpad(ascii(symbol),3,'0'), '.png') from simpleStationSpatial group by aprstype,symbol;

-- *** Permissions ***
-- Use the following grants to set up a user with minimal rights 
-- to the database.
-- 
-- Change the password on the next line and uncomment the next four lines. 
-- GRANT USAGE ON xastir.* to 'xastir_user'@'localhost' identified by 'set_this_password';
-- GRANT SELECT ON xastir.version TO 'xastir_user'@'localhost';
-- GRANT SELECT, INSERT, UPDATE ON xastir.simpleStation to 'xastir_user'@'localhost';
-- GRANT SELECT, INSERT, UPDATE ON xastir.simpleStationSpatial TO 'xastir_user'@'localhost';
