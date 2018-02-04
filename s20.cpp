#include "ESP8266.h" /* needed to switch the GPIO attached relay and device config */

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
   state          = false;
   newData        = false;
   buttonDebounce = 0;
   led            = true;
   setLed(S20_GREEN_LED_ON); /* enable green LED during init */
}
void S20::setLed(bool state)
{
   if (led != state)
   {
      led = state;

      if (led == S20_GREEN_LED_ON)
      {
         digitalWrite(ledPin, LOW);
      }
      else
      {
         digitalWrite(ledPin, HIGH);
      }
   }
}

bool S20::getLed(void)
{
   return led;
}

void S20::main(void)
{
   if (getLed() == S20_GREEN_LED_ON)
   {
      setLed(S20_GREEN_LED_OFF); /* disable green LED when operational */
   }

   if (buttonDebounce > 0)
   {
      buttonDebounce--;
   }
}

void S20::toggleState(void)
{
   if (buttonDebounce == 0)
   {
      buttonDebounce = buttonDebounceTime;
      newData = true;

      if (state == true)
      {
         state = false;
         digitalWrite(relayPin, LOW);
      }
      else
      {
         state = true;
         digitalWrite(relayPin, HIGH);
      }
   }
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
