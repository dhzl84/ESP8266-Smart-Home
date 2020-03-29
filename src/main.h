#ifndef MAIN_H_
#define MAIN_H_
#include "board.h"
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
  bool          mode;         /* 0 = TH_OFF, 1 = TH_HEAT */
  bool          inputMethod;  /* 0 = rotary encoder , 1 = three push buttons */
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
  uint8_t       dispBrightn;
  uint8_t       sensor;
  bool          discovery;
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
void updateEncoder(void);

/* MACRO to append another line of the webpage table (2 columns) */
#define webpageTableAppend2Cols(key, value) (webpage +="<tr><td>" + key + ":</td><td>"+ value + "</td></tr>");
/* MACRO to append another line of the webpage table (4 columns) */
#define webpageTableAppend4Cols(key, value, current_value, description) (webpage +="<tr><td>" + key + "</td><td>"+ value + "</td><td>"+ current_value + "</td><td>" + description + "</td></tr>");

/*===================================================================================================================*/
/* global scope functions */
/*===================================================================================================================*/
void homeAssistantDiscovery(void);
void homeAssistantUndiscover(void);
void homeAssistantUndiscoverObsolete(void);  /* DEPRECATED */
void mqttPubState(void);
void loadConfiguration(configuration &config);  // NOLINT: pass by reference
bool saveConfiguration(const configuration &config);
String readSpiffs(String file);

/*===================================================================================================================*/
/* library functions */
/*===================================================================================================================*/
float   intToFloat(int16_t intValue);
int16_t floatToInt(float floatValue);
String sensErrorToString(bool boolean);
String boolToStringOnOff(bool boolean);
String boolToStringHeatOff(bool boolean);
int32_t TimeDifference(uint32_t prev, uint32_t next);
int32_t TimePassedSince(uint32_t timestamp);
bool TimeReached(uint32_t timer);
void SetNextTimeInterval(volatile uint32_t* timer, const uint32_t step);
bool splitSensorDataString(String sensorCalib, int16_t *offset, int16_t *factor);
String millisFormatted(void);
String wifiStatusToString(wl_status_t status);
bool splitHtmlCommand(String sInput, String *key, String *value);

String getEspChipId(void);

#if CFG_BOARD_ESP32
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);  // NOLINT
String getEspResetReason(RESET_REASON reason);
#endif

#endif  // MAIN_H_
