#ifndef _MAIN_H_
#define _MAIN_H_
#include "Arduino.h"
#include <FS.h> // SPIFFS

/*===================================================================================================================*/
/* variable declarations */
/*===================================================================================================================*/
struct configuration
{
  const char*    name;
  int            tTemp;  /* persistent target temperature */
  int            calibF;
  int            calibO;
  const char*    ssid;
  const char*    wifiPwd;
  const char*    mqttHost;
  int            mqttPort;
  const char*    mqttUser;
  const char*    mqttPwd;
  const char*    updServer;
  int            sensUpdInterval;
  int            mqttPubCycleInterval;
};

/*===================================================================================================================*/
/* function declarations */
/*===================================================================================================================*/
/* setup */
void GPIO_CONFIG(void);
void SPIFFS_INIT(void);
void DISPLAY_INIT(void);
void WIFI_CONNECT(void);
void MQTT_CONNECT(void);
void OTA_INIT(void);
/* loop */
void HANDLE_SYSTEM_STATE(void);
void SENSOR_MAIN(void);
void DRAW_DISPLAY_MAIN(void);
void MQTT_MAIN(void);
void SPIFFS_MAIN(void);
void HANDLE_HTTP_UPDATE(void);
/* callback */
LOCAL void sensor_cb(void *arg); /* sensor timer callback */
void messageReceived(String &topic, String &payload); /* MQTT callback */
void encoderSwitch (void);
void updateEncoder(void);
/* others */
void homeAssistantDiscovery(void);
void mqttPubState(void);
void loadConfiguration(configuration &config);
bool saveConfiguration(const configuration &config);
String readSpiffs(String file);
boolean writeSpiffs(String file, String newFileContent);
/*===================================================================================================================*/
/* library functions */
/*===================================================================================================================*/
float  intToFloat(int intValue);
int    floatToInt(float floatValue);
String boolToStringOnOff(bool boolean);
String boolToStringHeatOff(bool boolean);
long TimeDifference(unsigned long prev, unsigned long next);
long TimePassedSince(unsigned long timestamp);
bool TimeReached(unsigned long timer);
void SetNextTimeInterval(unsigned long& timer, const unsigned long step);
bool splitSensorDataString(String sensorCalib, int *offset, int *factor);

#endif /* _MAIN_H_ */