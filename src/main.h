#ifndef _MAIN_H_
#define _MAIN_H_
#include "Arduino.h"
#include <FS.h> // SPIFFS

/*===================================================================================================================*/
/* variable declarations */
/*===================================================================================================================*/
struct configuration
{
  char    name[64];
  int     tTemp;  /* persistent target temperature */
  int     tHyst;  /* thermostat hysteresis */
  int     calibF;
  int     calibO;
  char    ssid[64];
  char    wifiPwd[64];
  char    mqttHost[64];
  int     mqttPort;
  char    mqttUser[64];
  char    mqttPwd[64];
  char    updServer[256];
  int     sensUpdInterval;
  int     mqttPubCycleInterval;
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
void handleWebServerClient(void);
void messageReceived(String &topic, String &payload); /* MQTT callback */
void encoderSwitch (void);
void updateEncoder(void);
/* others */
void homeAssistantDiscovery(void);
void mqttPubState(void);
void loadConfiguration(configuration &config);
bool saveConfiguration(const configuration &config);
String readSpiffs(String file);
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