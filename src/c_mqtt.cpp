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

int mqttHelper::buildStateJSON(char* buf, size_t size, const char* name, const char* temp, const char* humid, int16_t hysteresis, const char* actState, int16_t tarTemp, const char* sensError, const char* thermoMode, int16_t calibration_factor, int16_t calibration_offset, const char* ip, const char* firmware) {
  /* hysteresis and tarTemp are scaled by 10, so the decimals are split with integer math;
     no heap allocation: the JSON lives in the caller's buffer and is published from there */
  const int hysteresisWhole = hysteresis / 10;
  const int hysteresisFrac  = (hysteresis % 10 < 0) ? -(hysteresis % 10) : (hysteresis % 10);
  const int targetWhole     = tarTemp / 10;
  const int targetFrac      = (tarTemp % 10 < 0) ? -(tarTemp % 10) : (tarTemp % 10);

  int written = snprintf(buf, size,
    "{\n"
    "  \"name\":\"%s\",\n"
    "  \"dev\":\"%s\",\n"
    "  \"mode\":\"%s\",\n"
    "  \"state\":\"%s\",\n"
    "  \"target_temp\":\"%d.%d\",\n"
    "  \"current_temp\":\"%s\",\n"
    "  \"humidity\":\"%s\",\n"
    "  \"hysteresis\":\"%d.%d\",\n"
    "  \"sens_status\":\"%s\",\n"
    "  \"sens_scale\":\"%d\",\n"
    "  \"sens_offs\":\"%d\",\n"
    "  \"fw\":\"%s\",\n"
    "  \"ip\":\"%s\"\n"
    "}",
    name, mqttNodeId_.c_str(), thermoMode, actState, targetWhole, targetFrac,
    temp, humid, hysteresisWhole, hysteresisFrac, sensError,
    calibration_factor, calibration_offset, firmware, ip);

  if (written < 0 || static_cast<size_t>(written) >= size) {
    #ifdef CFG_DEBUG
    Serial.println("State JSON truncated, name too long for the buffer");
    #endif
    return -1;
  }
  return written;
}

String mqttHelper::buildHassDiscoveryClimate(String name, String firmware, String model) {
  String JSON = \
  "{\n" \
  "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
  "  \"name\":\"" + name + "\",\n" \
  "  \"mode_cmd_t\":\"~" + mqttThermostatModeCmd_ + "\",\n" \
  "  \"mode_stat_t\":\"~" + mqttData_ + "\",\n" \
  "  \"mode_stat_tpl\":\"{{value_json.mode}}\",\n" \
  "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
  "  \"pl_avail\":\"online\",\n" \
  "  \"pl_not_avail\":\"offline\",\n" \
  "  \"temp_cmd_t\":\"~" + mqttTargetTempCmd_ + "\",\n" \
  "  \"temp_stat_t\":\"~" + mqttData_ + "\",\n" \
  "  \"temp_stat_tpl\":\"{{value_json.target_temp}}\",\n" \
  "  \"curr_temp_t\":\"~" + mqttData_ + "\",\n" \
  "  \"curr_temp_tpl\":\"{{value_json.current_temp}}\",\n" \
  "  \"min_temp\":\"15\",\n" \
  "  \"max_temp\":\"25\",\n" \
  "  \"temp_step\":\"0.5\",\n" \
  "  \"modes\":[\"heat\",\"off\"],\n" \
  "  \"act_t\":\"~" + mqttData_ + "\",\n" \
  "  \"act_tpl\":\"{{value_json.state}}\",\n" \
  "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
  "  \"uniq_id\":\"" + mqttNodeId_ + "_climate\",\n" \
  "  \"ic\":\"mdi:thermostat-box\",\n" \
  "  \"dev\" : { \n" \
  "    \"ids\":[\"" + mqttNodeId_ + "\"],\n" \
  "    \"mdl\":\"" + model + " Thermostat\",\n" \
  "    \"name\":\"Thermostat " + name + "\",\n" \
  "    \"sw\":\"" + firmware + "\",\n" \
  "    \"mf\":\"dhzl84\",\n" \
  "    \"cu\":\"http://" + name + "/\"\n"
  "  }\n" \
  "}";

  return (JSON);
}

String mqttHelper::buildHassDiscoveryBinarySensor(BinarySensor_t binarySensor) {
  String JSON;

  switch (binarySensor) {
    case kThermostatState:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"State\",\n" \
      "  \"dev_cla\":\"heat\",\n" \
      "  \"stat_t\":\"~" + mqttData_ + "\",\n" \
      "  \"val_tpl\":\"{{value_json.state}}\",\n" \
      "  \"pl_on\":\"heating\",\n" \
      "  \"pl_off\":\"off\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_state\",\n" \
      "  \"ic\":\"mdi:radiator\",\n" \
      "  \"dev\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId_ + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    default:
    break;
  }
  return (JSON);
}

String mqttHelper::buildHassDiscoverySensor(String name, Sensor_t sensor) {
  String JSON;

  switch (sensor) {
    case kTemp:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"Temperatur " + name + "\",\n" \
      "  \"dev_cla\":\"temperature\",\n" \
      "  \"stat_t\":\"~" + mqttData_ + "\",\n" \
      "  \"val_tpl\":\"{{value_json.current_temp}}\",\n" \
      "  \"unit_of_meas\":\"°C\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_sensTemp\",\n" \
      "  \"ic\":\"mdi:thermometer\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId_ + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case kHum:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"Luftfeuchtigkeit " + name + "\",\n" \
      "  \"dev_cla\":\"humidity\",\n" \
      "  \"stat_t\":\"~" + mqttData_ + "\",\n" \
      "  \"val_tpl\":\"{{value_json.humidity}}\",\n" \
      "  \"unit_of_meas\":\"%\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_sensHum\",\n" \
      "  \"ic\":\"mdi:water-percent\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId_ + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    default:
    break;
  }

  return (JSON);
}

String mqttHelper::buildHassDiscoveryButton(String name, Button_t buttons) {
  String JSON;

  switch (buttons) {
    case kRestart_button:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"Neustart " + name + "\",\n" \
      "  \"cmd_t\":\"~" + mqttSystemRestartRequest_ + "\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"qos\":\"1\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_swRestart\",\n" \
      "  \"ic\":\"mdi:restart\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId_ + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case kUpdate_button:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"Firmwareupdate " + name + "\",\n" \
      "  \"cmd_t\":\"~" + mqttUpdateFirmware_ + "\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"qos\":\"1\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_swUpdate\",\n" \
      "  \"ic\":\"mdi:cellphone-arrow-down\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId_ + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    default:
    break;
  }

  return (JSON);
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
