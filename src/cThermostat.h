#ifndef cTHERMOSTAT_H_
#define cTHERMOSTAT_H_

#include "Arduino.h"

#ifndef minTargetTemp
  #define minTargetTemp (unsigned char)150
#endif

#ifndef maxTargetTemp
  #define maxTargetTemp (unsigned char)250
#endif

#ifndef CFG_MEDIAN_QUEUE_SIZE
  #define CFG_MEDIAN_QUEUE_SIZE 10
#endif

#define TH_HEAT true
#define TH_OFF  false

class Thermostat
{
  public:
    Thermostat(void);
    ~Thermostat(void);
    void init(void);
    void setup(unsigned char gpio, unsigned char tarTemp, int calibFactor, int calibOffset);
    void loop(void);
    // heating
    void setActualState(bool value);
    void setTargetTemperature(int value);
    void increaseTargetTemperature(unsigned int value);
    void decreaseTargetTemperature(unsigned int value);
    void resetTransmitRequest();
    void resetNewData();
    bool getActualState(void);
    int  getTargetTemperature(void);
    bool getNewData();
    bool getThermostatMode();
    void setThermostatMode(bool value);
    void toggleThermostatMode();

    //sensor
    void setCurrentTemperature(int temperature);
    void setCurrentHumidity(int value);
    void setLastSensorReadFailed(bool value);
    void resetNewCalib();
    void setSensorCalibData(int factor, int offset, bool calib);
    int  getSensorFailureCounter(void);
    int  getCurrentTemperature(void);
    int  getCurrentHumidity(void);
    int  getFilteredTemperature(void);
    int  getFilteredHumidity(void);
    int  getSensorCalibOffset(void);
    int  getSensorCalibFactor(void);
    bool getSensorError(void);
    bool getNewCalib();

  private:
    // heating
    bool      thermostatMode;
    bool      actualState;
    uint8     targetTemperature;
    uint8     relayGpio;
    bool      newData;
    // sensor
    int       currentTemperature;
    int       currentHumidity;
    int       filteredTemperature;
    int       filteredHumidity;
    int       sensorFailureCounter;
    int       tempOffset;
    int       tempFactor;
    bool      sensorError;
    bool      newCalib;
    const int sensorErrorThreshold = 3;
    // filter
    bool      tempValueQueueFilled;
    int       tempValueSampleID;
    int       tempValueQueue[CFG_MEDIAN_QUEUE_SIZE];
    bool      humidValueQueueFilled;
    int       humidValueSampleID;
    int       humidValueQueue[CFG_MEDIAN_QUEUE_SIZE];
};

#endif /* cTHERMOSTAT_H_ */
