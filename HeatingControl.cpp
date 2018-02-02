#include "HeatingControl.h"

#include "ESP8266.h" /* needed to switch the GPIO attached relay */

HeatingControl::HeatingControl()
{
   HeatingControl::init();
}

HeatingControl::~HeatingControl()
{
   /* do nothing */
}

void HeatingControl::init()
{
   heatingAllowed       = true;
   heatingEnabled       = false;
   targetTemperature    = 200;
   newData              = false;
   digitalWrite(relayPin,HIGH); /* switch relay OFF */
}

void HeatingControl::setHeatingEnabled(bool value)
{
   if (value != heatingEnabled)
   {
      newData = true;
      if (value == true)
      {
         digitalWrite(relayPin,LOW); /* switch relay ON */
      }
      else
      {
         digitalWrite(relayPin,HIGH); /* switch relay OFF */
      }
   }
   heatingEnabled  = value;
}

void HeatingControl::resetNewData()
{
   newData  = false;
}

void HeatingControl::increaseTargetTemperature(unsigned int value)
{
   if (value >= 0)
   {
      if ((targetTemperature + value) <= maxTargetTemp)
      {
         targetTemperature += value;
         newData = true;
      }
   }
}

void HeatingControl::decreaseTargetTemperature(unsigned int value)
{
   if (value >= 0)
   {
      if ((targetTemperature - value) >= minTargetTemp)
      {
         targetTemperature -= value;
         newData = true;
      }
   }
}

void HeatingControl::setTargetTemperature(int value)
{
   if (value > maxTargetTemp)
   {
      value = maxTargetTemp;
   }
   if (value < minTargetTemp)
   {
      value = minTargetTemp;
   }
   if (value != targetTemperature)
   {
      targetTemperature  = value;
      newData = true;
   }
}

bool HeatingControl::getHeatingEnabled(void)
{
   return heatingEnabled;
}

int HeatingControl::getTargetTemperature(void)
{
   return targetTemperature;
}

bool HeatingControl::getNewData()
{
   return newData;
}

bool HeatingControl::getHeatingAllowed()
{
   return heatingAllowed;
}

void HeatingControl::setHeatingAllowed(bool value)
{
   if (value != heatingAllowed)
   {
      newData = true;
   }
   heatingAllowed = value;
}

void HeatingControl::toggleHeatingAllowed()
{
   if (heatingAllowed == true)
   {
      heatingAllowed = false;
   }
   else
   {
      heatingAllowed = true;
   }
   newData = true;
}
