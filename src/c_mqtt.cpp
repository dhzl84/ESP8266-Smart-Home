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
    mqttUpdateFirmwareAccepted_("/updateFirmwareAccepted"), \
    mqttChangeName_("/changeName"), \
    mqttWill_("/availability"), \
    mqttSystemRestartRequest_("/systemRestartRequest"), \
    mqttPrefix_("homeassistant/"), \
    mqttNodeId_("tbd"), \
    mqttDeviceName_("unknown"), \
    mqttCompDevice_("climate/"), \
    mqttCompSensor_("sensor/"), \
    mqttCompSwitch_("switch/"), \
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
  mqttNodeId_              = nodeId;
  buildBaseTopic();
}

void mqttHelper::buildBaseTopic(void) {
  mqttGeneralBaseTopic_ =        mqttPrefix_ + mqttCompDevice_ + mqttNodeId_ + mqttObjectId_;
}

String mqttHelper::buildStateJSON(String name, String temp, String humid, String hysteresis, String actState, String tarTemp, String sensError, String thermoMode, String calibration_factor, String calibration_offset, String ip, String firmware) {
  String JSON = \
  "{\n" \
  "  \"name\":\"" + name + "\",\n" \
  "  \"dev\":\"" + mqttNodeId_ + "\",\n" \
  "  \"mode\":\"" + thermoMode + "\",\n" \
  "  \"state\":\"" + actState + "\",\n" \
  "  \"target_temp\":\"" + tarTemp + "\",\n" \
  "  \"current_temp\":\"" + temp + "\",\n" \
  "  \"humidity\":\"" + humid + "\",\n" \
  "  \"hysteresis\":\"" + hysteresis + "\",\n" \
  "  \"sens_status\":\"" + sensError + "\",\n" \
  "  \"sens_scale\":\"" + calibration_factor + "\",\n" \
  "  \"sens_offs\":\"" + calibration_offset + "\",\n" \
  "  \"fw\":\"" + firmware + "\",\n" \
  "  \"ip\":\"" + ip + "\"\n "\
  "}";
  return (JSON);
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
  "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
  "  \"uniq_id\":\"" + mqttNodeId_ + "_climate\",\n" \
  "  \"dev\" : { \n" \
  "    \"ids\":[\"" + mqttNodeId_ + "\"],\n" \
  "    \"mdl\":\"" + model + " Thermostat\",\n" \
  "    \"name\":\"" + name + "\",\n" \
  "    \"sw\":\"" + firmware + "\",\n" \
  "    \"mf\":\"dhzl84\"\n" \
  "  }\n" \
  "}";

  return (JSON);
}

String mqttHelper::buildHassDiscoveryBinarySensor(String name, BinarySensor_t binarySensor) {
  String JSON;

  switch (binarySensor) {
    case kThermostatState:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"State " + name + "\",\n" \
      "  \"dev_cla\":\"heat\",\n" \
      "  \"stat_t\":\"~" + mqttData_ + "\",\n" \
      "  \"val_tpl\":\"{{value_json.state}}\",\n" \
      "  \"pl_on\":\"on\",\n" \
      "  \"pl_off\":\"off\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_state\",\n" \
      "  \"device\" : { \n" \
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
  String JSON = "void";

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

String mqttHelper::buildHassDiscoverySwitch(String name, Switch_t switches) {
  String JSON = "void";

  switch (switches) {
    case kRestart:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"Neustart " + name + "\",\n" \
      "  \"cmd_t\":\"~" + mqttSystemRestartRequest_ + "\",\n" \
      "  \"pl_on\":\"true\",\n" \
      "  \"pl_off\":\"false\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"qos\":\"1\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_swRestart\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId_ + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case kUpdate:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic_ + "\",\n" \
      "  \"name\":\"Firmwareupdate " + name + "\",\n" \
      "  \"cmd_t\":\"~" + mqttUpdateFirmware_ + "\",\n" \
      "  \"stat_t\":\"~" + mqttUpdateFirmwareAccepted_ + "\",\n" \
      "  \"pl_on\":\"true\",\n" \
      "  \"pl_off\":\"false\",\n" \
      "  \"avty_t\":\"~" + mqttWill_ + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"qos\":\"1\",\n" \
      "  \"json_attr_t\":\"~" + mqttData_ + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId_ + "_swUpdate\",\n" \
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
String mqttHelper::getTopicUpdateFirmwareAccepted(void)           { return mqttGeneralBaseTopic_ + mqttUpdateFirmwareAccepted_; }
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
bool   mqttHelper::getTriggerDiscovery(void)                      { return mqttTriggerDiscovery_; }
void   mqttHelper::setTriggerDiscovery(bool discover)             { mqttTriggerDiscovery_ = discover; }
bool   mqttHelper::getTriggerRemoveDiscovered(void)               { return mqttTriggerRemoveDiscovered_; }
void   mqttHelper::setTriggerRemoveDiscovered(bool undiscover)    { mqttTriggerRemoveDiscovered_ = undiscover; }
String mqttHelper::getTopicHassDiscoveryBinarySensor(BinarySensor_t binarySensor) {
  String topic = "void";

  switch (binarySensor) {
    case kSensFail:
    {
      topic = mqttPrefix_ + mqttCompBinarySensor_ + mqttNodeId_ + mqttObjectId_ + "SensFail" + mqttHassDiscoveryTopic_;
    }
    break;

    case kThermostatState:
    {
      topic = mqttPrefix_ + mqttCompBinarySensor_ + mqttNodeId_ + mqttObjectId_ + "State" + mqttHassDiscoveryTopic_;
    }
    break;
  }
  return (topic);
}

String mqttHelper::getTopicHassDiscoverySensor(Sensor_t sensor) {
  String topic = "void";

  switch (sensor) {
    case kTemp:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "Temp" + mqttHassDiscoveryTopic_;
    break;
    case kHum:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "Hum" + mqttHassDiscoveryTopic_;
    break;
    case kIP:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "IP" + mqttHassDiscoveryTopic_;
    break;
    case kCalibF:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "CalibF" + mqttHassDiscoveryTopic_;
    break;
    case kCalibO:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "CalibO" + mqttHassDiscoveryTopic_;
    break;
    case kFW:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "FW" + mqttHassDiscoveryTopic_;
    break;
    case kHysteresis:
      topic = mqttPrefix_ + mqttCompSensor_ + mqttNodeId_ + mqttObjectId_ + "Hysteresis" + mqttHassDiscoveryTopic_;
    break;
    default:
    break;
  }
  return topic;
}

String mqttHelper::getTopicHassDiscoverySwitch(Switch_t switches) {
  String topic = "void";

  switch (switches) {
    case kRestart:
      topic = mqttPrefix_ + mqttCompSwitch_ + mqttNodeId_ + mqttObjectId_ + "Reset" + mqttHassDiscoveryTopic_;
    break;
    case kUpdate:
      topic = mqttPrefix_ + mqttCompSwitch_ + mqttNodeId_ + mqttObjectId_ + "Update" + mqttHassDiscoveryTopic_;
    break;
    default:
    break;
  }
  return topic;
}
