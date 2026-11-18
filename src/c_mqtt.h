#ifndef C_MQTT_H_
#define C_MQTT_H_

/* buffer for the Home Assistant discovery payloads; the biggest one is the climate payload with
   ~880 bytes for a short device name and ~1080 bytes for the maximum name length of 63 chars */
#define cMQTT_PAYLOAD_SIZE 1280

#include "Arduino.h"

typedef enum {
  kTemp,
  kHum,
  kIP,  /* DEPRECATED */
  kCalibF,  /* DEPRECATED */
  kCalibO,  /* DEPRECATED */
  kFW,  /* DEPRECATED */
  kHysteresis,  /* DEPRECATED */
  kSensorState  /* DEPRECATED */
} Sensor_t;

typedef enum {
  kSensFail,  /* DEPRECATED */
  kThermostatState
} BinarySensor_t;

typedef enum {
  kRestart_switch,
  kUpdate_switch
} Switch_t;

typedef enum {
  kRestart_button,
  kUpdate_button
} Button_t;

class mqttHelper {
 public:
  mqttHelper(void);
  ~mqttHelper(void);
  void   setup(String nodeId);
  void   setTriggerDiscovery(bool discover);
  bool   getTriggerDiscovery(void);
  void   setTriggerRemoveDiscovered(bool remove);
  bool   getTriggerRemoveDiscovered(void);
  /* topic getters return cached topic strings built once in setup(), so that the
     MQTT callback and the publish path do not allocate a new String per call */
  const String& getTopicUpdateFirmware(void);
  const String& getTopicChangeName(void);
  const String& getTopicLastWill(void);
  const String& getTopicSystemRestartRequest(void);
  const String& getTopicChangeSensorCalib(void);
  const String& getTopicChangeHysteresis(void);
  const String& getTopicTargetTempCmd(void);
  const String& getTopicThermostatModeCmd(void);
  const String& getTopicHassDiscoveryClimate(void);
  String getTopicHassDiscoveryBinarySensor(BinarySensor_t binarySensor);
  String getTopicHassDiscoverySensor(Sensor_t sensor);
  String getTopicHassDiscoverySwitch(Switch_t switches);
  String getTopicHassDiscoveryButton(Button_t buttons);
  const String& getTopicData(void);
  const String& getTopicOutsideTemperature(void);
  /* Build Home Assistant discovery payloads into caller-provided buffers (no heap alloc).
     Use a buffer of cMQTT_PAYLOAD_SIZE bytes.
     Returns number of bytes written, or -1 (and an empty buf) if the buffer is too small. */
  int  buildHassDiscoveryClimate(char* buf, size_t size, const char* name, const String& firmware);
  int  buildHassDiscoveryBinarySensor(char* buf, size_t size, BinarySensor_t binarySensor);
  int  buildHassDiscoverySensor(char* buf, size_t size, Sensor_t sensor);
  int  buildHassDiscoveryButton(char* buf, size_t size, Button_t buttons);
  String mqttLastErrorToString(int8_t last_error);
  /* Build state topic payload into caller-provided buffer (no heap alloc).
     Returns number of bytes written, or -1 if truncated. */
  int  buildStateJSON(char* buf, size_t size, const char* name, const char* temp, const char* humid,
                      const char* hysteresis, const char* actState, const char* tarTemp,
                      const char* sensError, const char* thermoMode, int16_t calibration_factor,
                      const char* calibration_offset, const char* ip, const char* firmware);

 private:
  void   buildBaseTopic(void);
  void   buildTopics(void);
  int    checkPayload(char* buf, size_t size, int written, const char* what);

  bool   mqttTriggerDiscovery_;
  bool   mqttTriggerRemoveDiscovered_;
  String mqttData_;
  String mqttUpdateFirmware_;
  String mqttChangeName_;
  String mqttWill_;
  String mqttSystemRestartRequest_;
  String mqttPrefix_;
  String mqttNodeId_;
  String mqttDeviceName_;
  String mqttCompDevice_;
  String mqttCompSensor_;
  String mqttCompSwitch_;
  String mqttCompButton_;
  String mqttCompBinarySensor_;
  String mqttObjectId_;
  String mqttGeneralBaseTopic_;
  String mqttHassDiscoveryTopic_;
  String mqttChangeSensorCalib_;
  String mqttChangeHysteresis_;
  String mqttThermostatModeCmd_;
  String mqttTargetTempCmd_;
  String mqttOutsideTemperature_;
  /* fully qualified topics, built once per connection in buildTopics() */
  String mqttTopicData_;
  String mqttTopicUpdateFirmware_;
  String mqttTopicChangeName_;
  String mqttTopicLastWill_;
  String mqttTopicSystemRestartRequest_;
  String mqttTopicChangeSensorCalib_;
  String mqttTopicChangeHysteresis_;
  String mqttTopicTargetTempCmd_;
  String mqttTopicThermostatModeCmd_;
  String mqttTopicOutsideTemperature_;
  String mqttTopicDiscoveryClimate_;
  String mqttTopicDiscoveryBinarySensorState_;
  String mqttTopicDiscoverySensorTemp_;
  String mqttTopicDiscoverySensorHum_;
  String mqttTopicDiscoveryButtonUpdateButton_;
  String mqttTopicDiscoveryButtonRestartButton_;
};

#endif  // C_MQTT_H_
