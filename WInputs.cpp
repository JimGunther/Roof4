#include "WInputs.h"
#include "config.h"
#include <string.h>
#include "Arduino.h"

/***********************************************************************************************
* WInputs.cpp: WInputs class: handles inerface with all sensors and counters (polling rain)    *
*                                                                                              *
* Version: 0.5                                                                                 *
* Last updated: 27/04/2025 16:06                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
// Interrupt Service Routine starts here______________

// Rain

volatile unsigned long _lastRTime  = 0;
volatile int _tipsCount; // number of rain bucket tips (cumulative per hour)
const int _margin = 5;  // minimum milliseconds between checks: insurance against contact bounce: ?? STILL NEEDED FOR HALL EFFECT SENSORS???  
const int _marginBuckets = 5;  // 5 milliseconds 

void IRAM_ATTR buckets_tipped();
void buckets_tipped() {
unsigned long thisRTime = esp_timer_get_time() / 1000;
  if (thisRTime - _lastRTime > _marginBuckets) {
    _tipsCount++;
    _lastRTime = thisRTime;
  }
}

// Wind 
volatile int _currRevs = 0;
volatile unsigned long _lastSTime = 0;
volatile unsigned long _thisSTime;

void IRAM_ATTR one_Rotation();
void one_Rotation() {
  _thisSTime = esp_timer_get_time() / 1000;
  if (_thisSTime - _lastSTime > _margin) {
    _currRevs++;
    _lastSTime = _thisSTime;
  }
}
// ------------------------ END OF ISRs --------------------------------------------------------------

// ************************* START OF Class WInputs PROPER *******************************************

WInputs::WInputs() {};

void WInputs::begin() {
  //set pin modes
  pinMode(RainPin, INPUT_PULLUP); 
  pinMode(RevsPin, INPUT_PULLUP);
  pinMode(WDPin, INPUT_PULLUP);
  pinMode(VoltsPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(RainPin), buckets_tipped, CHANGE); // rain buckets
  attachInterrupt(digitalPinToInterrupt(RevsPin), one_Rotation, RISING);  // anemometer

  _tipsCount = 0;
  _prevTip = digitalRead(RainPin);
  _currRevs = 0;
  _prevWDRevs = 0;
  _sensorStatus = 0;
  //_prevA7 = analogRead(WDPin) >> 7;
  char ixr[NUM_ITEMS << 1 + 1];
  strcpy(ixr, INDEXER);
  int i2;
  for (int i = 0; i < NUM_ITEMS; i++) {
    i2 = i << 1;
    _items[i].index = i;
    _items[i].name[0] = ixr[i2];
    _items[i].name[1] = ixr[i2 + 1];
    _items[i].name[2] = '\0';
  }
  
  // Initialize the four I2C sensors
  if (!_bmp.begin()) _sensorStatus = 1;
  if (!_aht.begin()) _sensorStatus |= 2;
  if (!_bh1750a.begin()) _sensorStatus |= 4;
  if (!_bh1750b.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5c)) _sensorStatus |=8;
  Serial.print("Sensors: ");
  Serial.println(_sensorStatus);  
}; 

/**************************************************************************************************
updateMaxGust(): results stored in one of 12 array posistions (12 == 3 secs speed measurement interval / 0.25 secs poll interval)
(polled 4 times a second to catch gusts)
parameters: none
returns: void
***************************************************************************************************/
void WInputs::updateMaxGust() { 
  int n = 2; // index for gust
  int revsNow = _currRevs; // from last interrupt
  int revsInc = revsNow - _pollRevs[_ixPoll];
  revsInc = max(0, revsInc);
  _pollRevs[_ixPoll] = revsNow;

  if (revsInc > _items[n].val) {
    _items[n].val = (float)revsInc;
  }

  _ixPoll = (_ixPoll + 1) % POLL_COUNT;
}

/**************************************************************************************************
checkTips(): checks if buckets have tipped and, if so, increments counter
parameters:none
returns: void
***************************************************************************************************/
void WInputs::checkTips() {
  int currTip = digitalRead(RainPin);
  if (currTip != _prevTip) _tipsCount++;
  _prevTip = currTip;
}

/**************************************************************************************************
WDChanged(): code to add anemometer revs to "frequent" and hourly revs count (polled every second)
parameters: none
returns: void
***************************************************************************************************/
void WInputs::WDChanged(bool endLoop) 
{
  int currA7 = analogRead(WDPin) >> 7;
  int revsNow = _currRevs; // _currRevs is volatile
  if ((currA7 != _prevA7) || endLoop) {
    _wd7[_prevA7] += revsNow - _prevWDRevs;
    _prevWDRevs = revsNow;
    _prevA7 = currA7;
  }
}

/****************************************************************************************************
getLight4Blink(): gets the average light level to aid switching off of blinking LED at night
parameters: none
return: int : light level (average of 2 sensors)
****************************************************************************************************/
float WInputs::getLight4Blink() {
  return (0.5f * (_items[7].val + _items[8].val));
}

/***************************************************************************************************
updateMeteo(): updates one of the Meteo values according to loopCount
parameters: loopCount: int: the loop count number used to choose which Meteo object to update
return: None
****************************************************************************************************/
int WInputs::updateMeteo(int loopCount, int maxLoop) {
  int startLoop = maxLoop - 2 * NUM_ITEMS;
  sensors_event_t humidity, temp;
  float tmp;
  float op;
  float p = 0.0f;
  int flag = 0;
  if (loopCount < startLoop) return 0;
  if (loopCount & 1) return 0;  // no odd-numbered loops

  int ix = (loopCount - startLoop) >> 1;  // divide "excess" by 2 to choose the right Meteo object
  
  switch (ix) {
    case 0: // rainfall/ bucket tips
      _items[ix].val = (float)_tipsCount;
      //flag = 1;
      break;
    case 1: // wind /revs
      _items[ix].val = (float)_currRevs;
      //flag = 2;
      break;
    case 2: // gusts
      break; // nothing to do as gusts are updated more frequently
    case 3:
      _items[ix].val = analogRead(WDPin); // RPi calculated modal WD from Star data: this for "raw" data only
      break;
    case 4:
      _hum = 98.0f;
      tmp = 99.0f;
      if (((_sensorStatus & 2) == 0) && _aht.getEvent(&humidity, &temp)) {
        _hum = humidity.relative_humidity;
        tmp = temp.temperature;
      }
      _items[ix].val = tmp;
      //flag = 4;
      break;
    case 5: // humidity should be stored 0.5 secs ago by line 190
      _items[ix].val = _hum;
      //flag = 8;
      break;
    case 6: // pressure:
      p = 97.0f;
      if ((_sensorStatus & 1) == 0) _bmp.getPressure(&p);
      _items[ix].val = 0.01f * p;
      //flag = 16;
      break;      
    case 7: // light 1 :: 11 * log10(1 + lux)
      op = 96.0f;
      if ((_sensorStatus & 4) == 0) op = _bh1750a.readLightLevel();
      if (op < 0.0) op = 0.0;
      _items[ix].val = op;
      //flag = 32;
      break;
    case 8: // light 2
      op = 95.0f;
      if ((_sensorStatus & 8) == 0) op = _bh1750b.readLightLevel();
      if (op < 0.0) op = 0.0;
      _items[ix].val = op;
      //flag = 64;
      break;
    case 9:
      _items[ix].val = (float)analogRead(VoltsPin);
      //flag = 128;
      break;
    default:
      break;
  }
  return flag;
}

/********************************************************************************************
resetAll(): all counters every Freq secs
parameters: none
returns: void
********************************************************************************************/
void WInputs::resetAll() {
  int i;
  for (i = 0; i < NUM_SHIFT7; i++) _wd7[i] = 0;
  _currRevs = 0;
  _prevWDRevs = 0;
  _tipsCount = 0;
  _items[2].val = 0;  // reset max gust
}

/*******************************************************************************************
getFreqCSV(): code to concatenate all the weather values CSV strings and place in buffer (82+ bytes length)
parameter: buf: char*: buffer to receive CSV text
returns: int: revs count this reporting interval
********************************************************************************************/
float WInputs::getFreqCSV(char* buf) {
  char mBuf[FITEM_LEN + 4];
  int i;
  int len = 0;
  for (i = 0; i < NUM_ITEMS; i++) {
    sprintf(mBuf, ",%07.2f", _items[i].val);
    strcpy(buf + len, mBuf);
    len = strlen(mBuf) + len;
  }
  return _items[1].val;
}

/*******************************************************************************************
getStarCSV(): code to concatenate all the wind direction values CSV strings and place in buffer (82+ bytes length)
parameter: buf: char*: buffer to receive CSV text
returns: bool: True if success, otherwise false
********************************************************************************************/
int WInputs::getStarCSV(char* buf) {
  char mBuf[HITEM_LEN + 1];
  int i, sum = 0;
  for (i = 0; i < NUM_SHIFT7; i++) {
    sprintf(mBuf, ",%02d", _wd7[i]);
    strcpy(buf + i * HITEM_LEN, mBuf);
    sum += _wd7[i];
  }
  return sum;
}

/*******************************************************************************************
getA7(): code to get WD analogue value / 128
parameter: none
returns: int: analogue value >> 7
*****************************************************************************************/
int WInputs::getA7() {
  return (analogRead(WDPin) >> 7);
}