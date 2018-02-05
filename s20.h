#ifndef S20_H_
#define S20_H_

#include "Arduino.h"

class S20
{
public:
   S20(void);
   ~S20(void);
   void init(void);
   void setup(unsigned char relayGpio, unsigned char toggleGpio, unsigned char ledPin);
   void main(void);
   void toggleState(void);
   void setState(bool newState);
   bool getState(void);
   bool getNewData(void);
   void resetNewData(void);

private:
   bool state;
   bool led;
   bool newData;
   int  buttonDebounce;
   unsigned char relayGpio;
   unsigned char toggleGpio;
   unsigned char ledGpio;
   void setLed(bool state);
   bool getLed(void);
};

#define buttonDebounceTime 15
#define S20_GREEN_LED_ON    0
#define S20_GREEN_LED_OFF   1

#endif /* S20_H_ */
