#include "c_mqtt.h"

// The Home Asistant discovery_enabled topic need to follow a specific format:
//
//     <discovery_prefix>/<component>/[<node_id>/]<object_id>/<>
//
// <components> may be climate, sensors, etc. like defined by HA
// <node_id> (Optional): ID of the node providing the topic.
// <object_id>: The ID of the device. This is only to allow for separate topics for each device and is not used for the entity_id.

mqttHelper::mqttHelper()
  : mqttTriggerDiscovery_(false), \
    mqttTriggerRemoveDiscovered_(false), \
    mqttData_("/state"), \
    mqttUpdateFirmware_("/updateFirmware"), \
    mqttChangeName_("/changeName"), \
    mqttWill_("/availability"), \
    mqttSystemRestartRequest_("/systemRestartRequest"), \
    mqttPrefix_("homeassistant/"), \
    mqttNodeId_("tbd"), \
    mqttDeviceName_("unknown"), \
    mqttCompDevice_("climate/"), \
    mqttCompSensor_("sensor/"), \
    mqttCompSwitch_("switch/"), \
    mqttCompButton_("button/"), \
    mqttCompBinarySensor_("binary_sensor/"), \
    mqttObjectId_("/thermostat"), \
    mqttGeneralBaseTopic_("tbd"), \
    mqttHassDiscoveryTopic_("/config"), \
    mqttChangeSensorCalib_("/changeSensorCalib"), \
    mqttChangeHysteresis_("/changeHysteresis"), \
    mqttThermostatModeCmd_("/thermostatModeCmd"), \
    mqttTargetTempCmd_("/targetTempCmd"), \
    mqttOutsideTemperature_("outsideTemperature") {}

mqttHelper::~mqttHelper() {}

void mqttHelper::setup(String nodeId) {
  mqttNodeId_ = nodeId;
  buildTopics();
}

void mqttHelper::buildBaseTopic(void) {
  mqttGeneralBaseTopic_ = mqttPrefix_ + mqttCompDevice_ + mqttNodeId_ + mqttObjectId_;
}

/* Build every fully qualified topic once per connection. The getters hand out references to
   these, so publishing and the MQTT receive callback allocate nothing on each call. */
void mqttHelper::buildTopics(void) {
  buildBaseTopic();
  mqttTopicData_                          = mqttGeneralBaseTopic_ + mqttData_;
  mqttTopicUpdateFirmware_                = mqttGeneralBaseTopic_ + mqttUpdateFirmware_;
  mqttTopicChangeName_                    = mqttGeneralBaseTopic_ + mqttChangeName_;
  mqttTopicLastWill_                      = mqttGeneralBaseTopic_ + mqttWill_;
  mqttTopicSystemRestartRequest_          = mqttGeneralBaseTopic_ + mqttSystemRestartRequest_;
  mqttTopicChangeSensorCalib_             = mqttGeneralBaseTopic_ + mqttChangeSensorCalib_;
  mqttTopicChangeHysteresis_              = mqttGeneralBaseTopic_ + mqttChangeHysteresis_;
  mqttTopicTargetTempCmd_                 = mqttGeneralBaseTopic_ + mqttTargetTempCmd_;
  mqttTopicThermostatModeCmd_             = mqttGeneralBaseTopic_ + mqttThermostatModeCmd_;
  mqttTopicDiscoveryClimate_              = mqttGeneralBaseTopic_ + mqttHassDiscoveryTopic_;
  mqttTopicOutsideTemperature_            = mqttPrefix_ + mqttOutsideTemperature_;
  mqttTopicDiscoveryBinarySensorState_    = mqttPrefix_ + mqttCompBinarySensor_ + mqttNodeId_ + mqttObjectId_ + "State" + mqttHassDiscoveryTopic_;
  mqttTopicDiscoverySensorTemp_           = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "Temp" + mqttHassDiscoveryTopic_;
  mqttTopicDiscoverySensorHum_            = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "Hum" + mqttHassDiscoveryTopic_;
  mqttTopicDiscoveryButtonRestartButton_  = mqttPrefix_ + mqttCompButton_ + mqttNodeId_ + mqttObjectId_ + "Reset"  + mqttHassDiscoveryTopic_;
  mqttTopicDiscoveryButtonUpdateButton_   = mqttPrefix_ + mqttCompButton_ + mqttNodeId_ + mqttObjectId_ + "Update" + mqttHassDiscoveryTopic_;
}

int mqttHelper::buildStateJSON(char* buf, size_t size, const char* name, const char* temp, const char* humid,
                               const char* hysteresis, const char* actState, const char* tarTemp,
                               const char* sensError, const char* thermoMode, int16_t calibration_factor,
                               const char* calibration_offset, const char* ip, const char* firmware) {

  /* Build the full JSON in a static buffer. The calibration_factor is formatted as an integer
     using snprintf into a small temp buffer, then concatenated. */
  
  char sensScaleBuf[64];
  snprintf(sensScaleBuf, sizeof(sensScaleBuf), ",\n  \"sens_scale\":\"%d\"", calibration_factor);

  int written = snprintf(buf, size,
    "{\n"
    "  \"name\":\"%s\",\n"
    "  \"dev\":\"%s\",\n"
    "  \"mode\":\"%s\",\n"
    "  \"state\":\"%s\",\n"
    "  \"target_temp\":\"%s\",\n"
    "  \"current_temp\":\"%s\",\n"
    "  \"humidity\":\"%s\",\n"
    "  \"hysteresis\":\"%s\",\n"
    "  \"sens_status\":\"%s\",\n"
    "  \"sens_scale\":\"%d\",\n"
    "  \"sens_offs\":\"%s\",\n"
    "  \"fw\":\"%s\",\n"
    "  \"ip\":\"%s\"\n"
    "}",
    name, mqttNodeId_.c_str(), thermoMode, actState, tarTemp, temp, humid, hysteresis, sensError, calibration_factor, calibration_offset, firmware, ip);

  if (written < 0 || static_cast<size_t>(written) >= size) {
    #ifdef CFG_DEBUG
    Serial.println("State JSON truncated");
    #endif
    return -1;
  }
  
  return written;
}

/* Build Home Assistant discovery payload into a fixed-size buffer.
   Uses stack-allocated char arrays to avoid heap fragmentation on ESP8266.
   Caller must call mqttHelper::getTopicHassDiscoveryClimate() to get the topic string. */
int mqttHelper::buildHassDiscoveryClimate(char* buf, size_t size, const char* name, const String& firmware) {
  static char climate_json[1024];

  int written = snprintf(climate_json, sizeof(climate_json),
    "{\n"
    "  \"~\":\"%s\",\n"
    "  \"name\":\"%s\",\n"
    "  \"mode_cmd_t\":\"~%s\",\n"
    "  \"mode_stat_t\":\"~%s\",\n"
    "  \"mode_stat_tpl\":\"{{value_json.mode}}\",\n"
    "  \"avty_t\":\"~%s\",\n"
    "  \"pl_avail\":\"online\",\n"
    "  \"pl_not_avail\":\"offline\",\n"
    "  \"temp_cmd_t\":\"~%s\",\n"
    "  \"temp_stat_t\":\"~%s\",\n"
    "  \"temp_stat_tpl\":\"{{value_json.target_temp}}\",\n"
    "  \"curr_temp_t\":\"~%s\",\n"
    "  \"curr_temp_tpl\":\"{{value_json.current_temp}}\",\n"
    "  \"min_temp\":\"15\",\n"
    "  \"max_temp\":\"25\",\n"
    "  \"temp_step\":\"0.5\",\n"
    "  \"modes\":[\"heat\",\"off\"],\n"
    "  \"act_t\":\"~%s\",\n"
    "  \"act_tpl\":\"{{value_json.state}}\",\n"
    "  \"json_attr_t\":\"~%s\",\n"
    "  \"uniq_id\":\"%s_climate\",\n"
    "  \"ic\":\"mdi:thermostat-box\",\n"
    "  \"dev\" : {\n"
    "    \"ids\":[\"%s\"],\n"
    "    \"mdl\":\"ESP8266 Thermostat\",\n"
    "    \"name\":\"Thermostat %s\",\n"
    "    \"sw\":\"%s\",\n"
    "    \"mf\":\"dhzl84\",\n"
    "    \"cu\":\"http://%s/\""
    "  }\n"
    "}\n"
    "}",
    mqttGeneralBaseTopic_.c_str(),
    name,
    mqttThermostatModeCmd_.c_str(),
    mqttData_.c_str(),
    mqttWill_.c_str(),
    mqttTargetTempCmd_.c_str(),
    mqttData_.c_str(),
    mqttData_.c_str(),
    mqttData_.c_str(),
    mqttData_.c_str(),
    mqttNodeId_.c_str(),
    mqttNodeId_.c_str(),
    name,
    firmware.c_str(),
    name
  );

  if (written < 0 || static_cast<size_t>(written) >= sizeof(climate_json)) {
    #ifdef CFG_DEBUG
    Serial.printf("Climate discovery JSON truncated at %d bytes\n", written);
    #endif
    written = -1;
  }

  size_t n = strlen(climate_json);
  if (n >= size) {
    n = size - 1;
  }
  memcpy(buf, climate_json, n);
  buf[n] = '\0';
  return written;
}

/* Build Home Assistant discovery payload for binary sensor into a fixed-size buffer. */
int mqttHelper::buildHassDiscoveryBinarySensor(char* buf, size_t size, BinarySensor_t binarySensor) {
  static char binary_sensor_json[cMQTT_PAYLOAD_SIZE];

  switch (binarySensor) {
    case kThermostatState:
    {
      size_t str_len = snprintf(binary_sensor_json, sizeof(binary_sensor_json),
        "{\n"
        "  \"~\":\"%s\",\n"
        "  \"name\":\"State\",\n"
        "  \"dev_cla\":\"heat\",\n"
        "  \"stat_t\":\"~%s\",\n"
        "  \"val_tpl\":\"{{value_json.state}}\",\n"
        "  \"pl_on\":\"heating\",\n"
        "  \"pl_off\":\"off\",\n"
        "  \"avty_t\":\"~%s\",\n"
        "  \"pl_avail\":\"online\",\n"
        "  \"pl_not_avail\":\"offline\",\n"
        "  \"json_attr_t\":\"~%s\",\n"
        "  \"uniq_id\":\"%s_state\",\n"
        "  \"ic\":\"mdi:radiator\",\n"
        "  \"dev\" : {\n"
        "    \"ids\":[\"%s\"]\n"
        "  }\n"
        "}",
        mqttGeneralBaseTopic_.c_str(),
        mqttData_.c_str(),
        mqttWill_.c_str(),
        mqttData_.c_str(),
        mqttNodeId_.c_str(),
        mqttNodeId_.c_str()
      );

      Serial.printf("Written: %d\n", str_len);
      Serial.printf("JSON Buffer size: %d\n", sizeof(binary_sensor_json));
      Serial.printf("JSON Buffer length: %d\n", strlen(binary_sensor_json));
      Serial.printf("JSON: %s\n", binary_sensor_json);

      if (str_len < size) {
        memcpy(buf, binary_sensor_json, str_len);
        buf[str_len] = '\0';
        Serial.printf("buf: %s\n", binary_sensor_json);
      } else {
        buf[0] = '\0';
      }
      return str_len;
    }
    default:
      buf[0] = '\0';
      return -1;
  }
}

/* Build Home Assistant discovery payload for sensor into a fixed-size buffer. */
int mqttHelper::buildHassDiscoverySensor(char* buf, size_t size, Sensor_t sensor) {
  static char sensor_json[512];

  switch (sensor) {
    case kTemp:
    {
      int written = snprintf(sensor_json, sizeof(sensor_json),
        "{\n"
        "  \"~\":\"%s\",\n"
        "  \"name\":\"Temperatur\",\n"
        "  \"dev_cla\":\"temperature\",\n"
        "  \"stat_t\":\"~%s\",\n"
        "  \"val_tpl\":\"{{value_json.current_temp}}\",\n"
        "  \"unit_of_meas\":\"°C\",\n"
        "  \"avty_t\":\"~%s\",\n"
        "  \"pl_avail\":\"online\",\n"
        "  \"pl_not_avail\":\"offline\",\n"
        "  \"json_attr_t\":\"~%s\",\n"
        "  \"uniq_id\":\"%s_sensTemp\",\n"
        "  \"ic\":\"mdi:thermometer\",\n"
        "  \"dev\" : {\n"
        "    \"ids\":[\"%s\"]\n"
        "  }\n"
        "}",
        mqttGeneralBaseTopic_.c_str(),
        mqttData_.c_str(),
        mqttWill_.c_str(),
        mqttData_.c_str(),
        mqttNodeId_.c_str(),
        mqttNodeId_.c_str()
      );

      size_t n = strlen(sensor_json);
      if (n >= size) {
        n = size - 1;
      }
      memcpy(buf, sensor_json, n);
      buf[n] = '\0';
      return written;
    }
    case kHum:
    {
      int written = snprintf(sensor_json, sizeof(sensor_json),
        "{\n"
        "  \"~\":\"%s\",\n"
        "  \"name\":\"Luftfeuchtigkeit\",\n"
        "  \"dev_cla\":\"humidity\",\n"
        "  \"stat_t\":\"~%s\",\n"
        "  \"val_tpl\":\"{{value_json.humidity}}\",\n"
        "  \"unit_of_meas\":\"%%\",\n"
        "  \"avty_t\":\"~%s\",\n"
        "  \"pl_avail\":\"online\",\n"
        "  \"pl_not_avail\":\"offline\",\n"
        "  \"json_attr_t\":\"~%s\",\n"
        "  \"uniq_id\":\"%s_sensHum\",\n"
        "  \"ic\":\"mdi:water-percent\",\n"
        "  \"dev\" : {\n"
        "    \"ids\":[\"%s\"]\n"
        "  }\n"
        "}",
        mqttGeneralBaseTopic_.c_str(),
        mqttData_.c_str(),
        mqttWill_.c_str(),
        mqttData_.c_str(),
        mqttNodeId_.c_str(),
        mqttNodeId_.c_str()
      );

      size_t n = strlen(sensor_json);
      if (n >= size) {
        n = size - 1;
      }
      memcpy(buf, sensor_json, n);
      buf[n] = '\0';
      return written;
    }
    default:
      snprintf(buf, size, "{}");
      return 2;
  }
}

/* Build Home Assistant discovery payload for button into a fixed-size buffer. */
int mqttHelper::buildHassDiscoveryButton(char* buf, size_t size, Button_t buttons) {
  static char button_json[512];

  switch (buttons) {
    case kRestart_button:
    {
      int written = snprintf(button_json, sizeof(button_json),
        "{\n"
        "  \"~\":\"%s\",\n"
        "  \"name\":\"Neustart\",\n"
        "  \"cmd_t\":\"~%s\",\n"
        "  \"avty_t\":\"~%s\",\n"
        "  \"pl_avail\":\"online\",\n"
        "  \"pl_not_avail\":\"offline\",\n"
        "  \"qos\":\"1\",\n"
        "  \"json_attr_t\":\"~%s\",\n"
        "  \"uniq_id\":\"%s_swRestart\",\n"
        "  \"ic\":\"mdi:restart\",\n"
        "  \"dev\" : { \n"
        "    \"ids\":[\"%s\"]\n"
        "  }\n"
        "}",
        mqttGeneralBaseTopic_.c_str(),
        mqttSystemRestartRequest_.c_str(),
        mqttWill_.c_str(),
        mqttData_.c_str(),
        mqttNodeId_.c_str(),
        mqttNodeId_.c_str()
      );

      size_t n = strlen(button_json);
      if (n >= size) {
        n = size - 1;
      }
      memcpy(buf, button_json, n);
      buf[n] = '\0';
      return written;
    }
    break;
    case kUpdate_button:
    {
      int written = snprintf(button_json, sizeof(button_json),
        "{\n"
        "  \"~\":\"%s\",\n"
        "  \"name\":\"Firmwareupdate\",\n"
        "  \"cmd_t\":\"~%s\",\n"
        "  \"avty_t\":\"~%s\",\n"
        "  \"pl_avail\":\"online\",\n"
        "  \"pl_not_avail\":\"offline\",\n"
        "  \"qos\":\"1\",\n"
        "  \"json_attr_t\":\"~%s\",\n"
        "  \"uniq_id\":\"%s_swUpdate\",\n"
        "  \"ic\":\"mdi:cellphone-arrow-down\",\n"
        "  \"dev\" : { \n"
        "    \"ids\":[\"%s\"]\n"
        "  }\n"
        "}",
        mqttGeneralBaseTopic_.c_str(),
        mqttUpdateFirmware_.c_str(),
        mqttWill_.c_str(),
        mqttData_.c_str(),
        mqttNodeId_.c_str(),
        mqttNodeId_.c_str()
      );

      size_t n = strlen(button_json);
      if (n >= size) {
        n = size - 1;
      }
      memcpy(buf, button_json, n);
      buf[n] = '\0';
      return written;
    }
    break;
    default:
      snprintf(buf, size, "{}");
      return 2;
  }
}

String mqttHelper::getTopicUpdateFirmware(void)                   { return mqttGeneralBaseTopic_ + mqttUpdateFirmware_;  }
String mqttHelper::getTopicChangeName(void)                       { return mqttGeneralBaseTopic_ + mqttChangeName_; }
String mqttHelper::getTopicLastWill(void)                         { return mqttGeneralBaseTopic_ + mqttWill_; }
String mqttHelper::getTopicSystemRestartRequest(void)             { return mqttGeneralBaseTopic_ + mqttSystemRestartRequest_; }
String mqttHelper::getTopicChangeSensorCalib(void)                { return mqttGeneralBaseTopic_ + mqttChangeSensorCalib_; }
String mqttHelper::getTopicChangeHysteresis(void)                 { return mqttGeneralBaseTopic_ + mqttChangeHysteresis_; }
String mqttHelper::getTopicTargetTempCmd(void)                    { return mqttGeneralBaseTopic_ + mqttTargetTempCmd_; }
String mqttHelper::getTopicThermostatModeCmd(void)                { return mqttGeneralBaseTopic_ + mqttThermostatModeCmd_; }
String mqttHelper::getTopicHassDiscoveryClimate(void)             { return mqttGeneralBaseTopic_ + mqttHassDiscoveryTopic_; }
String mqttHelper::getTopicData(void)                             { return mqttGeneralBaseTopic_ + mqttData_; }
String mqttHelper::getTopicOutsideTemperature(void)               { return mqttPrefix_ + mqttOutsideTemperature_; }
bool   mqttHelper::getTriggerDiscovery(void)                             { return mqttTriggerDiscovery_; }
void   mqttHelper::setTriggerDiscovery(bool discover)                    { mqttTriggerDiscovery_ = discover; }
bool   mqttHelper::getTriggerRemoveDiscovered(void)                      { return mqttTriggerRemoveDiscovered_; }
void   mqttHelper::setTriggerRemoveDiscovered(bool remove)               { mqttTriggerRemoveDiscovered_ = remove; }

String mqttHelper::getTopicHassDiscoveryBinarySensor(BinarySensor_t binarySensor) {
  String topic;

  switch (binarySensor) {
    case kThermostatState:
    {
      topic = mqttTopicDiscoveryBinarySensorState_;
    }
    break;
    default:
    {
      topic = "void";
    }
    break;
  }
  return (topic);
}

String mqttHelper::getTopicHassDiscoverySensor(Sensor_t sensor) {
  String topic;

  switch (sensor) {
    case kTemp:
      topic = mqttTopicDiscoverySensorTemp_;
    break;
    case kHum:
      topic = mqttTopicDiscoverySensorHum_;
    break;
    default:
    {
      topic = "void";
    }
    break;
  }
  return topic;
}

String mqttHelper::getTopicHassDiscoveryButton(Button_t buttons) {
  String topic = "void";

  switch (buttons) {
    case kRestart_button:
      topic = mqttTopicDiscoveryButtonRestartButton_;
    break;
    case kUpdate_button:
      topic = mqttTopicDiscoveryButtonUpdateButton_;
    break;
    default:
    {
      topic = "void";
    }
    break;
  }
  return topic;
}

String mqttHelper::mqttLastErrorToString(int8_t last_error) {
  String error_string;
  switch (last_error) {
    case 0:   error_string = "LWMQTT_SUCCESS";                     break;
    case -1:  error_string = "LWMQTT_BUFFER_TOO_SHORT";            break;
    case -2:  error_string = "LWMQTT_VARNUM_OVERFLOW";             break;
    case -3:  error_string = "LWMQTT_NETWORK_FAILED_CONNECT";      break;
    case -4:  error_string = "LWMQTT_NETWORK_TIMEOUT";            break;
    case -5:  error_string = "LWMQTT_NETWORK_FAILED_READ";        break;
    case -6:  error_string = "LWMQTT_NETWORK_FAILED_WRITE";       break;
    case -7:  error_string = "LWMQTT_REMAINING_LENGTH_OVERFLOW";   break;
    case -8:  error_string = "LWMQTT_REMAINING_LENGTH_MISMATCH";   break;
    case -9:  error_string = "LWMQTT_MISSING_OR_WRONG_PACKET";     break;
    case -10: error_string = "LWMQTT_CONNECTION_DENIED";           break;
    case -11: error_string = "LWMQTT_FAILED_SUBSCRIPTION";         break;
    case -12: error_string = "LWMQTT_SUBACK_ARRAY_OVERFLOW";       break;
    case -13: error_string = "LWMQTT_PONG_TIMEOUT";                break;
    default:  error_string = "UNKNOWN";                            break;
  }
  return error_string;
}
