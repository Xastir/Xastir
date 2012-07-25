-- MySQL dump 8.23
--
-- Host: localhost    Database: open2300
---------------------------------------------------------
-- Server version	3.23.58-log

--
-- Table structure for table `weather`
--

CREATE TABLE weather (
  timestamp bigint(14) NOT NULL default '0',
  rec_date date NOT NULL default '0000-00-00',
  rec_time time NOT NULL default '00:00:00',
  temp_in decimal(4,1) NOT NULL default '0.0',
  temp_out decimal(4,1) NOT NULL default '0.0',
  dewpoint decimal(4,1) NOT NULL default '0.0',
  rel_hum_in tinyint(3) NOT NULL default '0',
  rel_hum_out tinyint(3) NOT NULL default '0',
  windspeed decimal(4,1) NOT NULL default '0.0',
  wind_angle decimal(4,1) NOT NULL default '0.0',
  wind_direction char(3) NOT NULL default '',
  wind_chill decimal(4,1) NOT NULL default '0.0',
  rain_1h decimal(4,2) NOT NULL default '0.00',
  rain_24h decimal(4,2) NOT NULL default '0.00',
  rain_total decimal(5,2) NOT NULL default '0.00',
  rel_pressure decimal(4,2) NOT NULL default '0.00',
  tendency varchar(7) NOT NULL default '',
  forecast varchar(6) NOT NULL default '',
  UNIQUE KEY timestamp (timestamp)
) TYPE=MyISAM;

