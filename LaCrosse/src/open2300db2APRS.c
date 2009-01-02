/******************************************************************
 *
 * Copyright (C) 2005 Jim Chandler <jim@n0vh.org>
 * Portions Copyright (C) 2004-2009 The Xastir Group
 *
 * Portions copied from Bruce Bennett's excellent Davis WX work
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements all of the database to APRS daemon.
 *
 *
    
        LaCrosse/Data Base Weather --> APRS Weather

        Intended use: 
		
        Create & provide APRS style packet string
        without position information from MySQL database
        weather information stored there by open2300
        (See http://open2300.sourceforge.net/ for source) to
        xastir-1.2.1 (See http://www.xastir.org for source)

        Note:  Open2300 is a weather data accumulator
        aimed at LaCrosse weather stations, which stores weather
        data in a mysql database.  

        Output is to the ip hostname:port required in the 
        command line.

    ACKNOWLEGEMENTS:

        Elements of this software are taken from wx200d ver 1.2
        by Tim Witham <twitham@quiknet.com>, and it is modeled
        after that application and from db2APRS by 
	Bruce Bennett <bruts@adelphia.net>. 

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
#include <mysql.h>

#define MAXARGS 20      /* maximum CGI args to parse */
#define TMPLEN 128      /* max length of CGI */
#define BUFLEN 32       /* max length of hostname:port */

#define VALID_WINDDIR   0x01
#define VALID_WINDSPD   0x02
#define VALID_WINDGST   0x04
#define VALID_TEMP      0x08
#define VALID_RAIN1HR   0x10
#define VALID_RAIN24H   0x20
#define VALID_HUMIDITY  0x40
#define VALID_AIRPRESS  0x80

#define MTPS2MPH        2.2369
#define DEGC2DEGF       1.8
#define MM2IN100TH      3.937
#define INHG2HPA10TH    338.638

#define OUTDOOR_SENSOR  1

    //---From the static table "mfield", which really should be dynamically read here---
    //			(but then I couldn't use a switch statement)

#define TEMPERATURE         0
#define TEMPERATURE_MIN     1
#define TEMPERATURE_MAX     2
#define HUMIDITY            10
#define HUMIDITY_MIN        11
#define HUMIDITY_MAX        12
#define WETNESS             20
#define WETNESS_MIN         21
#define WETNESS_MAX         22
#define AIR_PRESSURE        30
#define AIR_PRESSURE_MIN    31
#define AIR_PRESSURE_MAX    32
#define SOLAR               40
#define UV                  41
#define RAIN                50
    // note: "51" is really rain total
#define RAIN_PER_DAY        51
#define RAIN_PER_HOUR       52
#define WIND_SPEED          60
#define WIND_DIRECTION      61
#define WIND_GUST           62
#define WIND_X              63
#define WIND_Y              64
#define MOISTURE            70
#define WATERLEVEL          71
#define WATERLEVEL_MIN      72
#define WATERLEVEL_MAX      73
#define BATTERY             110
#define TRANSMITTER         111
#define DURATION            120
#define SAMPLES             121

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
    printf("  -u    --user [database user]      username for mysql - default meteo\n");
    printf("  -p    --password [db passwd]      password for mysql - default none\n");
    printf("  -b    --database [database]       database name - default meteo\n");
    printf("  -n    --nodaemon                  do not run as daemon\n");
    printf("  -r    --repeat                    keep running\n");
    printf("options may be uniquely abbreviated; units are as defined in APRS\n"); 
    printf("Specification 1.0.1 for positionless weather data (English/hPa).\n");
    exit(ret);
}





/******************************************************************
1/2/2003
            Make an APRS string out of WX data

*******************************************************************/

int APRS_str(char *APRS_buf, 
            double winddir,
            double windspeed,				
            double windgust,
            double temp,
            double rain1hr,
            double rain24h,
            double humidity,
            double airpressure,
            unsigned int valid_data_flgs,
            int Metric_Data)
{

    int intval;
    char pbuf[10];

    if (APRS_buf == NULL) {
        if (debug_level & 1)
            fprintf(stderr,"err: Null string buffer for APRS string.\n");
        return 0;  //  Ooo!!  *****Nasty Bad Exit Point Here****

    }
    if(valid_data_flgs & VALID_WINDDIR) {
        intval = (winddir + 0.5); // rounding to whole degrees
        if (intval > 360) {
            if (debug_level & 1)
                fprintf(stderr,"err: Wind direction > 360\n");
            sprintf(APRS_buf, "c...");
        } 
        else if (intval < 0) {
            if (debug_level & 1)
                fprintf(stderr,"err: Wind direction negative\n");
            sprintf(APRS_buf, "c...");

        } 
        else {
            sprintf(APRS_buf, "c%0.3d", intval);
        }
    } 
    else {
        if (debug_level & 1)
            fprintf(stderr,"info: Wind direction flagged as invalid\n");
        sprintf(APRS_buf, "c...");

    }

    if(valid_data_flgs & VALID_WINDSPD) {
        if (Metric_Data)
            intval = (windspeed*MTPS2MPH + 0.5); // converting & rounding to whole MPH 
        else
            intval = (windspeed + 0.5); // rounding to whole MPH
        if (intval > 600) { // Let's be reasonable here - center of a tornado??
            if (debug_level & 1)
                fprintf(stderr,"err: Wind speed > 600 MPH\n");
            sprintf(pbuf, "s...");

        } 
        else if (intval < 0) {
            if (debug_level & 1)
                fprintf(stderr,"err: Wind speed negative\n");
            sprintf(pbuf, "s...");

		} 
        else {
				sprintf(pbuf, "s%0.3d", intval);
        }
    } 
    else {
        if (debug_level & 1)
            fprintf(stderr,"info: Wind speed flagged as invalid\n");
        sprintf(pbuf, "s...");

    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_WINDGST) {
        if (Metric_Data)
            intval = (windgust*MTPS2MPH + 0.5); // converting & rounding to whole MPH
        else
            intval = (windgust + 0.5); // rounding to whole MPH
        if (intval > 600) { // Let's be reasonable here - center of a tornado??
            if (debug_level & 1)
                fprintf(stderr,"err: Wind gust > 600 MPH\n");
            sprintf(pbuf, "g...");

        } 
        else if (intval < 0) {
            if (debug_level & 1)
                fprintf(stderr,"err: Wind speed negative\n");
            sprintf(pbuf, "g...");

        } 
        else {
            sprintf(pbuf, "g%0.3d", intval);
        }

    } 
    else {
	    if (debug_level & 1)
            fprintf(stderr,"info: Wind gust flagged as invalid\n");
        sprintf(pbuf, "g...");

    }
	strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_TEMP) {
        if (Metric_Data)
            intval = ((temp)*DEGC2DEGF + 0.5)+32; // converting & rounding to whole Deg F
        else
            intval = (temp + 0.5); // rounding to whole Deg F
        if (intval > 200) { // Let's be reasonable here - boiling?
            if (debug_level & 1)
                fprintf(stderr,"err: Temperature > 200 Deg F\n");
            sprintf(pbuf, "t...");

        } 
        else if (intval < -99) {
            if (debug_level & 1)
                fprintf(stderr,"err: Temperature < -99 Deg F\n");
            sprintf(pbuf, "t...");

        } 
        else {
            if (intval < 0) sprintf(pbuf,"t%0.2d",intval);
            else sprintf(pbuf, "t%0.3d", intval);
        }

    } 
    else {
        if (debug_level & 1)
            fprintf(stderr,"info: Temperature flagged as invalid\n");
        sprintf(pbuf, "t...");

    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_RAIN1HR) {
        if (Metric_Data)
            intval = ((rain1hr)*MM2IN100TH + 0.5); // converting & rounding to whole 1/100 inch
        else
            intval = (rain1hr*100.0 + 0.5); // rounding to whole 1/100 inch
        if (intval > 999) { // 10 in/hr? Garden Hose -> rain gauge?
            if (debug_level & 1)
                fprintf(stderr,"err: Rainfall/Hr > 9.99 inch\n");
            sprintf(pbuf, "\0\0\0\0");

        } 
        else if (intval < -99) {
            if (debug_level & 1)
                fprintf(stderr,"err: Rainfall/Hr negative\n");
            sprintf(pbuf, "\0\0\0\0");

        } 
        else {
            sprintf(pbuf, "r%0.3d", intval);
        }
    } 
    else {
	
        if (debug_level & 1)
            fprintf(stderr,"info: Rainfall/Hr flagged as invalid\n");
        sprintf(pbuf, "\0\0\0\0");

    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_RAIN24H) {
        if (Metric_Data)
            intval = ((rain24h)*MM2IN100TH + 0.5); // converting & rounding to whole 1/100 inch
        else
            intval = (rain24h*100.0 + 0.5); // rounding to whole 1/100 inch
        if (intval > 999) { // Can't handle greater than 9.99 inches of rain in 24 hours
            if (debug_level & 1)
                fprintf(stderr,"err: Rainfall/24Hr > 9.99 inch - reporting 9.99 inches\n");
            sprintf(pbuf, "p999");

        } 
        else if (intval < -99) {
            if (debug_level & 1)
                fprintf(stderr,"err: Rainfall/Hr negative\n");
            sprintf(pbuf, "\0\0\0\0");

        } 
        else {
            sprintf(pbuf, "p%0.3d", intval);
        }
    } 
    else {
        if (debug_level & 1)
            fprintf(stderr,"info: Rainfall/24Hr flagged as invalid\n");
        sprintf(pbuf, "\0\0\0\0");

    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_HUMIDITY) {
        intval = (humidity + 0.5); // rounding to whole percent
        if (intval > 100) { // Unlike the space shuttle engines, 100 % is max
            if (debug_level & 1)
                fprintf(stderr,"err: Humidity reported > 100%\n");
            sprintf(pbuf, "\0\0\0\0");

		} 
        else if (intval < 1) {
            if (debug_level & 1)
                fprintf(stderr,"err: Humidity reported < 1%\n");
            sprintf(pbuf, "\0\0\0\0");

        } 
        else {
            if (intval == 100) intval = 0;  // Report 100% as 'h00'
            sprintf(pbuf, "h%0.2d", intval);
        }
    } 
    else {
        if (debug_level & 1)
            fprintf(stderr,"info: Humidity flagged as invalid\n");
        sprintf(pbuf, "\0\0\0\0");

    }
    strcat(APRS_buf,pbuf);

    if(valid_data_flgs & VALID_AIRPRESS) {
        if (Metric_Data)
            intval = (airpressure*10.0 + 0.5); //  rounding to whole tenth of a hPa
        else
            intval = (airpressure*INHG2HPA10TH + 0.5); // convering In-Hg to 1/10 hPa and rounding
        if (intval > 20000) { //two atmospheres - about 29 PSIA
            if (debug_level & 1)
                fprintf(stderr,"err: Air Pressure reported > 2 Atmospheres%\n");
                sprintf(pbuf, "\0\0\0\0");

        } 
        else if (intval < 0) {
            if (debug_level & 1)
                fprintf(stderr,"err: Air Pressure reported negative%\n");
            sprintf(pbuf, "\0\0\0\0");

        } 
        else {
            sprintf(pbuf, "b%0.5d", intval);
		}
	} else {
        if (debug_level & 1)
            fprintf(stderr,"info: Air Pressure flagged as invalid\n");
        sprintf(pbuf, "\0\0\0\0");

    }
    strcat(APRS_buf,pbuf);
    strcat(APRS_buf,"xDvs\n");  // add X aprs and LaCrosse WX station ID's and <lf>

    if (debug_level & 1)
        fprintf(stderr,"\ninfo: APRS Version of WX - %s\n\n",APRS_buf);
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
                double *rain24h,
                double *humidity,
                double *airpressure,
                unsigned int *valid_data_flgs,
                int *Metric_Data)

{

    char latest_timestamp[20];
    char query_buffer[160];
    int nrows, row_cnt, item_count;

		
    // Find latest, see if it's new to us
    // --new to us is a simple timestamp follower, so upon startup
    // it will always read one set of data, assuming any exists

    if (mysql_query(&mysql, "SELECT MAX(timestamp) from weather")) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest timestamp query failed - exiting: %s\n", mysql_error(&mysql));
        return 0;
    }
	
    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest timestamp query failed - exiting: %s\n", mysql_error(&mysql));
        return 0;
    }
	
    if (mysql_num_rows(result) != 1 ) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest timestamp query failed - exiting: number of results %d\n",
                mysql_num_rows(result));
         // release query buffer
         mysql_free_result(result);
        return 0;
    }
	
    row = mysql_fetch_row(result);
	
    if( row[0] == NULL ) {
        if(debug_level & 1)
            fprintf(stderr,"err: NULL result for timestamp query\n");
        // release query buffer
        mysql_free_result(result);
        return 0;
    }
    // if no new data. exit with negative status
		
    if (!strncmp(last_timestamp, row[0], 14)) {
        if (debug_level & 1)
            fprintf(stderr,"info: No new weather data recorded - exiting: negative data\n");
        // release query buffer
        mysql_free_result(result);
        return -1;
    }
    strcpy(last_timestamp, row[0]);	  // For next pass & following query

    if( debug_level & 1) fprintf(stdout,"Timestamp: %s\n",last_timestamp);
	
    // release query buffer
    mysql_free_result(result);

    sprintf(query_buffer,"SELECT wind_angle, windspeed, temp_out, rain_1h, rain_24h, rel_hum_out, rel_pressure FROM weather WHERE timestamp = %s", last_timestamp);

    if (mysql_query(&mysql, query_buffer)) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest Weather Data query failed - exiting: %s\n\t --Query: %s\n",
                mysql_error(&mysql), query_buffer);
        return 0;
    }

    if (!(result = mysql_store_result(&mysql))) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest Weather Data query failed - exiting: %s\n", mysql_error(&mysql));
        return 0;
    }
    if ((nrows=mysql_num_rows(result)) < 1 ) {
        if (debug_level & 1)
            fprintf(stderr,"err: Latest Weather Data query failed - exiting: number of results %d\n",nrows);
        // release query buffer
        mysql_free_result(result);
        return 0;
    }
    else {
        if (debug_level & 1)
            fprintf(stderr,"info: Latest Weather Data query: number of types of readings %d\n",nrows);
    }

    *valid_data_flgs = 0;
    item_count = 0;

    //WIND_DIRECTION :
    *winddir = strtod(row[0],NULL);
    *valid_data_flgs |= VALID_WINDDIR;  
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"wind direction %f\n ",*winddir);
    //case WIND_SPEED :
    *windspeed = strtod(row[1],NULL);
    *windspeed = *windspeed * 1.15077945; // Convert from knots to mph
    *valid_data_flgs |= VALID_WINDSPD;  
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"wind speed %f\n ",*windspeed);
    //case WIND_GUST :
    *windgust = strtod("0",NULL);
    //*valid_data_flgs |= VALID_WINDGST;  No gust information from open2300
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"wind gust speed %f\n ",*windgust);
    //case TEMPERATURE :
    *temp = strtod(row[2],NULL);
    *valid_data_flgs |= VALID_TEMP;
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"temperature %f\n ",*temp);
    //case RAIN_PER_HOUR :
    *rain1hr = strtod(row[3],NULL);
    *valid_data_flgs |= VALID_RAIN1HR;  
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"rainfall for 1 hr %f\n ",*rain1hr);
    //case RAIN_PER_DAY :
    *rain24h = strtod(row[4],NULL);
    *valid_data_flgs |= VALID_RAIN24H;  
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"rainfall for 24 hrs %f\n ",*rain24h);
    //case HUMIDITY :
    *humidity = strtod(row[5],NULL);
    *valid_data_flgs |= VALID_HUMIDITY;
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"humidity %f\n ",*humidity);
    //case AIR_PRESSURE :
    *airpressure = strtod(row[6],NULL);
    *valid_data_flgs |= VALID_AIRPRESS;
    item_count++;
    if(debug_level & 1)
         fprintf(stderr,"air pressure %f\n ",*airpressure);

    *Metric_Data = 0;  // My station reports F, knots and inHG
    // release query buffer	& close connection
    mysql_free_result(result);

    if (debug_level & 1)
        fprintf(stderr,"info: success - Weather Data number of reading types %d\n",
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

    // release query buffer	& close connection
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
    const char *flags = "Hhvnc:u:p:d:s:r";
    char WX_APRS[120];
    int data_len = 0 ;
    double winddir;
    double windspeed;				
    double windgust;
    double temp;
    double rain1hr;
    double rain24h;
    double humidity;
    double airpressure;
    unsigned int valid_data_flgs;
    int Metric_Data = 0, dsts = 0;
    int  pid, s;
    socklen_t clen = sizeof(struct sockaddr_in);
    int fd[CONNECTIONS];
    int *max = 0,  c, dly_cnt = 1;
    int not_a_daemon = 0, repetitive = 0, tcp_wx_port = PORT;
    int  i, index = 0;
    struct sockaddr_in server, client;
    struct in_addr bind_address;
    fd_set rfds;
    struct timeval tv;

    struct option longopt[] = {
        {"help", 0, 0, 'h'},
        {"refresh", 0, 0, 'r'},
        {"verbose", 0, 0, 'v'},
        {"user", 1, 0, 'u'},
        {"password", 1, 0, 'p'},
        {"database", 1, 0, 'd'},
        {"cport", 1, 0, 'c'},
        {"nodaemon", 0, 0, 'n'},
        {0, 0, 0, 0}
    };

    debug_level = 0;

    strcpy(db.user,"open2300");	 // set default values for database access
    strcpy(db.name,"open2300");
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
    }

        // Data base connection
	
	
    if (!(mysql_real_connect(&mysql, "localhost", db.user, db.pswrd, db.name, 0, NULL, 0))) {
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
        else if (pid)
            exit (0);

        setsid();
        for (i = 0; i < NOFILE; i++) if ( i != s) close(i);
    }

    /* catch signals to close the database connection */
    signal( SIGTERM, term_handler );/* termination */
#if defined(SIGPWR) /* SIGPWR is linux centric */
    signal( SIGPWR, term_handler ); /* power failure */
#endif

    if (debug_level & 1) fprintf(stdout,"Main Loop...\n");
    dly_cnt = 1;  //N0VH, change back to 1
    do {	
        if(!(dly_cnt--)) {	 
            dly_cnt = 25;		// Every 'dly_cnt' passes check for WX data update
            if((dsts = Get_Latest_WX(&winddir,&windspeed,&windgust,
                &temp,&rain1hr,&rain24h,&humidity,&airpressure,
                &valid_data_flgs,&Metric_Data)) !=0 ) {
                if ( dsts > 0 ) {
                    data_len = APRS_str(WX_APRS, winddir,windspeed,windgust,
                        temp, rain1hr, rain24h, humidity, airpressure,
                        valid_data_flgs, Metric_Data);

                    if (!data_len) {
                        if (debug_level & 1)
                            fprintf(stderr, "err: WX info formatting problem!");                    
                        exit(13);
                    }
                }
            } 
            else {
                exit(dsts);
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
        if (dly_cnt == 25) {
            if (debug_level & 1) fprintf(stdout,"Updating clients:");
            for (current = fd; current <=max; current++)
            if (*current > 0) {   // active socket
                if (debug_level & 1) fprintf(stdout," #");
                signal(SIGPIPE, pipe_handler);
                write(*current, WX_APRS, data_len);
        }
        if (debug_level & 1) fprintf(stdout," done\n");
    }
    sleep(1);	 // 	   
    } while (repetitive);

    mysql_close(&mysql);

    if (debug_level & 1) fprintf(stdout,"Exiting normally.\n");
    exit(0);
}

