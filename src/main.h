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

#define seconds_to_milliseconds(s)  static_cast<uint32_t>(s*1000u)
#define minutes_to_milliseconds(m)  static_cast<uint32_t>(m*60000u)
#define millisecondsToSeconds(ms)  static_cast<uint32_t>(ms/1000u)

/* input_method */
#define cROTARY_ENCODER 0
#define cPUSH_BUTTONS   1

/* sensor */
#define cDHT22 0
#define cBME280 1

/*===================================================================================================================*/
/* variable declarations */
/*===================================================================================================================*/
struct Configuration {
  char          name[64];
  bool          thermostat_mode;               /* 0 = TH_OFF, 1 = TH_HEAT */
  bool          input_method;                  /* 0 = rotary encoder , 1 = three push buttons */
  int16_t       target_temperature;            /* persistent target temperature */
  uint8_t       temperature_hysteresis;        /* thermostat hysteresis */
  int16_t       calibration_factor;
  int16_t       calibration_offset;
  char          ssid[64];
  char          wifi_password[64];
  char          mqtt_host[64];
  int16_t       mqtt_port;
  char          mqtt_user[64];
  char          mqtt_password[64];
  char          update_server_address[256];
  uint8_t       sensor_update_interval;
  uint8_t       mqtt_publish_cycle;
  uint8_t       display_brightness;
  uint8_t       sensor_type;
  bool          discovery_enabled;
  char          timezone[64];                   // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  bool          display_enabled;
  bool          auto_update;
  String        available_firmware_version;
  String        binary_address;
  String        binary_version_address;
};

/*===================================================================================================================*/
/* function declarations */
/*===================================================================================================================*/
/* setup */
void GPIO_CONFIG(void);
void FS_INIT(void);
void DISPLAY_INIT(void);
void WIFI_CONNECT(void);
void MQTT_CONNECT(void);
void OTA_INIT(void);
void SENSOR_INIT(void);
/* loop */
void HANDLE_SYSTEM_STATE(void);
void SENSOR_MAIN(void);
void DISPLAY_MAIN(void);
void MQTT_MAIN(void);
void FS_MAIN(void);
void HANDLE_HTTP_UPDATE(void);
void CHECK_FOR_UPDATE(void);
void NTP(void);
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
void homeAssistantRemoveDiscovered(void);
void homeAssistantRemoveDiscoveredObsolete(void);  /* DEPRECATED */
void mqttPubState(void);
void loadConfiguration(Configuration &config);  // NOLINT: pass by reference
bool saveConfiguration(const Configuration &config);
String readSpiffs(String file);

/*===================================================================================================================*/
/* library functions */
/*===================================================================================================================*/
float   intToFloat(int16_t intValue);
int16_t floatToInt(float floatValue);
String sensErrorToString(bool boolean);
String boolToStringOnOff(bool boolean);
String boolToStringHeatOff(bool boolean);
String boolToStringHeatingOff(bool boolean);
int32_t TimeDifference(uint32_t prev, uint32_t next);
int32_t TimePassedSince(uint32_t timestamp);
bool TimeReached(uint32_t timer);
void SetNextTimeInterval(volatile uint32_t* timer, const uint32_t step);
bool splitSensorDataString(String sensorCalib, int16_t *offset, int16_t *factor);
String millisFormatted(void);
String wifiStatusToString(wl_status_t status);
bool splitHtmlCommand(String sInput, String *key, String *value);
String getEspChipId(void);
bool is_daylight_saving_time(int year, int month, int day, int hour, int8_t tzHours);
#if CFG_BOARD_ESP32
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);  // NOLINT
String getEspResetReason(RESET_REASON reason);
#endif
int int_cmp(const void *a, const void *b);
void quickSort(int arr[], int left, int right);

class DiffTime {
 public:
  DiffTime(void);
  ~DiffTime(void);
  void set_time_start(void);
  void set_time_end(void);
  uint32_t get_time_duration(void);
  uint32_t get_time_duration_mean(void);
  uint32_t get_time_duration_min(void);
  uint32_t get_time_duration_max(void);
 private:
  uint32_t time_start_;
  uint32_t time_end_;
  uint32_t time_duration_;
  uint32_t time_duration_min_;
  uint32_t time_duration_max_;
  uint32_t time_duration_mean_;
  uint32_t time_duration_mean_buffer_;
  uint16_t time_count_;
};

#endif  // MAIN_H_
