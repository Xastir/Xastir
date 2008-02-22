-- POSTGRES/POSTGIS

create database xastir;

--set the password and uncomment
--create user xastir_user with encrypted password '<password>';

-- edit pg_hba.conf to allow access from local host
--local xastir xastir_user md5
--host xastir xastir_user 127.0.0.1/32 md5

-- run create lang to add plpgsql to xastir db
--createlang --dbname=xastir plpgsql
-- run lwpostgis script to enable postgis for xastir db
--psql -d xastir -f lwpostgis.sql

--run the following sql commands in psql
--psql xastir

create table version (
     version_number int,
     compatable_series int
);
grant select on version to xastir_user;

insert into version (version_number,compatable_series) values (1,1);

create table simpleStation (
     simpleStationId serial primary key,
     station varchar(9) not null, 
     symbol varchar(1),   
     overlay varchar(1),    
     aprstype varchar(1),   
     transmit_time timestamptz not null default now(),  
     origin varchar(9) not null default '',
     record_type varchar(1),
     node_path varchar(56),
);
create index ssstation on simplestation(station);
create index sssymbol on simplestation(symbol);
create index sstype on simplestation(aprstype);
create index sstransmittime on simplestation(transmit_time);

create index ssstationtime on simplestation(station,transmit_time);

--select AddGeometryColumn('','simpleStation','position',-1,'POINT',2);

--alternately, set geometry explicitly as WGS84 Latitude/Longitude: 
insert into spatial_ref_sys (srid,auth_name,auth_srid,srtext,proj4text) values (1,'NAD83',4269,'+proj=longlat +ellps=GRS80 +datum=NAD83 +no_defs ','+proj=longlat +ellps=GRS80 +datum=NAD83 +no_defs ');

--- EPSG 4326 : WGS 84 Lat/Long
INSERT INTO spatial_ref_sys (srid,auth_name,auth_srid,srtext,proj4text) VALUES (4326,'EPSG',4326,'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]','+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ');

select AddGeometryColumn('','simpleStation','position',4326,'POINT',2);


grant select, insert, update on simpleStation to xastir_user;
grant select, update on simpleStation_simpleStationId_seq to xastir_user;
-- the next two grants allow xastir_user to be used in other applications
-- such as qgis that need access to the spatial metadata tables
grant select on geometry_columns to xastir_user;
grant select on spatial_ref_sys to xastir_user;

-- 0 update
alter table simpleStation add column origin varchar(9) not null default '';
alter table simpleStation add column record_type varchar(1);
alter table simpleStation add column node_path varchar(56);
-- note - lat/long is transposed in version 0 and version 1

-- example query to list symbols by aprsworld icon filenames
-- select count(*), lpad(ascii(aprstype),3,'0') || '_' || lpad(ascii(symbol),3,'0') || '.png' from simpleStation group by aprstype, symbol;

-- view to add icon filenames
create view simplestationicons as select *, lpad(ascii(aprstype),3,'0') || '_' || lpad(ascii(symbol),3,'0') || '.png' as icon from simpleStation;

