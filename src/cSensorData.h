#ifndef cSENSORDATA_H_
#define cSENSORDATA_H_

#include "Arduino.h"

class SensorData
{
public:
   SensorData(void);
   ~SensorData(void);
   void init(void);
   void setCurrentTemperature(int temperature);
   void setCurrentHumidity(int value);
   void setFilteredTemperature(int value);
   void setFilteredHumidity(int value);
   void setCheckSensor(bool value);
   void setLastSensorReadFailed(bool value);
   void resetNewData();
   void resetNewCalib();
   void setSensorCalibData(int offset, int factor, bool calib);
   int  getSensorFailureCounter(void);
   int  getCurrentTemperature(void);
   int  getCurrentHumidity(void);
   int  getFilteredTemperature(void);
   int  getFilteredHumidity(void);
   int  getSensorCalibOffset(void);
   int  getSensorCalibFactor(void);
   bool getCheckSensor(void);
   bool getSensorError(void);
   bool getNewData();
   bool getNewCalib();
private:
   int         currentTemperature;
   int         currentHumidity;
   int         filteredTemperature;
   int         filteredHumidity;
   int         sensorFailureCounter;
   int         tempOffset;
   int         tempFactor;
   bool        checkSensorThisLoop;
   bool        sensorError;
   bool        newData;
   bool        newCalib;
   const int   sensorErrorThreshold = 3;
};

#endif /* cSENSORDATA_H_ */
