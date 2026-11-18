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

/* snprintf() returns the length the payload WOULD have had. If that does not fit into the buffer
   the JSON is cut off and invalid, so the buffer is cleared and -1 is returned. The caller must not
   publish an empty payload: an empty retained discovery message makes Home Assistant delete the entity. */
int mqttHelper::checkPayload(char* buf, size_t size, int written, const char* what) {
  if ((written < 0) || (static_cast<size_t>(written) >= size)) {
    #ifdef CFG_DEBUG
    Serial.print(what);
    Serial.print(" JSON does not fit into buffer, needed bytes: ");
    Serial.println(written + 1);
    #endif
    buf[0] = '\0';
    return -1;
  }
  return written;
}

int mqttHelper::buildStateJSON(char* buf, size_t size, const char* name, const char* temp, const char* humid,
                               const char* hysteresis, const char* actState, const char* tarTemp,
                               const char* sensError, const char* thermoMode, int16_t calibration_factor,
                               const char* calibration_offset, const char* ip, const char* firmware) {

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

  return checkPayload(buf, size, written, "State");
}

/* Build Home Assistant discovery payload for the climate component into a fixed-size buffer. */
int mqttHelper::buildHassDiscoveryClimate(char* buf, size_t size, const char* name, const String& firmware) {
  int written = snprintf(buf, size,
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
    "    \"cu\":\"http://%s/\"\n"
    "  }\n"
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

  return checkPayload(buf, size, written, "Climate discovery");
}

/* Build Home Assistant discovery payload for binary sensor into a fixed-size buffer. */
int mqttHelper::buildHassDiscoveryBinarySensor(char* buf, size_t size, BinarySensor_t binarySensor) {
  switch (binarySensor) {
    case kThermostatState:
    {
      int written = snprintf(buf, size,
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

      return checkPayload(buf, size, written, "Binary sensor discovery");
    }
    default:
      buf[0] = '\0';
      return -1;
  }
}

/* Build Home Assistant discovery payload for sensor into a fixed-size buffer. */
int mqttHelper::buildHassDiscoverySensor(char* buf, size_t size, Sensor_t sensor) {
  switch (sensor) {
    case kTemp:
    {
      int written = snprintf(buf, size,
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

      return checkPayload(buf, size, written, "Temperature sensor discovery");
    }
    case kHum:
    {
      int written = snprintf(buf, size,
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

      return checkPayload(buf, size, written, "Humidity sensor discovery");
    }
    default:
      snprintf(buf, size, "{}");
      return 2;
  }
}

/* Build Home Assistant discovery payload for button into a fixed-size buffer. */
int mqttHelper::buildHassDiscoveryButton(char* buf, size_t size, Button_t buttons) {
  switch (buttons) {
    case kRestart_button:
    {
      int written = snprintf(buf, size,
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

      return checkPayload(buf, size, written, "Restart button discovery");
    }
    break;
    case kUpdate_button:
    {
      int written = snprintf(buf, size,
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

      return checkPayload(buf, size, written, "Update button discovery");
    }
    break;
    default:
      snprintf(buf, size, "{}");
      return 2;
  }
}

const String& mqttHelper::getTopicUpdateFirmware(void)            { return mqttTopicUpdateFirmware_; }
const String& mqttHelper::getTopicChangeName(void)                { return mqttTopicChangeName_; }
const String& mqttHelper::getTopicLastWill(void)                  { return mqttTopicLastWill_; }
const String& mqttHelper::getTopicSystemRestartRequest(void)      { return mqttTopicSystemRestartRequest_; }
const String& mqttHelper::getTopicChangeSensorCalib(void)         { return mqttTopicChangeSensorCalib_; }
const String& mqttHelper::getTopicChangeHysteresis(void)          { return mqttTopicChangeHysteresis_; }
const String& mqttHelper::getTopicTargetTempCmd(void)             { return mqttTopicTargetTempCmd_; }
const String& mqttHelper::getTopicThermostatModeCmd(void)         { return mqttTopicThermostatModeCmd_; }
const String& mqttHelper::getTopicHassDiscoveryClimate(void)      { return mqttTopicDiscoveryClimate_; }
const String& mqttHelper::getTopicData(void)                      { return mqttTopicData_; }
const String& mqttHelper::getTopicOutsideTemperature(void)        { return mqttTopicOutsideTemperature_; }
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
