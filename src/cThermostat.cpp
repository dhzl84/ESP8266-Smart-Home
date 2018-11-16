#include "cThermostat.h"

Thermostat::Thermostat()
{
   Thermostat::init();
}

Thermostat::~Thermostat()
{
   /* do nothing */
}

void Thermostat::init()
{
   //heating
   thermostatMode             = TH_HEAT;
   actualState                = TH_OFF;
   targetTemperature          = 200;      // initial value if setup() is not called; resolution is 0.1 °C
   newData                    = false;    // flag to indicate new data to be displayed and transmitted vai MQTT
   relayGpio                  = 16;       // relay GPIO if setup() is not called
   //sensor
   sensorError                = false;    // filtered sensor error
   currentTemperature         = 0;        // current sensor temperature
   currentHumidity            = 0;        // current sensor humidity
   filteredTemperature        = 0;        // filtered sensor temperature; median of CFG_MEDIAN_QUEUE_SIZE samples
   filteredHumidity           = 0;        // filtered sensor humidity; median of CFG_MEDIAN_QUEUE_SIZE samples
   sensorFailureCounter       = 0;        // count sensor failures during read
   tempOffset                 = 0;        // offset in 0.1 *C
   tempFactor                 = 100;      // factor in percent
   // temperture filter
   tempValueQueueFilled       = false;
   tempValueSampleID          = 0;

   for (int i=0; i<CFG_MEDIAN_QUEUE_SIZE; i++)
   {
      tempValueQueue[i] = (int)0;
   }

   // humidity filter
   humidValueQueueFilled      = false;
   humidValueSampleID         = 0;

   for (int i=0; i<CFG_MEDIAN_QUEUE_SIZE; i++)
   {
      humidValueQueue[i] = (int)0;
   }
}

void Thermostat::setup(unsigned char gpio, unsigned char tarTemp)
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

bool Thermostat::getActualState(void)              { return actualState; }
int  Thermostat::getTargetTemperature(void)        { return targetTemperature; }
bool Thermostat::getNewData()                      { return newData; }
bool Thermostat::getThermostatMode()               { return thermostatMode; }
void Thermostat::resetNewCalib()                   { newCalib  = false; }
int  Thermostat::getSensorFailureCounter(void)     { return sensorFailureCounter; }
int  Thermostat::getCurrentTemperature(void)       { return currentTemperature; }
int  Thermostat::getCurrentHumidity(void)          { return currentHumidity; }
int  Thermostat::getFilteredTemperature(void)      { return filteredTemperature; }
int  Thermostat::getFilteredHumidity(void)         { return filteredHumidity; }
bool Thermostat::getSensorError(void)              { return sensorError; }
bool Thermostat::getNewCalib(void)                 { return newCalib; }
int  Thermostat::getSensorCalibOffset(void)        { return tempOffset; }
int  Thermostat::getSensorCalibFactor(void)        { return tempFactor; }

void Thermostat::setActualState(bool value)
{
   if (value != actualState)
   {
      newData = true;
      if (value == TH_HEAT)
      {
         digitalWrite(relayGpio,LOW); /* switch relay ON */
      }
      else
      {
         digitalWrite(relayGpio,HIGH); /* switch relay OFF */
      }
   }
   actualState  = value;
}

void Thermostat::resetNewData()
{
   newData  = false;
}

void Thermostat::increaseTargetTemperature(unsigned int value)
{
   if ((targetTemperature + value) <= maxTargetTemp)
   {
      targetTemperature += value;
      newData = true;
   }
}

void Thermostat::decreaseTargetTemperature(unsigned int value)
{
   if ((targetTemperature - value) >= minTargetTemp)
   {
      targetTemperature -= value;
      newData = true;
   }
}

void Thermostat::setTargetTemperature(int value)
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

void Thermostat::setThermostatMode(bool value)
{
   if (value != thermostatMode)
   {
      newData = true;
   }
   thermostatMode = value;
}

void Thermostat::toggleThermostatMode()
{
   if (thermostatMode == TH_OFF)
   {
      thermostatMode  = TH_HEAT;
   }
   else
   {
      thermostatMode = TH_OFF;
   }
   newData = true;
}

// Sensor ===============================================================

void Thermostat::setCurrentTemperature(int value)
{
   currentTemperature = ((value * tempFactor/100) + tempOffset);

   // add new temperature to filter
   tempValueQueue[tempValueSampleID] = value;
   tempValueSampleID++;
   if (tempValueSampleID == CFG_MEDIAN_QUEUE_SIZE)
   {
      tempValueSampleID = 0;
      if (tempValueQueueFilled == false)
      {
         tempValueQueueFilled = true;
      }
   }

   // calculate new filtered temeprature
   float tempValue = (int) 0;
   if (tempValueQueueFilled == true)
   {
      for (int i=0; i<CFG_MEDIAN_QUEUE_SIZE; i++)
      {
         tempValue += (tempValueQueue[i]);
      }
      tempValue = (tempValue / (int) (CFG_MEDIAN_QUEUE_SIZE));
   }
   else /* return partially filtered value until queue is filled */
   {
      if (tempValueSampleID > 0)
      {
         for (int i=0; i<tempValueSampleID; i++)
         {
            tempValue += (tempValueQueue[i]);
         }
         tempValue = (tempValue / tempValueSampleID);
      }
   }

   filteredTemperature = tempValue;
}

void Thermostat::setCurrentHumidity(int value)
{
   currentHumidity = value;

   // add new humdity to filter
   humidValueQueue[humidValueSampleID] = value;
   humidValueSampleID++;
   if (humidValueSampleID == CFG_MEDIAN_QUEUE_SIZE)
   {
      humidValueSampleID = 0;
      if (humidValueQueueFilled == false)
      {
         humidValueQueueFilled = true;
      }
   }

   // calculate new filtered temeprature
   float humidValue = (int) 0;
   if (humidValueQueueFilled == true)
   {
      for (int i=0; i<CFG_MEDIAN_QUEUE_SIZE; i++)
      {
         humidValue += (humidValueQueue[i]);
      }
      humidValue = (humidValue / (int) (CFG_MEDIAN_QUEUE_SIZE));
   }
   else /* return partially filtered value until queue is filled */
   {
      if (humidValueSampleID > 0)
      {
         for (int i=0; i<humidValueSampleID; i++)
         {
            humidValue += (humidValueQueue[i]);
         }
         humidValue = (humidValue / humidValueSampleID);
      }
   }

   filteredHumidity = humidValue;
}

void Thermostat::setLastSensorReadFailed(bool value)
{
   /* filter sensor read failure here to avoid switching back and forth for single failure events */
   if (value == true)
   {
      sensorFailureCounter++;
   }
   else
   {
      if (sensorFailureCounter > 0)
      {
         sensorFailureCounter--;
      }
   }

   if (sensorError == false)
   {
      if (sensorFailureCounter >= sensorErrorThreshold)
      {
         newData = true;
         sensorError = true;
      }
   }
   else
   {
      if (sensorFailureCounter == 0)
      {
         newData = true;
         sensorError = false;
      }
   }
}

void Thermostat::setSensorCalibData(int offset, int factor, bool calib)
{
   if (tempOffset != offset)
   {
      tempOffset = offset;
      newCalib   = calib;
   }
   if (tempFactor != factor)
   {
      if (factor != 0)
      {
         tempFactor = factor;
         newCalib   = calib;
      }
   }

}