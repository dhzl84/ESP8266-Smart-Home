#ifndef cS20_H_
#define cS20_H_

#include "Arduino.h"

void S20_TOGGLE_PIN_CB(void);

class S20
{
public:
   S20(void);
   ~S20(void);
   void init(void);
   void setup(unsigned char relayGpio, unsigned char toggleGpio, unsigned char ledPin, unsigned int switchDebounce);
   void main(void);
   void toggleState(void);
   void setState(bool newState);
   bool getState(void);
   bool getNewData(void);
   void resetNewData(void);
   void togglePinCB(void);

private:
   bool state;
   bool led;
   bool newData;
   unsigned char relayGpio;
   unsigned char toggleGpio;
   unsigned char ledGpio;
   unsigned long toggleTime;
   unsigned long toggleTimeRef;
   unsigned int  toggleTimeThres;
   void setLed(bool state);
   bool getLed(void);
   void toggleLed();

};

#define buttonDebounceTime 15
#define S20_GREEN_LED_ON    0
#define S20_GREEN_LED_OFF   1

#endif /* cS20_H_ */
