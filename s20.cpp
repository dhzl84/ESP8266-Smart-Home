#include "config.h"

#if CFG_DEVICE == cS20
#include "s20.h"

S20::S20()
{
   S20::init();
}

S20::~S20()
{
   /* do nothing */
}

void S20::init(void)
{
   state             = false;
   newData           = false;
   led               = true;
   toggleTime        = 0;
   toggleTimeRef     = 0;
   S20::setLed(S20_GREEN_LED_ON); /* enable green LED during init */
}

void S20::setup(unsigned char relayPin, unsigned char togglePin, unsigned char ledPin, unsigned int switchDebounce)
{
   relayGpio         = relayPin;
   toggleGpio        = togglePin;
   ledGpio           = ledPin;
   toggleTimeThres   = switchDebounce;

   pinMode(relayGpio, OUTPUT);
   pinMode(toggleGpio, INPUT_PULLUP);
   pinMode(ledGpio, OUTPUT);
   S20::setLed(S20_GREEN_LED_ON); /* enable green LED during init */
   attachInterrupt(togglePin, S20_TOGGLE_PIN_CB, FALLING);
}

void S20::main(void)
{
   if (led == S20_GREEN_LED_ON)
   {
      setLed(S20_GREEN_LED_OFF); /* disable green LED after init */
   }
}

void S20::togglePinCB(void)
{
   toggleTime = millis();
   if ((toggleTime - toggleTimeRef) > toggleTimeThres)
   {
      toggleTimeRef = toggleTime;
      toggleState();
   }
}

void S20::toggleLed()
{
   led = !led;
   digitalWrite(ledGpio, led);
}

void S20::setLed(bool state)
{
   if (led != state)
   {
      led = state;

      if (led == S20_GREEN_LED_ON)
      {
         digitalWrite(ledGpio, LOW);
      }
      else
      {
         digitalWrite(ledGpio, HIGH);
      }
   }
}

bool S20::getLed(void)
{
   return led;
}


void S20::toggleState(void)
{
   state = !state;
   newData = true;
   digitalWrite(relayGpio, state);
}

void S20::setState(bool newState)
{
   if (state != newState)
   {
      toggleState();
   }
}

bool S20::getState(void)
{
   return state;
}

bool S20::getNewData(void)
{
   return newData;
}

void S20::resetNewData(void)
{
   newData = false;
}

#endif
