#include "cMQTT.h"
// The Home Asistant discovery topic need to follow a specific format:
//
//     <discovery_prefix>/<component>/[<node_id>/]<object_id>/<>
//
// <components> may be climate, sensors, etc. like defined by HA
// <node_id> (Optional): ID of the node providing the topic.
// <object_id>: The ID of the device. This is only to allow for separate topics for each device and is not used for the entity_id.


mqttHelper::mqttHelper()  { mqttHelper::init(); }
mqttHelper::~mqttHelper() {}

void mqttHelper::init() {
}

void mqttHelper::setup() {
  mqttPrefix              = "homeassistant/";
  mqttNodeId              = String(ESP.getChipId(), HEX);                     // to be set by SPIFFS_INIT before mqttHelper setup
  mqttObjectId            = "/sensor";
  mqttComp                = "sensor/";
  mqttCompBinarySensor    = "binary_sensor/";
  mqttCompSensor          = "sensor/";
  mqttCompSwitch          = "switch/";
  mqttDeviceName          = "unknown";
  buildTopics();
}

void mqttHelper::buildTopics(void) {
  mqttGeneralBaseTopic =        mqttPrefix + mqttComp + mqttNodeId + mqttObjectId;  // used for all topics except discovery of the non climate components
// sub
  mqttUpdateFirmware =          "/updateFirmware";
  mqttChangeName =              "/changeName";
  mqttSystemRestartRequest =    "/systemRestartRequest";
  mqttChangeSensorCalib =       "/changeSensorCalib";
  mqttTargetTempCmd =           "/targetTempCmd";
  mqttThermostatModeCmd =       "/thermostatModeCmd";
  mqttChangeHysteresis =        "/changeHysteresis";
// pub
  mqttUpdateFirmwareAccepted =  "/updateFirmwareAccepted";
  mqttWill =                    "/availability";
  mqttHassDiscoveryTopic =      "/config";
  mqttData =                    "/state";
}

String mqttHelper::buildStateJSON(String name, String temp, String humid, String sensError, String calibF, String calibO, String ip, String firmware) {
  String JSON = \
  "{\n" \
  "  \"name\":\"" + name + "\",\n" \
  "  \"device\":\"" + mqttNodeId + "\",\n" \
  "  \"current_temp\":\"" + temp + "\",\n" \
  "  \"humidity\":\"" + humid + "\",\n" \
  "  \"sens_status\":\"" + sensError + "\",\n" \
  "  \"sens_scale\":\"" + calibF + "\",\n" \
  "  \"sens_offs\":\"" + calibO + "\",\n" \
  "  \"fw\":\"" + firmware + "\",\n" \
  "  \"ip\":\"" + ip + "\"\n "\
  "}";
  return (JSON);
}

String mqttHelper::buildHassDiscoveryDevice(String name, String firmware) {
  String JSON = \
  "{\n" \
  "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
  "  \"name\":\"" + name + "\",\n" \
  "  \"stat_t\":\"~" + mqttData + "\",\n" \
  "  \"val_tpl\":\"{{value_json.current_temp}}\",\n" \
  "  \"unit_of_meas\":\"°C\",\n" \
  "  \"frc_upd\":\"true\",\n" \
  "  \"avty_t\":\"~" + mqttWill + "\",\n" \
  "  \"pl_avail\":\"online\",\n" \
  "  \"pl_not_avail\":\"offline\",\n" \
  "  \"uniq_id\":\"" + mqttNodeId + "_sensor\",\n" \
  "  \"device\" : { \n" \
  "    \"identifiers\":[\"" + mqttNodeId + "\"],\n" \
  "    \"name\":\"" + name + "\",\n" \
  "    \"sw_version\":\"" + firmware + "\",\n" \
  "    \"manufacturer\":\"dhzl84\"\n" \
  "  }\n" \
  "}";

  return (JSON);
}
String mqttHelper::buildHassDiscoveryBinarySensor(String name, binarySensor_t binarySensor) {
  String JSON;

  switch (binarySensor) {
    case bsSensFail:
    default:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Sensorfehler " + name + "\",\n" \
      "  \"dev_cla\":\"problem\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.sens_status}}\",\n" \
      "  \"pl_on\":\"1\",\n" \
      "  \"pl_off\":\"0\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_sensStatus\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
  }
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
      "  \"uniq_id\":\"" + mqttNodeId + "_sensTemp\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
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
      "  \"uniq_id\":\"" + mqttNodeId + "_sensHum\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case sIP:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"IP " + name + "\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.ip}}\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_ip\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case sCalibF:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Skalierung " + name + "\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.sens_scale}}\",\n" \
      "  \"unit_of_meas\":\"%\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_calibF\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case sCalibO:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Offset " + name + "\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.sens_offs}}\",\n" \
      "  \"unit_of_meas\":\"°C\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_calibO\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
    break;
    case sFW:
    {
      JSON = \
      "{\n" \
      "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
      "  \"name\":\"Firmware " + name + "\",\n" \
      "  \"stat_t\":\"~" + mqttData + "\",\n" \
      "  \"val_tpl\":\"{{value_json.fw}}\",\n" \
      "  \"avty_t\":\"~" + mqttWill + "\",\n" \
      "  \"pl_avail\":\"online\",\n" \
      "  \"pl_not_avail\":\"offline\",\n" \
      "  \"uniq_id\":\"" + mqttNodeId + "_fw\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
      "  }\n" \
      "}";
    }
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
      "  \"uniq_id\":\"" + mqttNodeId + "_swRestart\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
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
      "  \"uniq_id\":\"" + mqttNodeId + "_swUpdate\",\n" \
      "  \"device\" : { \n" \
      "    \"identifiers\":[\"" + mqttNodeId + "\"]\n" \
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
String mqttHelper::getTopicHassDiscoveryDevice(void)              { return mqttGeneralBaseTopic + mqttHassDiscoveryTopic; }
String mqttHelper::getTopicData(void)                             { return mqttGeneralBaseTopic + mqttData; }

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
