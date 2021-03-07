#ifndef MAIN_H_
#define MAIN_H_
#include "Arduino.h"
#include <FS.h>  // SPIFFS
#include "config.h"
#include <ESP8266WiFi.h>

/* the config.h file contains your personal configuration of the parameters below: 
  #define WIFI_SSID                   "xxx"
  #define WIFI_PWD                    "xxx"
  #define LOCAL_MQTT_USER             "xxx"
  #define LOCAL_MQTT_PWD              "xxx"
  #define LOCAL_MQTT_PORT             1234
  #define LOCAL_MQTT_HOST             "123.456.789.012"
  #define DEVICE_BINARY               "http://<domain or ip>/<name>.bin"
  #define WIFI_RECONNECT_TIME         30      // seconds
  #define CFG_PUSH_BUTTONS            false
*/

/*===================================================================================================================*/
/* variable declarations */
/*===================================================================================================================*/
struct configuration {
  char    name[64];
  int16_t tTemp;  /* persistent target temperature */
  int16_t tHyst;  /* thermostat hysteresis */
  int16_t calibF;
  int16_t calibO;
  char    ssid[64];
  char    wifiPwd[64];
  char    mqtt_host[64];
  int16_t mqtt_port;
  char    mqtt_user[64];
  char    mqtt_password[64];
  char    update_server_address[256];
  int16_t sensUpdInterval;
  int16_t mqtt_publish_cycle;
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
void MQTT_MAIN(void);
void SPIFFS_MAIN(void);
void HANDLE_HTTP_UPDATE(void);
/* callback */
void handleWebServerClient(void);
void messageReceived(char* c_topic, byte* c_payload, unsigned int length);
void onOffButton(void);
#if CFG_PUSH_BUTTONS
void upButton(void);
void downButton(void);
#else
void updateEncoder(void);
#endif /* CFG_PUSH_BUTTONS */

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
void SetNextTimeInterval(volatile uint32_t *timer, const uint32_t step);
bool splitSensorDataString(String sensorCalib, int16_t *offset, int16_t *factor);
/* MACRO to append another line of the webpage table (2 columns) */
#define webpageTableAppend2Cols(key, value) (webpage +="<tr><td>" + key + ":</td><td>"+ value + "</td></tr>");
/* MACRO to append another line of the webpage table (4 columns) */
#define webpageTableAppend4Cols(key, value, current_value, description) (webpage +="<tr><td>" + key + "</td><td>"+ value + "</td><td>"+ current_value + "</td><td>" + description + "</td></tr>");
String millisFormatted(void);
String wifiStatusToString(wl_status_t status);
bool splitHtmlCommand(String sInput, String *key, String *value);
void updateTimeBuffer(void);
String getEspChipId(void);

#endif  // MAIN_H_
