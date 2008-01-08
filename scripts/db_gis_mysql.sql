-- MYSQL SPATIAL (>4.1)

create database xastir;
-- set the password and uncomment
--grant select on xastir to user xastir_user@localhost identified by password '<password>';
  
use xastir;

create table version (
     version_number int,
     compatable_series int
);
grant select on version to xastir_user@localhost

insert into version (version_number,compatable_series) values (1,1);

-- MySQL spatial table using geometry POINT rather than lat/long
-- 

create table simpleStationSpatial (
     simpleStationId int primary key not null auto_increment
     station varchar(9) not null, 
     symbol varchar(1),   
     overlay varchar(1),   
     aprstype varchar(1),   
     transmit_time datetime not null default now(),
     position POINT  
     origin varchar(9) not null default '',
     record_type varchar(1),
     node_path varchar(56)
);

grant select, insert, update on simpleStationSpatial to xastir_user@localhost;

-- MYSQL (default table)

create table simpleStation (
     simpleStationId int primary key not null auto_increment
     station varchar(9) not null, 
     symbol varchar(1),   
     overlay varchar(1),   
     aprstype varchar(1),   
     transmit_time datetime not null default now(),
     latitude float,
     longitude float,
     origin varchar(9) not null default '',
     record_type varchar(1),
     node_path varchar(56)
);

grant select, insert, update on simpleStation to xastir_user@localhost;

