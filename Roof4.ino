/*************************************************************************************
* Roof4.ino: MiS Roof ESP32 Code (Weather Station) with NTP                          *
*                                                                                    *
* Version: 0.1                                                                       *
* Last updated: 14/04/2025 19:52                                                     *
* Author: Jim Gunther                                                                *
*                                                                                    *
*                                                                                    *
**************************************************************************************/
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClient.h> //OTA
#include <WebServer.h>  //OTA
#include <ESPmDNS.h>    //OTA
#include <Update.h>     //OTA
#include <PubSubClient.h>

/*
library WiFi at version 3.1.3 
library NTPClient at version 3.2.1
library Networking at version 3.1.3 
library WebServer at version 3.1.3 
library FS at version 3.1.3 
library ESPmDNS at version 3.1.3 
library Update at version 3.1.3 
library PubSubClient at version 2.8 
library Wire at version 3.1.3 
library Adafruit BMP085 Unified at version 1.1.3 
library Adafruit Unified Sensor at version 1.1.15 
library Adafruit AHTX0 at version 2.0.5
library Adafruit BusIO at version 1.17.0 
library SPI at version 3.1.3 
library BH1750 at version 1.3.0
 */
 
// Other includes
#include "Config.h"
#include "Komms.h"
#include "WInputs.h"

// Class instantiation
WebServer server(80); // OTA
Komms kom;
WInputs wi;

const char* host = "esp32"; // OTA

//===================================================================================================================
// MQTT stuff
WiFiClient espClient;
PubSubClient qtClient(espClient);
int nwkIx;
bool bNewMQTT = false;
unsigned int icLength;
byte bqticBuf[QT_LEN];
char qticBuf[QT_LEN];
char qtogBuf[QT_LEN];
char mqttServer[IP_LEN];
char versionBuf[52];

// Array and variables initiation
unsigned long loopStart;
int loopCount = 0;
int rptIntvl = 120; // default value
int maxGust;
int prevDay = 10;  // any invalid day of week will do

/******************************************************************************************************/

void qtCallback(char* topic, byte* message, unsigned int length) {
  for (int i = 0; i < length; i++) {
    qticBuf[i] = (char)message[i];
  }
  bNewMQTT = true;
}

/*********************************************************************************************************************
qtSetup(): sets up MQTT connection
parameters: nwkIx: int: index no. of wifi network (0-2) Shed, Jim, Richard
returns: boolean True for success, False for failure
**********************************************************************************************************************/
bool qtSetup(int nwkIx) {
  switch (nwkIx) {
    case 0:
      strcpy(mqttServer, SHED_IP);
      break;
    case 1:
      strcpy(mqttServer, JIM_IP);
      break;
    case 2:
      strcpy(mqttServer, RICH_IP);
      break;
  }

  qtClient.setServer(mqttServer, 1883);
  qtClient.setCallback(qtCallback);
  return qtReconnect();
}

/*********************************************************************************************************************
qtReconnect(): does the actual connection to MQTT broker (3 attempts)
parameters: none
returns: boolean: True for success, False for failure
**********************************************************************************************************************/
bool qtReconnect() {

  // Loop until we're reconnected
  int numTries = 0;

  while (!qtClient.connected() && (numTries++ < 3)) {
    fn_RedLed( ON );
	Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (qtClient.connect("misRoof")) {
      Serial.println("connected");
      qtClient.loop();
	  fn_RedLed( OFF );
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(qtClient.state());
      Serial.println(" try again in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }

  }

  return qtClient.connected();
}
/********************************************************************************************************************
publishMQTT(): post CSV data, including initial character and CSV-style requests to Shed
parameters:
  init: bool: if initial request for report interval value
  star: bool: true if WD star CSV, otherwise false
  csv: string containing CSV data: NB MUST start with comma and no final comma
returns: void
*********************************************************************************************************************/
void publishMQTT(bool init, bool star, const char* csv) {
  char buf[STARBUF_LEN];  // STARBUF_LEN is currently > BUF_LEN, so use bigger buffer
  int len = strlen(csv);
  buf[0] = '$';
  strcpy(buf + 1, csv);
  if (init){
    qtClient.publish(TOPIC_INIT, buf, false);
    return;
  }
  if (star) qtClient.publish(TOPIC_STAR, buf, false);
  else qtClient.publish(TOPIC_CSV, buf, false); 
}

/********************************************************************************************************************
postMessage(): post message verbatim, just adding 'M' to the front
parameters:
  txt: string containing message
returns: void
*********************************************************************************************************************/
void postMessage(const char* txt) {
  char buf[BUF_LEN];
  buf[0] = '$';
  strcpy(buf + 1, txt);
  qtClient.publish(TOPIC_MESS, buf, false); 
}
// BEGIN NTP ===================================================================================================================

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// END NTP =====================================================================================================================

// BEGIN OTA ===================================================================================================================

/*
 * Login page
 */
const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='MIS' && form.pwd.value=='mispas')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

// END OTA =====================================================================================================================

// -----------------------------------------------------------------------------------------------------------------------------
void setup() {
  //Setup code here, to run once:

  Serial.begin(115200);
  pinMode(LEDPin, OUTPUT);
  pinMode(RedPin, OUTPUT);

  fn_RedLed( ON );

  kom.begin();
  wi.begin();
   
  tryMQTTStart();

  // OTA START ================================================================================================
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  // OTA END ==================================================================================================
  timeClient.begin();
  timeClient.setTimeOffset(0);  // UTC

  qtClient.subscribe(TOPIC_SETUP, 1);
  loopCount = 0;
  requestRptInterval();

  qtClient.unsubscribe(TOPIC_SETUP);
  //qtClient.subscribe(TOPIC_IN, 1);
  logStartupSuccess();
  fn_RedLed( OFF );
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void loop() {
  int flag = 0;
  unsigned long m1, z1 = 0ul;
  server.handleClient();  // OTA
  loopStart = millis();
  qtClient.loop(); // keep MQTT going
  
  // Loop timing zones start here
  
  // ZONE 1: EVERY LOOP (1/4 sec) ----------------------------------------------------------------- 
  wi.updateMaxGust(); // 4 times/sec to catch gusts
  flag = wi.updateMeteo(loopCount, rptIntvl);  // updateMeteo uses loopCount to decide when to update each Meteo
  // END ZONE 1 -----------------------------------------------------------------------------------
  
  //ZONE 4: EVERY 4 LOOPS (1 sec) ---------------------------------------------------------
  if ((loopCount % ZONE4) == 0) {
    wi.WDChanged(false); // updates the set of 32 values used to produce WD stats
    flag = flag | 256;
    //shedRequested();  // responds to any shed messages received (hourly)
  }
  // END ZONE 4 -----------------------------------------------------------------------------------
  
  // ZONE 12: EVERY 12 LOOPS (3 secs) -------------------------------------------------------------
  if ((loopCount % ZONE12) == 0) {
    if (!qtReconnect()) { // checks connection and attempts retry if none
      postMessage("MQTT failed");
    }
    blink();  // "normal" 3 second blink
    flag = flag | 512;
  }
  // END ZONE 12 ----------------------------------------------------------------------------------
  
  // ZONE SLOWEST: EVERY rptInterval LOOPS (DEFAULT 30 secs)
    if (loopCount == 0) {
      m1 = millis();
      if (timeClient.update()) {
        int ntpH = timeClient.getHours();
        int ntpM = timeClient.getMinutes();
        int ntpS = timeClient.getSeconds();
        int ntpD = timeClient.getDay();
        flag = flag | 1024;
        if ((ntpD != prevDay) && (ntpH == 1) && (ntpM == 0) && (ntpS >= 30)) {
          // it's between 01:00:30 and 01:00:59 (UTC): time to reset (occurs only once a day as reset will take > 1 second!)
          esp_restart();
        }
        prevDay = ntpD;
        z1 = millis() - m1;
      }

      wi.WDChanged(true);
      getAndPostCSV();
      wi.resetAll();
      kom.checkWifi();  
    }
  
  //Loop timing zones end here--------------------------------------------------------------------

  loopTimer(flag, z1);
  loopCount = (loopCount + 1) % rptIntvl;
  // END OF LOOP
}

/*****************************************************************************************************/

// Global setup methods ------------------------------------------------------------------------------

/*****************************************************************************************************
a. tryMQTTStart(): attempts to start MQTT connection: reboots ESP after 10 failed attempts
parameters: none
returns: void
*****************************************************************************************************/
void tryMQTTStart() {
  int qtCount = 0;
  while ( !qtSetup(kom.getNwkIx()) ) {
    fn_RedLed( ON );
    Serial.println("MQTT setup failed. No MQTT comms. Check RPi is powered and running.");
    digitalWrite(LEDPin, !digitalRead(LEDPin));
    delay(800);
    if (qtCount++ == 11) esp_restart();
  }
  fn_RedLed( OFF );
}

/*****************************************************************************************************
b. requestRptInterval(): sends message to Shed to prompt repeat Interval value by return
parameters: none
returns: void
*****************************************************************************************************/
void requestRptInterval() {
  // format and post initial reQuest string
  int i;
  char buf[BUF_LEN];
  sprintf(buf, "%02d", wi.getA7());
  buf[2] = '\0';
  publishMQTT(true, false, buf);
  char ch;
  bool bContinue = true;
  int loops = 0;
  while (bContinue && (loops < INIT_WAIT)) {
    qtClient.loop();
    if (bNewMQTT) {  // 'I' == initial Shed information message
      ch = qticBuf[0];
      //Serial.print(ch);
      rptIntvl = atoi(qticBuf + 1);
      Serial.print("Rpt Intvl: ");
      Serial.println(rptIntvl);
      bNewMQTT = false;
      bContinue = false;
    }
    loops++;
    delay(100); // Review this interval once Python code is written!
  }
}

/*****************************************************************************************************
c. logStartupSuccess(): sends message reporting startup is complete and successful
parameters: none
returns: void
*****************************************************************************************************/
void logStartupSuccess() {
  // Log a startup success message
  strcpy(versionBuf, "Roof4 ver ");
  strcpy(versionBuf + 10, __DATE__);
  versionBuf[21] = ' ';
  strcpy(versionBuf + 22, __TIME__);
  strcpy(versionBuf + 30, ": Roof setup done.");
  postMessage(versionBuf);
  versionBuf[30] = '\0';  // truncate for all later uses
}

// Global loop methods -------------------------------------------------------------------------------

/*****************************************************************************************************
1. shedRequested(): confirms Shed wants hourly WD data and posts hourly CSV
parameters: none
returns: bool
*************************************************************************************************************/
/*
*/

/************************************************************************************************************
2. blink(): controls 3-seocond LED blinking (daylight only)
parameters: none
returns: void
************************************************************************************************************/
void blink() {
  if (wi.getLight4Blink() > MIN_LIGHT) { // daytime
    digitalWrite(LEDPin, !digitalRead(LEDPin)); // blink
  } else { // nighttime
    digitalWrite(LEDPin, LOW);
  }
}
/*
2a.	Operate Red LED
*/
void fn_RedLed( int state )
{
  digitalWrite(RedPin, state );
}	

/************************************************************************************************************
3. getAndPostCSV(): gathers CSV data and posts to Shed
parameters: none
returns: void
*************************************************************************************************************/
void getAndPostCSV() {
  char freqBuf[BUF_LEN];
  char starBuf[STARBUF_LEN];
  wi.getStarCSV(starBuf);
  publishMQTT(false, true, starBuf);
  Serial.println(starBuf); // TEMP
  wi.getFreqCSV(freqBuf);
  publishMQTT(false, false, freqBuf);
  Serial.println(freqBuf); // TEMP
}

/******************************************************************************************************
4. loopTimer(): aims to keep loop time to 1/4 second intervals: logs message if code runs longer
parameters: none
returns: void
******************************************************************************************************/
void loopTimer(int flag, unsigned long z1) {
// End-of-loop timing code
  unsigned long loopEnd = millis();
  if ((loopEnd - loopStart) > LOOP_TIME) { // Code took > LOOP_TIME
    char mBuf[BUF_LEN];
    sprintf(mBuf, "Long lp time %ul; Flg:%d; NTP:%u", loopEnd - loopStart, flag, z1);
    postMessage(mBuf);  // log all long loops (> LOOP_TIME)
  }
  else {  // wait until LOOP_TIME has elapsed
    while(millis() - loopStart < LOOP_TIME) {
      delay(1);
    }
  }
}

