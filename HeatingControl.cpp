#include "HeatingControl.h"

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
   relayGpio            = 16;
}

void HeatingControl::setup(unsigned char gpio, unsigned char tarTemp)
{
   /* limit target temperature range */
   if (tarTemp < minTargetTemp) {
      targetTemperature = minTargetTemp;
   }
   else if (tarTemp > maxTargetTemp) {
      targetTemperature = maxTargetTemp;
   }
   else {
      targetTemperature = tarTemp;
   }
   relayGpio          = gpio;    /* assign GPIO */
   pinMode(relayGpio, OUTPUT);   /* configure GPIO */
   digitalWrite(relayGpio,HIGH); /* switch relay OFF */
}

void HeatingControl::setHeatingEnabled(bool value)
{
   if (value != heatingEnabled)
   {
      newData = true;
      if (value == true)
      {
         digitalWrite(relayGpio,LOW); /* switch relay ON */
      }
      else
      {
         digitalWrite(relayGpio,HIGH); /* switch relay OFF */
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
   if ((targetTemperature + value) <= maxTargetTemp)
   {
      targetTemperature += value;
      newData = true;
   }
}

void HeatingControl::decreaseTargetTemperature(unsigned int value)
{
   if ((targetTemperature - value) >= minTargetTemp)
   {
      targetTemperature -= value;
      newData = true;
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
