#include "Komms.h"
#include "Arduino.h"

/*
**************************************************************************
* Komms.cpp: Komms class: handles all Wifi and MQTT comms                *
*                                                                        *
* Version: 0.2                                                           *
* Last updated: 20/02/2025 10:55                                         *
* Author: Jim Gunther                                                    *
*                                                                        * 
* Change History                                                         *
*                                                                        *
*	14/03/2025	0.2 - > 0.3	RDG		Added RED led blink on WiFi              *                                                                        *
*                                                                        *
**************************************************************************
 */

// class Komms: responsible for wifi communications
// Also reads stored Wifi login credentials

Komms::Komms() {};  // empty constructor

/*****************************************************************************************************
begin(): initiation code for Komms object: connects to Wifi
parameters: none
returns: void
******************************************************************************************************/
void Komms::begin() {
  // Set WiFi to station mode and disconnect from an AP if it was previously connected.

  int WiFi_count;
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  readCredentials();  // gets all networks' router credentials
  if (!connectToWiFi()) {
    _status = 1;
    Serial.println("WiFi connection failed.");

    while( WiFi_count < 1000 ) {
      WiFi_count++;
      digitalWrite( RedPin, ON );
      digitalWrite(LEDPin, !digitalRead(LEDPin));
      delay(200);
      digitalWrite( RedPin, OFF );
    }
    esp_restart();
  }
  delay( 1000 );
}
/*******************************************************************************************
unScram: algorithm to unscramble string buffer contents
parameters:
  src: string input
  dest: string: output
returns: void
*******************************************************************************************/
void Komms::unScram(const char* src, char* dest) {
  int len = strlen(src);
  int i;
  for (i = 0; i < len; i++) {
    dest[i] = src[i] + 3 - i;
  }
  dest[len] = '\0'; // null-terminated string
}

/********************************************************************************************************
connectToWiFi(): scans wifi networks and choose a match with Shed or Home
parameters: none
Returns: boolean: true if successful
/*********************************************************************************************************/
bool Komms::connectToWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.print("WiFi Connecting...");
  // Scan and choose network to connect
  WiFi.disconnect();
  int nn = WiFi.scanNetworks();
  if (nn > 0) {
    int i, j;
    for (i = 0; i < nn; i++) {
      for (j = 0; j < NUM_NETWORKS; j++) {
        if (strcmp(WiFi.SSID(i).c_str(), _ssid[j]) == 0) {
          strcpy(_ssidChosen, _ssid[j]);
          unScram(_pwd[j], _pwdChosen);
          _nwkIx = j;
        }
      }
    }
  } else {
    return false;
  }
  
  // Now attempt connection to chosen network
  int numTries = 0;
  WiFi.disconnect();
  WiFi.begin(_ssidChosen, _pwdChosen);
  WiFi.waitForConnectResult();
  while ((WiFi.status() != WL_CONNECTED) || numTries++ < 5) {
    delay(1000);
  }
  Serial.print("WiFi connected. IP Address: ");
  strcpy(_ipAddress, WiFi.localIP().toString().c_str());
  Serial.println(_ipAddress); // this is IP address for HUB, NOT MQTT server!
  return true;
}

/*****************************************************************************************************
getNwkIx();: gets the index number (0-2) of which Wifi network kad been found (shed, Jim, Richard)
parameters: none
returns: int: the network index number
*****************************************************************************************************/
int Komms::getNwkIx() { return _nwkIx; }

/*****************************************************************************************************
readCredentials(): reads the login credentials of allowed Wifi networks
parameters: none
returns: boolen: always true
******************************************************************************************************/
bool Komms::readCredentials() {
  strcpy(_ssid[0], SHED_SSID);
  strcpy(_pwd[0], SHED_PWD);
  strcpy(_ssid[1], JIM_SSID);
  strcpy(_pwd[1], JIM_PWD);
  strcpy(_ssid[2], RICH_SSID);
  strcpy(_pwd[2], RICH_PWD);
  return true;
}

/*****************************************************************************************************
checkWifi(): checks if wfi is still reconnected and reboots the ESP if not
parameters: none
returns bool: true if still connected
*****************************************************************************************************/
bool Komms::checkWifi() {
  bool ok = WiFi.status() == WL_CONNECTED;
  if (!ok) 
  {
    digitalWrite( RedPin, ON );
    esp_restart();
  } 
  return true;
}
