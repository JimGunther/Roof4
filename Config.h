#ifndef CONFIG_H
#define CONFIG_H

/***********************************************************************************************
*config.h: header file for commonly used constants (numbers and strings)                       *
*                                                                                              *
* Version: 0.2                                                                                 *
* Last updated: 20/02/2025 10:02                                                               *
* Author: Jim Gunther 
*
* Change History
*
*	14/03/2025	0.2 - > 0.3	RDG		Added RED led & Updated RDG Home MQTT Server                                                                      *
*
***********************************************************************************************/

/*  Definition Section  */

// General Constants
// Wifi-related
#define NUM_NETWORKS 3
#define SSID_LEN 16
#define PWD_LEN 20
#define IP_LEN 20
#define SHED_SSID "BTB-NTCHT6"
#define JIM_SSID "BT-S7AT5Q"
#define RICH_SSID "GOULD_TP"
#define SHED_PWD "RHCagINtyI}`k>"
#define JIM_PWD "vHqbS8{:YqKX@q"
#define RICH_PWD "pr`rxgvx"
#define ON 1
#define OFF 0

// MQTT
#define SHED_IP "192.168.1.249"
#define JIM_IP "192.168.1.165" 
#define RICH_IP "192.168.1.178"
#define QT_LEN 108  //hdate + 10x(1+4+1+2): [,0000.00]
#define TOPIC_SETUP "ws/setup"
//#define TOPIC_IN "ws/shedRequests"
#define TOPIC_INIT "ws/init"
#define TOPIC_MESS "ws/messages"
#define TOPIC_CSV "ws/csv"
#define TOPIC_STAR "ws/wdstar"

// Other constants
#define POLL_COUNT 12 // number of loops between successive revs calculations 
#define NUM_ITEMS 10
#define INIT_WAIT 50
#define NUM_SHIFT7 32
#define CALM 33
#define INDEXER "RfWsGuWdTpHmPrL1L2Vo"

// Various character buffers' lengths
#define BUF_LEN 88
#define STARBUF_LEN 132
#define ICSV_LEN 12 // buffer size for item's CSV
#define INAME_LEN 3 // buffer size for Meteo name (2 chars + null)
#define HITEM_LEN 3 // string length for star data item
#define FITEM_LEN 8 // string length for "frequent" data item
#define ICBUF_LEN 10
//#define HRREQ_LEN 6 // length of hourly i/c request message: HxxDxx (hour and date requested)

#define NUL_WD 18  // code for wind direction NULL value
#define MIN_LIGHT 10

// Timings, etc
#define LOOP_TIME 250  // milliseconds
//#define MAX_LOOP_COUNT 120
#define ZONE4 4
#define ZONE12 12
#define ZONE40 40
#define ZONE0 120

// Pin numbers == GPIO numbers
const int RevsPin = 25;  // pin connected to wind speed rotation counter (interrupt 0) 
const int RainPin = 33; // pin connected to rain gauge - buckets tipped (interrupt 1);
const int WDPin = 32; // vane (wind direction)
const int VoltsPin = 34;  // raw analog value, not volts!
const int LEDPin = 19;		// Green LED
const int RedPin = 26;		// Red LED

#endif