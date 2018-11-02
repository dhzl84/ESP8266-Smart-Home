#ifndef cHEATINGCONTROL_H_
#define cHEATINGCONTROL_H_

#include "Arduino.h"

#define minTargetTemp (unsigned char) 150
#define maxTargetTemp (unsigned char) 250

class HeatingControl
{
public:
   HeatingControl(void);
   ~HeatingControl(void);
   void init(void);
   void setup(unsigned char gpio, unsigned char tarTemp);
   void setHeatingEnabled(bool value);
   void setTargetTemperature(int value);
   void increaseTargetTemperature(unsigned int value);
   void decreaseTargetTemperature(unsigned int value);
   void resetTransmitRequest();
   void resetNewData();
   bool getHeatingEnabled(void);
   int  getTargetTemperature(void);
   bool getNewData();
   bool getHeatingAllowed();
   void setHeatingAllowed(bool value);
   void toggleHeatingAllowed();
private:
   bool          heatingEnabled;
   unsigned char targetTemperature;
   unsigned char relayGpio;
   bool          newData;
   bool          heatingAllowed;
};



#endif /* cHEATINGCONTROL_H_ */
