/******************************************************************
 *
 * @(#)$Id$
 *
 * Copyright (C) 2004 Bruce Bennett <bruts@adelphia.net>
 * Portions Copyright (C) 2004-2006 The Xastir Group
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements all of the database to APRS daemon.
 *
 *
    
        Davis/Data Base Weather --> APRS Weather

        Intended use: 
		
        Create & provide APRS style packet string
        without position information from MySQL database
        weather information stored there by meteo-0.9.4
        (See http://meteo.othello.ch for source) to
        xastir-1.2.1 (See http://www.xastir.org for source)

        Note:  "meteo-0.9.x" is a weather data accumulator
        aimed at Davis weather stations, which stores weather
        data in a mysql database.  It is configured in two 
        places, an XML file (default name meteo.xml) and in
        the database named in the XML file (default database 
        name is "meteo")		

        Output is to the ip hostname:port required in the 
        command line.

    ACKNOWLEGEMENTS:

        Elements of this software are taken from wx200d ver 1.2
        by Tim Witham <twitham@quiknet.com>, and it is modeled
        after that application. 

*******************************************************************/
#include <config.h>
#include <defs.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <getopt.h>
#include <mysql/mysql.h>

#define BUFLEN 32       /* max length of hostname:port */

#define POLL_INTERVAL	90		// default polling interval

#define VALID_WINDDIR   0x01
#define VALID_WINDSPD   0x02
#define VALID_WINDGST   0x04
#define VALID_TEMP      0x08
#define VALID_RAIN      0x10
#define VALID_HUMIDITY  0x40
#define VALID_AIRPRESS  0x80

#define OUTDOOR_SENSOR  1

#define MAX_WINDDIR	360
#define MAX_WINDSPD	180
#define MAX_TEMP	110
#define MIN_TEMP	-20
#define MAX_AIRPRESS	1050
#define MIN_AIRPRESS	750		
#define MIN_HUMIDITY	5


#define TEMPERATURE         0
#define HUMIDITY            10
#define AIR_PRESSURE        30
#define RAIN                50
#define WIND_SPEED          60
#define WIND_DIRECTION      61
#define WIND_GUST           62

struct dbinfo {
    char user[30];
    char pswrd[15];
    char name[30];
} db;
	
char *progname;
char *query;
int *current = 0;

int opt;

MYSQL       mysql;    // Yeah, globals...
MYSQL_RES   *result;
MYSQL_ROW   row;

char last_timestamp[20];
char last_datetime[20];

int debug_level;

char wxAPRShost[BUFLEN];
int wxAPRSport = PORT;
int outdoor_instr = OUTDOOR_SENSOR;

/******************************************************************
1/4/2003
            Usage brief

*******************************************************************/
void usage(int ret)
{
    if (query)
        printf("Content-type: text/plain\nStatus: 200\n\n");
	printf("usage: %s [options] \n",progname);
	printf("VERSION: %s\n",VERSION);
    printf("  -h    --help                      show this help and exit\n");
    printf("  -v    --verbose                   debugging info --> stderr\n");
    printf("  -c    --cport [port#]             IP port for data output\n");
    printf("  -s    --sensor [sensor group#]    from meteo, your OUTDOOR sensor set\n");
    printf("  -u    --user [database user]      username for mysql - default meteo\n");
    printf("  -p    --password [db passwd]      password for mysql - default none\n");
    printf("  -b    --database [database]       database name - default meteo\n");
    printf("  -n    --nodaemon                  do not run as daemon\n");
    printf("  -r    --repeat                    keep running\n");
    printf("  -i    --interval [seconds]        polling interval\n");
    printf("options may be uniquely abbreviated; units are as defined in APRS\n"); 
    printf("Specification 1.0.1 for positionless weather data (English/hPa).\n");
    exit(ret);
}





/******************************************************************
1/2/2003
            Make an APRS string out of WX data

*******************************************************************/

int APRS_str(char *APRS_buf, 
            char *datetime,
            double winddir,
            double windspeed,				
            double windgust,
            double temp,
            double rain1hr,
            double rain24hr,
	    double rainday,
            double humidity,
            double airpressure,
            unsigned int valid_data_flgs)
{

    int intval;
    char pbuf[10];

    if (APRS_buf == NULL) {
        if (debug_level & 1)
            fprintf(stderr,"err: Null string buffer for APRS string.\n");
        return -1;  //  Ooo!!  *****Nasty Bad Exit Point Here****

    }
//	timestamp first
    sprintf(APRS_buf, "_%s",datetime);
//
    if(valid_data_flgs & VALID_WINDDIR) {
        intval = winddir;
        sprintf(pbuf, "c%0.3d", intval);
    } 
    else {
        if (debug_level & 1) fprintf(stderr,"info: Wind direction flagged as invalid\n");
        sprintf(pbuf, "c...");
    }
    strcat(APRS_buf, pbuf);

    if(valid_data_flgs & VALID_WINDSPD) {
        intval = windspeed;
	sprintf(pbuf, "s%0.3d", intval);
    } 
    else {
        if (debug_level & 1) fprintf(stderr,"info: Wind speed flagged as invalid\n");
        sprintf(pbuf, "s...");
    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_WINDGST) {
        intval = (windgust + 0.5); // rounding to whole MPH
        if (intval > MAX_WINDSPD) { 
            if (debug_level & 1) fprintf(stderr,"err: Wind gust > %d\n", MAX_WINDSPD);
            sprintf(pbuf, "g...");
        } 
        else if (intval < 0) {
            if (debug_level & 1) fprintf(stderr,"err: Wind speed negative\n");
            sprintf(pbuf, "g...");
        } 
        else {
            sprintf(pbuf, "g%0.3d", intval);
        }
    } 
    else {
	if (debug_level & 1) fprintf(stderr,"info: Wind speed flagged as invalid\n");
        sprintf(pbuf, "g...");

    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_TEMP) {
        intval = temp;
        if (intval < 0) sprintf(pbuf,"t%0.2d",intval);
            else sprintf(pbuf, "t%0.3d", intval);
        strcat(APRS_buf,pbuf);
    } 
    else {
        if (debug_level & 1) fprintf(stderr,"info: Temperature flagged as invalid\n");
    }

    if(valid_data_flgs & VALID_RAIN) {
        intval = rain1hr;
	sprintf(pbuf,"r%0.3d",intval);
        strcat(APRS_buf,pbuf);
	intval = rain24hr;
	sprintf(pbuf,"p%0.3d",intval);
        strcat(APRS_buf,pbuf);
	intval = rainday;
	sprintf(pbuf,"P%0.3d",intval);
        strcat(APRS_buf,pbuf);
    } 
    else {
        if (debug_level & 1) fprintf(stderr,"info: no rain or rain flagged as invalid\n");
    }

    if(valid_data_flgs & VALID_HUMIDITY) {
        intval = humidity;
	sprintf(pbuf,"h%0.2d",intval);
        strcat(APRS_buf,pbuf);
    } 
    else {
        if (debug_level & 1) fprintf(stderr,"info: Humidity flagged as invalid\n");
    }

    if(valid_data_flgs & VALID_AIRPRESS) {
        intval = airpressure;
        sprintf(pbuf, "b%0.5d", intval);
        strcat(APRS_buf,pbuf);
	} 
        else {
        if (debug_level & 1) fprintf(stderr,"info: Air Pressure flagged as invalid\n");
    }
    strcat(APRS_buf,"xDvs\n");  // add X aprs and Davis WX station ID's and <lf>

    if (debug_level & 1) fprintf(stderr,"\ninfo: APRS Version of WX - %s\n\n",APRS_buf);
    return strlen(APRS_buf);
}





/******************************************************************
1/2/2003
            Get the latest set of Weather Data from the Data Base

*******************************************************************/

int Get_Latest_WX( double *winddir,
                double *windspeed,				
                double *windgust,
                double *temp,
                double *rain1hr,
                double *rain24hr,
		double *rainday,
                double *humidity,
                double *airpressure,
                unsigned int *valid_data_flgs)

{
    long last_hour_timestamp;
    long int last_24_timestamp;
    long int last_day_timestamp;
    char query_buffer[240];
    int nrows, row_cnt, item_count;

		
    // Find latest, see if it's new to us
    // --new to us is a simple timestamp follower, so upon startup
    // it will always read one set of data, assuming any exists

    if (mysql_query(&mysql, "SELECT MAX(timekey) from sdata")) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest timestamp query failed - exiting: %s\n", mysql_error(&mysql));
        return -1;
    }
	
    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest timestamp query failed - exiting: %s\n", mysql_error(&mysql));
        return -1;
    }
	
    if (mysql_num_rows(result) != 1 ) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest timestamp query failed - exiting: number of results %d\n",
                mysql_num_rows(result));
         // release query buffer
         mysql_free_result(result);
        return -1;
    }
	
    row = mysql_fetch_row(result);
	
    if( row[0] == NULL ) {
        if(debug_level & 1)
            fprintf(stderr,"err: NULL result for timestamp query\n");
        // release query buffer
        mysql_free_result(result);
        return -1;
    }
    // if no new data. exit with negative status
		
    if (!strncmp(last_timestamp, row[0], 11)) {
        if (debug_level & 1)
            fprintf(stderr,"info: No new weather data recorded - exiting: negative data\n");
        // release query buffer
        mysql_free_result(result);
        return 0;
    }
    strcpy(last_timestamp, row[0]);	  // For next pass & following query
        
    if( debug_level & 1) fprintf(stdout,"Timestamp: %s\n",last_timestamp);
	
    // release query buffer
    mysql_free_result(result);
    sprintf(query_buffer,"SELECT value,sensorid,fieldid,from_unixtime(timekey,'%%m%%d%%H%%i%%S') FROM sdata WHERE timekey = %s", last_timestamp);

    if (mysql_query(&mysql, query_buffer)) {
        if (debug_level & 1) fprintf(stderr,"err: Latest Weather Data query failed - exiting: %s\n\t --Query: %s\n", mysql_error(&mysql), query_buffer);
        return -1;
    }

    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1) fprintf(stderr,"err: Latest Weather Data query failed - exiting: %s\n", mysql_error(&mysql));
        return -1;
    }
    if ((nrows=mysql_num_rows(result)) < 1 ) {
        if (debug_level & 1) fprintf(stderr,"err: Latest Weather Data query failed - exiting: number of results %d\n",nrows);
        // release query buffer
        mysql_free_result(result);
        return -1;
    } 
    else {
        if (debug_level & 1) fprintf(stderr,"info: Latest Weather Data query: number of types of readings %d\n",nrows);
    }
	
    *valid_data_flgs = 0;
    item_count = 0;
    for (row_cnt = 0; row_cnt < nrows; row_cnt++) {
        row = mysql_fetch_row(result);
	strcpy(last_datetime,row[3]);
        if(atoi(row[1]) == outdoor_instr) {	 // sensors are really groups of data
            switch (atoi(row[2]))  {  // type of reading
                case WIND_DIRECTION :
                    *winddir = strtod(row[0],NULL);
                    if(debug_level & 1) fprintf(stderr,"wind direction %f\n ",*winddir);
		    if((*winddir < 0 ) || (*winddir > MAX_WINDDIR)) break;
                    *valid_data_flgs |= VALID_WINDDIR;
                    item_count++;
                    break;
                case WIND_SPEED : 
                    *windspeed = strtod(row[0],NULL);
                    if((*windspeed < 0) || (*windspeed > MAX_WINDSPD)) break;
                    if(debug_level & 1) fprintf(stderr,"wind speed %f\n ",*windspeed);
                    *valid_data_flgs |= VALID_WINDSPD;
                    item_count++;
                    break;
                case WIND_GUST :
                    *windgust = strtod(row[0],NULL); 
                    if((*windgust < 0) || (*windgust > MAX_WINDSPD)) break;
                    if(debug_level & 1) fprintf(stderr,"wind gust speed %f\n ",*windgust);
                    *valid_data_flgs |= VALID_WINDGST;
                    item_count++;
                    break;
                case TEMPERATURE :
                    *temp = strtod(row[0],NULL);
		    if((*temp < MIN_TEMP) || (*temp > MAX_TEMP)) break;
                    if(debug_level & 1) fprintf(stderr,"temperature %f\n ",*temp);
                    *valid_data_flgs |= VALID_TEMP;
                    item_count++;
                    break;
                case HUMIDITY :
                    *humidity = strtod(row[0],NULL);
		    if(*humidity < MIN_HUMIDITY) break;
                    if(debug_level & 1) fprintf(stderr,"humidity %f\n ",*humidity);
                    if(*humidity > 100) {
			*humidity = 0;
		    }
                    *valid_data_flgs |= VALID_HUMIDITY;
                    item_count++;
                    break;
                case AIR_PRESSURE :
                    *airpressure = strtod(row[0],NULL);
		    if((*airpressure < MIN_AIRPRESS) || (*airpressure > MAX_AIRPRESS)) break;
                    if(debug_level & 1) fprintf(stderr,"air pressure %f\n ",*airpressure);
		    *airpressure = *airpressure * 10;
                    *valid_data_flgs |= VALID_AIRPRESS;
                    item_count++;
                    break;
                default :
                    break;
            }
        } 
        else {	// must be indoor
            switch (atoi(row[2]))  {  // type of reading
                case AIR_PRESSURE :
                    *airpressure = strtod(row[0],NULL);
		    if((*airpressure < MIN_AIRPRESS) || (*airpressure > MAX_AIRPRESS)) break;
                    if(debug_level & 1) fprintf(stderr,"air pressure %f\n ",*airpressure);
		    *airpressure = *airpressure * 10;
                    *valid_data_flgs |= VALID_AIRPRESS;
                    item_count++;
                    break;
                 default :
                    break;
            }         
        }
    }

    if(debug_level & 1) fprintf(stderr,"loop ends\n");
    // release query buffer
    mysql_free_result(result);

/*	get rain figures	*/
/*
/*	hourly first		*/
    last_hour_timestamp = atol(last_timestamp) - 3600;
    sprintf(query_buffer,"SELECT round(sum(value),2) FROM sdata WHERE timekey > %ld and fieldid = 50", last_hour_timestamp);

    if (mysql_query(&mysql, query_buffer)) {
        if (debug_level & 1) fprintf(stderr,"err: rain 1 hour query failed - exiting: %s\n\t --Query: %s\n", mysql_error(&mysql), query_buffer);
        return -1;
    }
    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1) fprintf(stderr,"err: rain 1 hour store failed - exiting: %s\n", mysql_error(&mysql));
        return -1;
    }
    if ((mysql_num_rows(result)) > 0) {
        row = mysql_fetch_row(result);
        if (row[0] != NULL) {
	    *rain1hr = strtod(row[0],NULL);
            if(debug_level & 1) fprintf(stderr,"rain last hour %f\n ",*rain1hr);
	    *rain1hr = *rain1hr * 100;
            *valid_data_flgs |= VALID_RAIN;
	}
	else {
	    *rain1hr = 0;
            if(debug_level & 1) fprintf(stderr,"no rain recorded in last hour\n");
	}
    }     
    // release query buffer
    mysql_free_result(result);

/*	Last 24 hours		*/
    
    last_24_timestamp = atol(last_timestamp) - 86400;
    sprintf(query_buffer,"SELECT round(sum(value),2) FROM sdata WHERE timekey > %d and fieldid = 50", last_24_timestamp);

    if (mysql_query(&mysql, query_buffer)) {
        if (debug_level & 1) fprintf(stderr,"err: rain 24 hour query failed - exiting: %s\n\t --Query: %s\n", mysql_error(&mysql), query_buffer);
        return -1;
    }
    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1) fprintf(stderr,"err: rain 24 hour store failed - exiting: %s\n", mysql_error(&mysql));
        return -1;
    }
    if ((mysql_num_rows(result)) > 0) {
        row = mysql_fetch_row(result);
        if (row[0] != NULL) {
	    *rain24hr = strtod(row[0],NULL);
            if(debug_level & 1) fprintf(stderr,"rain last 24 hours %f\n ",*rain24hr);
	    *rain24hr = *rain24hr * 100;
            *valid_data_flgs |= VALID_RAIN;
	}
	else {
	    *rain24hr = 0;
            if(debug_level & 1) fprintf(stderr,"no rain recorded in last 24 hours\n");
	}
    }     
    // release query buffer
    mysql_free_result(result);

/*	since midnite		*/

/*	we can get the timestamp for midnite from the avg table		*/

last_day_timestamp = 0;
    sprintf(query_buffer,"SELECT max(timekey) FROM avg WHERE fieldid = 50 and intval = 86400", last_hour_timestamp);
    if (mysql_query(&mysql, query_buffer)) {
        if (debug_level & 1) fprintf(stderr,"err: midnite timekey query failed - exiting: %s\n\t --Query: %s\n", mysql_error(&mysql), query_buffer);
        return -1;
    }
    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1) fprintf(stderr,"err: midnite timekey store failed - exiting: %s\n", mysql_error(&mysql));
        return -1;
    }
    if ((mysql_num_rows(result)) > 0) {
        row = mysql_fetch_row(result);
        if (row[0] != NULL) {
	    last_day_timestamp = strtod(row[0],NULL);
	}
    }
    mysql_free_result(result);

    if (last_day_timestamp > 0) {
	last_day_timestamp += 115200;	// add 8 hours for offset - this should really be queried
    	sprintf(query_buffer,"SELECT round(sum(value),2) FROM sdata WHERE timekey > %d and fieldid = 50", last_day_timestamp);
    	if (mysql_query(&mysql, query_buffer)) {
       	    if (debug_level & 1) fprintf(stderr,"err: rain last day query failed - exiting: %s\n\t --Query: %s\n", mysql_error(&mysql), query_buffer);
       	    return -1;
        }
    	if (!(result = mysql_store_result(&mysql))) {
            if (debug_level & 1) fprintf(stderr,"err: rain last day store failed - exiting: %s\n", mysql_error(&mysql));
            return -1;
        }
        if ((mysql_num_rows(result)) > 0) {
            row = mysql_fetch_row(result);
            if (row[0] != NULL) {
	        *rainday = strtod(row[0],NULL);
                if(debug_level & 1) fprintf(stderr,"rain since midnite %f\n ",*rainday);
	    	*rainday = *rainday * 100;
                *valid_data_flgs |= VALID_RAIN;
	    }
	    else {
	        *rainday = 0;
                if(debug_level & 1) fprintf(stderr,"no rain recorded in last 24 hours\n");
	    }
        }     
    }
    // release query buffer
    mysql_free_result(result);
    
    if (debug_level & 1) fprintf(stderr,"info: success - Weather Data number of reading types %d\n",
             item_count);

    return item_count;
}				





/******************************************************************
1/5/2003
            SIGPIPE signal handler

*******************************************************************/
void pipe_handler(int sig)		/*  */
{
    signal(SIGPIPE, SIG_IGN);
    if (sig == SIGPIPE) {		// client went bye-bye
        shutdown(*current, 2); 
        close(*current);
        *current = -1;
        if (debug_level & 1)
            fprintf(stderr, "info: %s - TCP client timed out", progname); 
    }
}





/******************************************************************
1/5/2003
            SIGTERM signal handler

*******************************************************************/
void term_handler(int sig)		/*  */
{
    if (debug_level & 1)
        fprintf(stderr, "info: %s - ordered to DIE, complying", progname);

    // release query buffer & close connection
    mysql_free_result(result);

    mysql_close(&mysql);

    exit( 0 );
}





/******************************************************************
1/2/2003
        Coordinating MAIN point

*******************************************************************/
int main(int argc, char **argv)
{
    const char *flags = "Hhvnirc:u:p:d:s:";
    char WX_APRS[120];
    int data_len = 0 ;
    double winddir;
    double windspeed;				
    double windgust;
    double temp;
    double rain1hr;
    double rain24hr;
    double rainday;
    double humidity;
    double airpressure;
    unsigned int valid_data_flgs;
    int dsts = 0;
    int  pid, s, fd[CONNECTIONS];
    socklen_t clen = sizeof(struct sockaddr_in);
    int *max = 0,  c;
    int not_a_daemon = 0, repetitive = 0, tcp_wx_port = PORT;
    int  i, index = 0;
    struct sockaddr_in server, client;
    struct in_addr bind_address;
    fd_set rfds;
    struct timeval tv;
    int poll_interval = POLL_INTERVAL;
    int dly_cnt = 1;
    FILE *pidfile;
    const char *pidfilename = "/var/run/db2APRS.pid";
    char cmd[120];

    struct option longopt[] = {
        {"help", 0, 0, 'h'},
        {"refresh", 0, 0, 'r'},
        {"verbose", 0, 0, 'v'},
        {"user", 1, 0, 'u'},
        {"password", 1, 0, 'p'},
        {"database", 1, 0, 'd'},
        {"cport", 1, 0, 'c'},
        {"sensor",1, 0, 's'},
        {"nodaemon", 0, 0, 'n'},
        {"interval",1, 0, 'i'},
        {0, 0, 0, 0}
    };

    debug_level = 0;

    strcpy(db.user,"meteo");	 // set default values for database access
    strcpy(db.name,"meteo");
    memset(db.pswrd,0,15);

    mysql_init(&mysql);


    progname = strrchr(argv[0], '/');
    if (progname == NULL)
        progname = argv[0];
    else
        progname++;

    while ((opt = getopt_long(argc, argv, flags, longopt, &index)) != EOF) { 
        switch (opt) {		/* parse command-line or CGI options */
            case 'r':
                repetitive = 1;
                break;
            case 'v':
                fprintf(stdout,"Verbose mode set:\n");
                debug_level = 1;
                break;
            case 'u':	 // mysql username 
                strncpy(db.user,(char *)optarg,30);
                break;
            case 'p':	 // mysql password 
                strncpy(db.pswrd,(char *)optarg,15);
                break;
            case 'd':	 // mysql database name
                strncpy(db.name,(char *)optarg,30);
                break;
            case 'n':			/* do not fork and become a daemon */
                not_a_daemon = 1;
                break;
            case 'c':			/* port to use */
                tcp_wx_port = strtol(optarg, NULL, 0);
                break;
            case 's':                   /* sensor number for outdoor data */
                outdoor_instr  = atoi(optarg);
                break;
            case 'i':                   /* sensor number for outdoor data */
                poll_interval  = atoi(optarg);
                break;
 
            case '?':
            case 'h':
            case 'H':
                usage(0);
                break;
            default :
                usage(1);
        }
    }
    if (debug_level & 1) {
        fprintf(stdout,"Starting...");
        if (repetitive) fprintf(stdout, " forever ");
            else fprintf(stdout, " one pass only ");
        fprintf(stdout," with  database user=%s, password=%s, for database=%s\n",
            db.user, db.pswrd, db.name);
        if(not_a_daemon) fprintf(stdout," as a program ");
            else fprintf(stdout," as a daemon ");
        fprintf(stdout, " using TCP port %d\n",tcp_wx_port);
        fprintf(stdout, "an with an outdoor sensor group number of %d\n",outdoor_instr);
    }

        // Data base connection
	
	
    if (!(mysql_real_connect(&mysql, "localhost", db.user, db.pswrd, db.name, 0, 0, 0))) {
        if (debug_level & 1)
            fprintf(stderr,"err: Data Base connect for user:%s to database:%s failed - exiting: \n\t%s\n",
                db.user, db.name, mysql_error(&mysql));
        exit(9);
    }
	                                                                        
    server.sin_family = AF_INET;
    bind_address.s_addr = htonl(INADDR_ANY);
    server.sin_addr = bind_address;
    server.sin_port = htons(tcp_wx_port);

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        if (debug_level & 1)
            fprintf(stderr, "err: %s - no socket", progname);
        exit(10);
    }
    /* <dirkx@covalent.net> / April 2001 Minor change to allow quick
     * (re)start of deamon or client while there are pending
     * conncections during the quit. To avoid addresss/port in use
     * error.  */
    i = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == -1) {
    if (debug_level & 1)
        fprintf(stderr, "err: %s - setsockopt", progname);
    }
    if (bind(s, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        if (debug_level & 1)
            fprintf(stderr, "err: %s - cannot bind to socket", progname);
        exit(11);
    }
    if (listen(s, CONNECTIONS) == -1) {
        if (debug_level & 1)
            fprintf(stderr, "err: %s - listen", progname);
        exit(12);
    }

    if (debug_level & 1)
        fprintf(stdout,"Sockets UP.\n");

    umask(0022);
    for (i = 0; i < CONNECTIONS; i++) fd[i] = -1;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    if (!not_a_daemon) {			/* setup has worked; now become a daemon? */

        if ((pid = fork()) == -1) {
            syslog(LOG_ERR, "can't fork() to become daemon: %m"); 
        exit(20);
        } 
        else if (pid) {
            pidfile = fopen(pidfilename, "w");
	    fprintf(pidfile,"%d\n",pid);
	    fclose(pidfile);
            exit (0);
	}
        syslog(LOG_ERR, "Started\n"); 
        setsid();
        for (i = 0; i < NOFILE; i++) if ( i != s) close(i);
    }

    /* catch signals to close the database connection */
    signal( SIGTERM, term_handler );/* termination */
    signal( SIGPWR, term_handler ); /* power failure */

    if (debug_level & 1) fprintf(stdout,"Main Loop...\n");
    dly_cnt = 2;
    do {	
        if(!(dly_cnt--)) {	 
            dly_cnt = poll_interval;	// Every 'dly_cnt' passes check for WX data update
            dsts = Get_Latest_WX(&winddir,&windspeed,&windgust,
                &temp,&rain1hr,&rain24hr,&rainday,&humidity,&airpressure, &valid_data_flgs);
            if ( dsts <= 0 ) {
                if (debug_level & 1) fprintf(stderr, "err: Get_Latest returned %d\n",dsts);
		syslog(LOG_ERR,"No Data Found\n");
		exit(dsts);
	    }
            data_len = APRS_str(WX_APRS, last_datetime, winddir,windspeed,windgust,
                        temp, rain1hr, rain24hr, rainday, humidity, airpressure, valid_data_flgs);
            if (!data_len) {
                if (debug_level & 1) fprintf(stderr, "err: WX info formatting problem!");
		syslog(LOG_ERR,"WX Data format error\n");
                exit(13);
	    }
        }
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        if (select(s + 1, &rfds, NULL, NULL, &tv) > 0) {
            for (current = fd; (*current > 0) && (current < fd + CONNECTIONS - 1); current++);
            if (current > max) max = current;
            if ((*current = accept(s, (struct sockaddr *)&client, &clen)) != -1)
                write(*current, WX_APRS, data_len);
        }
        if (dly_cnt == poll_interval) {
            if (debug_level & 1) fprintf(stdout,"Updating clients:");
            for (current = fd; current <=max; current++)
            if (*current > 0) {   // active socket
                if (debug_level & 1) fprintf(stdout," #");
                signal(SIGPIPE, pipe_handler);
                write(*current, WX_APRS, data_len);
            if (debug_level & 1) fprintf(stdout," done\n");
            }
        }
    sleep(1);	
    } while (repetitive);

    mysql_close(&mysql);

    if (debug_level & 1) fprintf(stdout,"Exiting normally.\n");
    syslog(LOG_ERR,"Terminated normally\n");
    exit(0);
}

