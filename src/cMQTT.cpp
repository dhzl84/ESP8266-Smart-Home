#include "cMQTT.h"
// The Home Asistant discovery topic need to follow a specific format:
//
//     <discovery_prefix>/<component>/[<node_id>/]<object_id>/<>
//
// <components> may be climate, sensors, etc. like defined by HA
// <node_id> (Optional): ID of the node providing the topic.
// <object_id>: The ID of the device. This is only to allow for separate topics for each device and is not used for the entity_id.


mqttHelper::mqttHelper()  {
  mqttTriggerDiscovery        = false;
  mqttTriggerUndiscover       = false;
  mqttPrefix                  = "homeassistant/";
  mqttObjectId                = "/thermostat";
  mqttcompDevice              = "climate/";
  mqttCompBinarySensor        = "binary_sensor/";
  mqttCompSensor              = "sensor/";
  mqttCompSwitch              = "switch/";
  mqttDeviceName              = "unknown";
  mqttUpdateFirmware          = "/updateFirmware";
  mqttChangeName              = "/changeName";
  mqttSystemRestartRequest    = "/systemRestartRequest";
  mqttChangeSensorCalib       = "/changeSensorCalib";
  mqttTargetTempCmd           = "/targetTempCmd";
  mqttThermostatModeCmd       = "/thermostatModeCmd";
  mqttChangeHysteresis        = "/changeHysteresis";
  mqttUpdateFirmwareAccepted  = "/updateFirmwareAccepted";
  mqttWill                    = "/availability";
  mqttHassDiscoveryTopic      = "/config";
  mqttData                    = "/state";
  mqttNodeId                  = "tbd";            // to be set by SPIFFS_INIT before mqttHelper setup
  mqttGeneralBaseTopic        = "tbd";            // to be set by buildBaseTopic() after mqttNodeId is defined
}
mqttHelper::~mqttHelper() {}

void mqttHelper::setup() {
  mqttNodeId              = String(ESP.getChipId(), HEX);
  buildBaseTopic();
}

void mqttHelper::buildBaseTopic(void) {
  mqttGeneralBaseTopic =        mqttPrefix + mqttcompDevice + mqttNodeId + mqttObjectId;  // used for all topics except discovery of the non climate components
}

String mqttHelper::buildStateJSON(String name, String temp, String humid, String hysteresis, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware) {
  String JSON = \
  "{\n" \
  "  \"name\":\"" + name + "\",\n" \
  "  \"dev\":\"" + mqttNodeId + "\",\n" \
  "  \"mode\":\"" + thermoMode + "\",\n" \
  "  \"state\":\"" + actState + "\",\n" \
  "  \"target_temp\":\"" + tarTemp + "\",\n" \
  "  \"current_temp\":\"" + temp + "\",\n" \
  "  \"humidity\":\"" + humid + "\",\n" \
  "  \"hysteresis\":\"" + hysteresis + "\",\n" \
  "  \"sens_status\":\"" + sensError + "\",\n" \
  "  \"sens_scale\":\"" + calibF + "\",\n" \
  "  \"sens_offs\":\"" + calibO + "\",\n" \
  "  \"fw\":\"" + firmware + "\",\n" \
  "  \"ip\":\"" + ip + "\"\n "\
  "}";
  return (JSON);
}

String mqttHelper::buildHassDiscoveryClimate(String name, String firmware) {
  String JSON = \
  "{\n" \
  "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
  "  \"name\":\"" + name + "\",\n" \
  "  \"mode_cmd_t\":\"~" + mqttThermostatModeCmd + "\",\n" \
  "  \"mode_stat_t\":\"~" + mqttData + "\",\n" \
  "  \"mode_stat_tpl\":\"{{value_json.mode}}\",\n" \
  "  \"avty_t\":\"~" + mqttWill + "\",\n" \
  "  \"pl_avail\":\"online\",\n" \
  "  \"pl_not_avail\":\"offline\",\n" \
  "  \"temp_cmd_t\":\"~" + mqttTargetTempCmd + "\",\n" \
  "  \"temp_stat_t\":\"~" + mqttData + "\",\n" \
  "  \"temp_stat_tpl\":\"{{value_json.target_temp}}\",\n" \
  "  \"curr_temp_t\":\"~" + mqttData + "\",\n" \
  "  \"curr_temp_tpl\":\"{{value_json.current_temp}}\",\n" \
  "  \"min_temp\":\"15\",\n" \
  "  \"max_temp\":\"25\",\n" \
  "  \"temp_step\":\"0.5\",\n" \
  "  \"modes\":[\"heat\",\"off\"],\n" \
  "  \"json_attr_t\":\"~" + mqttData + "\",\n" \
  "  \"uniq_id\":\"" + mqttNodeId + "_climate\",\n" \
  "  \"dev\" : { \n" \
  "    \"ids\":[\"" + mqttNodeId + "\"],\n" \
  "    \"mdl\":\"ESP8266 Thermostat\",\n" \
  "    \"name\":\"" + name + "\",\n" \
  "    \"sw\":\"" + firmware + "\",\n" \
  "    \"mf\":\"dhzl84\"\n" \
  "  }\n" \
  "}";

  return (JSON);
}

String mqttHelper::buildHassDiscoverySensor(String name, sensor_t sensor) {
  String JSON = "void";

  switch (sensor) {
    case sTemp:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Temperatur " + name + "\",\n" \
      "  \"dev_cla\":\"temperature\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.current_temp}}\",\n" \
      "  \"unit_of_meas\":\"°C\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"json_attr_t\":\"~" + mqttData + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_sensTemp\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case sHum:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Luftfeuchtigkeit " + name + "\",\n" \
      "  \"dev_cla\":\"humidity\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.humidity}}\",\n" \
      "  \"unit_of_meas\":\"%\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"json_attr_t\":\"~" + mqttData + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_sensHum\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    default:
    break;
  }

  return (JSON);
}

String mqttHelper::buildHassDiscoverySwitch(String name, switch_t switches) {
  String JSON = "void";

  switch (switches) {
    case swRestart:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Neustart " + name + "\",\n" \
      "  \"cmd_t\":\"~" + mqttSystemRestartRequest + "\",\n" \
      "  \"stat_t\":\"~" + mqttSystemRestartResponse + "\",\n" \
      "  \"pl_on\":\"true\",\n" \
      "  \"pl_off\":\"false\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"qos\":\"1\",\n" \
      "  \"json_attr_t\":\"~" + mqttData + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_swRestart\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case swUpdate:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Firmwareupdate " + name + "\",\n" \
      "  \"cmd_t\":\"~" + mqttUpdateFirmware + "\",\n" \
      "  \"stat_t\":\"~" + mqttUpdateFirmwareAccepted + "\",\n" \
      "  \"pl_on\":\"true\",\n" \
      "  \"pl_off\":\"false\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"qos\":\"1\",\n" \
      "  \"json_attr_t\":\"~" + mqttData + "\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_swUpdate\",\n" \
      "  \"dev\" : { \n" \
      "    \"ids\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    default:
    break;
  }

  return (JSON);
}

String mqttHelper::getTopicUpdateFirmware(void)                   { return mqttGeneralBaseTopic + mqttUpdateFirmware;  }
String mqttHelper::getTopicUpdateFirmwareAccepted(void)           { return mqttGeneralBaseTopic + mqttUpdateFirmwareAccepted; }
String mqttHelper::getTopicChangeName(void)                       { return mqttGeneralBaseTopic + mqttChangeName; }
String mqttHelper::getTopicLastWill(void)                         { return mqttGeneralBaseTopic + mqttWill; }
String mqttHelper::getTopicSystemRestartRequest(void)             { return mqttGeneralBaseTopic + mqttSystemRestartRequest; }
String mqttHelper::getTopicChangeSensorCalib(void)                { return mqttGeneralBaseTopic + mqttChangeSensorCalib; }
String mqttHelper::getTopicChangeHysteresis(void)                 { return mqttGeneralBaseTopic + mqttChangeHysteresis; }
String mqttHelper::getTopicTargetTempCmd(void)                    { return mqttGeneralBaseTopic + mqttTargetTempCmd; }
String mqttHelper::getTopicThermostatModeCmd(void)                { return mqttGeneralBaseTopic + mqttThermostatModeCmd; }
String mqttHelper::getTopicHassDiscoveryClimate(void)             { return mqttGeneralBaseTopic + mqttHassDiscoveryTopic; }
String mqttHelper::getTopicData(void)                             { return mqttGeneralBaseTopic + mqttData; }
bool   mqttHelper::getTriggerDiscovery(void)                      { return mqttTriggerDiscovery; }
void   mqttHelper::setTriggerDiscovery(bool discover)             { mqttTriggerDiscovery = discover; }
bool   mqttHelper::getTriggerUndiscover(void)                     { return mqttTriggerUndiscover; }
void   mqttHelper::setTriggerUndiscover(bool undiscover)          { mqttTriggerUndiscover = undiscover; }
String mqttHelper::getTopicHassDiscoveryBinarySensor(binarySensor_t binarySensor) {
  String topic = "void";

  switch (binarySensor) {
    case bsSensFail:
    {
      topic = mqttPrefix + mqttCompBinarySensor + mqttNodeId + mqttObjectId + "SensFail" + mqttHassDiscoveryTopic;
    }
    break;

    case bsState:
    {
      topic = mqttPrefix + mqttCompBinarySensor + mqttNodeId + mqttObjectId + "State" + mqttHassDiscoveryTopic;
    }
    break;
  }
  return (topic);
}

String mqttHelper::getTopicHassDiscoverySensor(sensor_t sensor) {
  String topic = "void";

  switch (sensor) {
    case sTemp:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "Temp" + mqttHassDiscoveryTopic;
    break;
    case sHum:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "Hum" + mqttHassDiscoveryTopic;
    break;
    case sIP:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "IP" + mqttHassDiscoveryTopic;
    break;
    case sCalibF:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "CalibF" + mqttHassDiscoveryTopic;
    break;
    case sCalibO:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "CalibO" + mqttHassDiscoveryTopic;
    break;
    case sFW:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "FW" + mqttHassDiscoveryTopic;
    break;
    case sHysteresis:
      topic = mqttPrefix + mqttCompSensor + mqttNodeId + mqttObjectId + "Hysteresis" + mqttHassDiscoveryTopic;
    break;
    default:
    break;
  }
  return topic;
}

String mqttHelper::getTopicHassDiscoverySwitch(switch_t switches) {
  String topic = "void";

  switch (switches) {
    case swRestart:
      topic = mqttPrefix + mqttCompSwitch + mqttNodeId + mqttObjectId + "Reset" + mqttHassDiscoveryTopic;
    break;
    case swUpdate:
      topic = mqttPrefix + mqttCompSwitch + mqttNodeId + mqttObjectId + "Update" + mqttHassDiscoveryTopic;
    break;
    default:
    break;
  }
  return topic;
}
