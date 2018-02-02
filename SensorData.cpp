#include "SensorData.h"

SensorData::SensorData()
{
   SensorData::init();
}

SensorData::~SensorData()
{
   /* do nothing */
}

void SensorData::init()
{
   sensorError                = false;
   currentTemperature         = 0;
   currentHumidity            = 0;
   filteredTemperature        = 0;
   filteredHumidity           = 0;
   checkSensorThisLoop        = true;
   sensorFailureCounter       = 0;
   newData                    = false;
   tempOffset                 = 0;
   tempFactor                 = 100; /* factor in percent */
}

void SensorData::setCurrentTemperature(int value)
{
   currentTemperature = ((value * tempFactor/100) + tempOffset);
}

void SensorData::setCurrentHumidity(int value)
{
   currentHumidity = value;
}

void SensorData::setFilteredTemperature(int value)
{
   if (value != filteredTemperature)
   {
      newData = true;
   }
   filteredTemperature = value;
}

void SensorData::setFilteredHumidity(int value)
{
   if (value != filteredHumidity)
   {
      newData = true;
   }
   filteredHumidity = value;
}

void SensorData::resetNewData()
{
   newData  = false;
}

void SensorData::resetNewCalib()
{
   newCalib  = false;
}

void SensorData::setCheckSensor(bool value)
{
   checkSensorThisLoop = value;
}

void SensorData::setLastSensorReadFailed(bool value)
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

void SensorData::setSensorCalibData(int offset, int factor, bool calib)
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

int SensorData::getSensorFailureCounter(void)
{
   return sensorFailureCounter;
}

int SensorData::getCurrentTemperature(void)
{
   return currentTemperature;
}

int SensorData::getCurrentHumidity(void)
{
   return currentHumidity;
}

int SensorData::getFilteredTemperature(void)
{
   return filteredTemperature;
}

int SensorData::getFilteredHumidity(void)
{
   return filteredHumidity;
}

bool SensorData::getCheckSensor(void)
{
   return checkSensorThisLoop;
}

bool SensorData::getSensorError(void)
{
   return sensorError;
}

bool SensorData::getNewData(void)
{
   return newData;
}

bool SensorData::getNewCalib(void)
{
   return newCalib;
}

int SensorData::getSensorCalibOffset(void)
{
   return tempOffset;
}

int SensorData::getSensorCalibFactor(void)
{
   return tempFactor;
}
