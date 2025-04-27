#ifndef W_INPUTS
#define W_INPUTS
#include "config.h"

#include <Wire.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>

/***********************************************************************************************
* WInputs.h: header file for WIinputs class (replaces both RainWind ans Sesnsors classes)      *
*                                                                                              *
* Version: 0.4                                                                                 *
* Last updated: 27/04/2025 16:06                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
struct meteo {
  int index;
  float val;
  char name[INAME_LEN];
};

class WInputs {
  public:
    WInputs();
    void begin();
    void updateMaxGust();
    void WDChanged(bool endLoop);
    void checkTips();
    float modalWD();
    float getLight4Blink();
    int updateMeteo(int loopCount, int maxLoop);
    void resetAll();
    float getFreqCSV(char* buf);
    int getStarCSV(char *buf);
    int getA7();

  private:
    // Nested classes
    Adafruit_AHTX0 _aht;
    Adafruit_BMP085_Unified _bmp;
    BH1750 _bh1750a;
    BH1750 _bh1750b;  
    
    int _ixPoll;
    uint _sensorStatus;
    float _hum; // temporary store for humidity between loops
    int _prevTip;
    int _tipsCount;
    int _prevWD;
    int _prevWDRevs;
    int _prevA7;
    int _pollRevs[POLL_COUNT];
    int _wd7[NUM_SHIFT7];
    meteo _items[NUM_ITEMS];
  
};
#endif