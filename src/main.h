#ifndef MAIN_H_
#define MAIN_H_
#include "Arduino.h"
#include <ESP8266WiFi.h>  /* for wl_status_t */
#include "config.h"
#include "version.h"

/* the config.h file contains your personal configuration of the parameters below: 
  #define WIFI_SSID                   "xxx"
  #define WIFI_PWD                    "xxx"
  #define LOCAL_MQTT_USER             "xxx"
  #define LOCAL_MQTT_PWD              "xxx"
  #define LOCAL_MQTT_PORT             1234
  #define LOCAL_MQTT_HOST             "123.456.789.012"
  #define DEVICE_BINARY               "http://<domain or ip>/<name>.bin"
  #define SENSOR_UPDATE_INTERVAL      20      // seconds
  #define THERMOSTAT_HYSTERESIS       2       // seconds 
  #define WIFI_RECONNECT_TIME         30      // seconds
*/

/* inputMethod */
#define cROTARY_ENCODER 0
#define cPUSH_BUTTONS   1

/* sensor */
#define cDHT22 0
#define cBME280 1

/*===================================================================================================================*/
/* variable declarations */
/*===================================================================================================================*/
struct configuration {
  char          name[64];
  boolean       mode;         /* 0 = TH_OFF, 1 = TH_HEAT */
  boolean       inputMethod;  /* 0 = rotary encoder , 1 = three push buttons */
  int16_t       tTemp;        /* persistent target temperature */
  int16_t       tHyst;        /* thermostat hysteresis */
  int16_t       calibF;
  int16_t       calibO;
  char          ssid[64];
  char          wifiPwd[64];
  char          mqttHost[64];
  int16_t       mqttPort;
  char          mqttUser[64];
  char          mqttPwd[64];
  char          updServer[256];
  uint8_t       sensUpdInterval;
  uint8_t       mqttPubCycle;
  uint8_t dispBrightn;
  uint8_t       sensor;
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
void handleHttpReset(void);
void messageReceived(String &topic, String &payload);  // NOLINT: pass by reference

void onOffButton(void);
void upButton(void);
void downButton(void);
void updateEncoder(void);

/* MACRO to append another line of the webpage table */
#define webpageTableAppend(key, value) (webpage +="<tr><td>" + key + ":</td><td>"+ value + "</td></tr>");

/*===================================================================================================================*/
/* global scope functions */
/*===================================================================================================================*/
void homeAssistantDiscovery(void);
void mqttPubState(void);
void loadConfiguration(configuration &config);  // NOLINT: pass by reference
bool saveConfiguration(const configuration &config);
String readSpiffs(String file);

/*===================================================================================================================*/
/* library functions */
/*===================================================================================================================*/
float   intToFloat(int16_t intValue);
int16_t floatToInt(float floatValue);
String boolToStringOnOff(bool boolean);
String boolToStringHeatOff(bool boolean);
int32_t TimeDifference(uint32_t prev, uint32_t next);
int32_t TimePassedSince(uint32_t timestamp);
bool TimeReached(uint32_t timer);
void SetNextTimeInterval(uint32_t& timer, const uint32_t step);  // NOLINT: pass by reference
bool splitSensorDataString(String sensorCalib, int16_t *offset, int16_t *factor);
char* millisFormatted(void);
String wifiStatusToString(wl_status_t status);
#endif  // MAIN_H_
