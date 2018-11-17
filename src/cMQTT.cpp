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

void mqttHelper::init()
{
   mqttPrefix              = "homeassistant/";
   mqttNodeId              = "unknown";                     // to be set by SPIFFS_INIT before mqttHelper setup
   mqttObjectId            = "/thermostat";
   mqttCompClimate         = "climate/";
   mqttCompBinarySensor    = "binary_sensor/";
   mqttCompSensor          = "sensor/";
   mqttCompSwitch          = "switch/";
   mqttDiscoveryTrigger    = "homeassistant/discover";
   mqttGeneralBaseTopic    = "";                            // built in buildTopics

   nameChanged = false;
}

void mqttHelper::setup(void)
{
   mqttHelper::buildTopics();
}

void mqttHelper::buildTopics(void)
{
   mqttGeneralBaseTopic =        mqttPrefix + mqttCompClimate + loweredMqttNodeId + mqttObjectId;  // used for all topics except discovery of the non climate components
// sub
   mqttUpdateFirmware =          "/updateFirmware";
   mqttChangeName =              "/changeName";
   mqttSystemRestartRequest =    "/systemRestartRequest";
   mqttChangeSensorCalib =       "/changeSensorCalib";
   mqttTargetTempCmd =           "/targetTempCmd";
   mqttThermostatModeCmd =       "/thermostatModeCmd";
//pub
   mqttUpdateFirmwareAccepted =  "/updateFirmwareAccepted";
   mqttWill =                    "/availability";
   mqttHassDiscoveryTopic =      "/config";
   mqttData =                    "/state";
}

String mqttHelper::buildStateJSON(String temp, String humid, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware)
{
   String JSON = \
   "{\n" \
   "  \"name\":\"" + mqttNodeId + "\",\n" \
   "  \"dev_cla\":\"climate\",\n" \
   "  \"mode\":\"" + thermoMode + "\",\n" \
   "  \"state\":\"" + actState + "\",\n" \
   "  \"target_temp\":\"" + tarTemp + "\",\n" \
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

String mqttHelper::buildHassDiscoveryClimate(void)
{
   String JSON = \
   "{\n" \
   "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
   "  \"name\":\"" + mqttNodeId + "\",\n" \
   "  \"dev_cla\":\"climate\",\n" \
   "  \"mode_cmd_t\":\"~" + mqttThermostatModeCmd + "\",\n" \
   "  \"mode_stat_t\":\"~" + mqttData + "\",\n" \
   "  \"mode_stat_tpl\":\"{{value_json.mode}}\",\n" \
   "  \"stat_t\":\"~" + mqttData + "\",\n" \
   "  \"stat_val_tpl\":\"{{value_json.state}}\",\n" \
   "  \"avty_t\":\"~" + mqttWill + "\",\n" \
   "  \"pl_avail\":\"online\",\n" \
   "  \"pl_not_avail\":\"offline\",\n" \
   "  \"temp_cmd_t\":\"~" + mqttTargetTempCmd + "\",\n" \
   "  \"temp_stat_t\":\"~" + mqttData + "\",\n" \
   "  \"temp_stat_tpl\":\"{{value_json.target_temp}}\",\n" \
   "  \"curr_temp_t\":\"~" + mqttData + "\",\n" \
   "  \"current_temperature_template\":\"{{value_json.current_temp}}\",\n" \
   "  \"min_temp\":\"15\",\n" \
   "  \"max_temp\":\"25\",\n" \
   "  \"temp_step\":\"0.5\",\n" \
   "  \"unit_of_meas\":\"°C\",\n" \
   "  \"modes\":[\"heat\",\"off\"]\n" \
   "}";

   return (JSON);
}

String mqttHelper::buildHassDiscoveryBinarySensor(void)
{
   String JSON = \
   "{\n" \
   "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
   "  \"name\":\"Sensorfehler " + mqttNodeId + "\",\n" \
   "  \"dev_cla\":\"problem\",\n" \
   "  \"stat_t\":\"~" + mqttData + "\",\n" \
   "  \"val_tpl\":\"{{value_json.sens_status}}\",\n" \
   "  \"pl_on\":\"1\",\n" \
   "  \"pl_off\":\"0\",\n" \
   "  \"avty_t\":\"~" + mqttWill + "\",\n" \
   "  \"pl_avail\":\"online\",\n" \
   "  \"pl_not_avail\":\"offline\"\n" \
   "}";

   return (JSON);
}

String mqttHelper::buildHassDiscoverySensor(sensor_t sensor)
{
   String JSON = "void";

   switch (sensor)
   {
      case sTemp:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Temperatur " + mqttNodeId + "\",\n" \
         "  \"dev_cla\":\"temperature\",\n" \
         "  \"stat_t\":\"~" + mqttData + "\",\n" \
         "  \"val_tpl\":\"{{value_json.current_temp}}\",\n" \
         "  \"unit_of_meas\":\"°C\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      case sHum:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Luftfeuchtigkeit " + mqttNodeId + "\",\n" \
         "  \"dev_cla\":\"humidity\",\n" \
         "  \"stat_t\":\"~" + mqttData + "\",\n" \
         "  \"val_tpl\":\"{{value_json.humidity}}\",\n" \
         "  \"unit_of_meas\":\"%\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      case sIP:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"IP " + mqttNodeId + "\",\n" \
         "  \"stat_t\":\"~" + mqttData + "\",\n" \
         "  \"val_tpl\":\"{{value_json.ip}}\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      case sCalibF:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Skalierung " + mqttNodeId + "\",\n" \
         "  \"stat_t\":\"~" + mqttData + "\",\n" \
         "  \"val_tpl\":\"{{value_json.sens_scale}}\",\n" \
         "  \"unit_of_meas\":\"%\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      case sCalibO:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Offset " + mqttNodeId + "\",\n" \
         "  \"stat_t\":\"~" + mqttData + "\",\n" \
         "  \"val_tpl\":\"{{value_json.sens_offs}}\",\n" \
         "  \"unit_of_meas\":\"°C\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      case sFW:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Firmware " + mqttNodeId + "\",\n" \
         "  \"stat_t\":\"~" + mqttData + "\",\n" \
         "  \"val_tpl\":\"{{value_json.fw}}\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      default:
      break;
   }

   return (JSON);
}

String mqttHelper::buildHassDiscoverySwitch(switch_t switches)
{
   String JSON = "void";

   switch (switches)
   {
      case swReset:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Neustart " + mqttNodeId + "\",\n" \
         "  \"dev_cla\":\"temperature\",\n" \
         "  \"cmd_t\":\"~" + mqttSystemRestartRequest + "\",\n" \
         "  \"stat_t\":\"~" + mqttSystemRestartResponse + "\",\n" \
         "  \"pl_on\":\"true\",\n" \
         "  \"pl_off\":\"false\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      case swUpdate:
      {
         JSON = \
         "{\n" \
         "  \"~\":\"" + mqttGeneralBaseTopic + "\",\n" \
         "  \"name\":\"Firmwareupdate " + mqttNodeId + "\",\n" \
         "  \"cmd_t\":\"~" + mqttUpdateFirmware + "\",\n" \
         "  \"stat_t\":\"~" + mqttUpdateFirmwareAccepted + "\",\n" \
         "  \"pl_on\":\"true\",\n" \
         "  \"pl_off\":\"false\",\n" \
         "  \"avty_t\":\"~" + mqttWill + "\",\n" \
         "  \"pl_avail\":\"online\",\n" \
         "  \"pl_not_avail\":\"offline\"\n" \
         "}";
      }
      break;
      default:
      break;
   }

   return (JSON);
}

void mqttHelper::changeName(String value)
{
   if(value != mqttNodeId)
   {
      mqttNodeId = value;
      loweredMqttNodeId = value;
      loweredMqttNodeId.toLowerCase();
      mqttHelper::buildTopics();
      nameChanged = true;
   }
}

void mqttHelper::setName(String value)
{
   if(value != mqttNodeId)
   {
      mqttNodeId = value;
      loweredMqttNodeId = value;
      loweredMqttNodeId.toLowerCase();
      mqttHelper::buildTopics();
      nameChanged = false;
   }
}

void   mqttHelper::resetNameChanged(void)                         { nameChanged = false; }
bool   mqttHelper::getNameChanged(void)                           { return nameChanged; }
String mqttHelper::getName(void)                                  { return mqttNodeId; }
String mqttHelper::getLoweredName(void)                           { return loweredMqttNodeId; }
String mqttHelper::getTopicUpdateFirmware(void)                   { return mqttGeneralBaseTopic + mqttUpdateFirmware;  }
String mqttHelper::getTopicUpdateFirmwareAccepted(void)           { return mqttGeneralBaseTopic + mqttUpdateFirmwareAccepted; }
String mqttHelper::getTopicChangeName(void)                       { return mqttGeneralBaseTopic + mqttChangeName; }
String mqttHelper::getTopicLastWill(void)                         { return mqttGeneralBaseTopic + mqttWill; }
String mqttHelper::getTopicSystemRestartRequest(void)             { return mqttGeneralBaseTopic + mqttSystemRestartRequest; }
String mqttHelper::getTopicChangeSensorCalib(void)                { return mqttGeneralBaseTopic + mqttChangeSensorCalib; }
String mqttHelper::getTopicTargetTempCmd(void)                    { return mqttGeneralBaseTopic + mqttTargetTempCmd; }
String mqttHelper::getTopicThermostatModeCmd(void)                { return mqttGeneralBaseTopic + mqttThermostatModeCmd; }
String mqttHelper::getTopicHassDiscoveryClimate(void)             { return mqttGeneralBaseTopic + mqttHassDiscoveryTopic; }
String mqttHelper::getTopicHassDiscoveryBinarySensor(void)        { return mqttPrefix + mqttCompBinarySensor + loweredMqttNodeId + mqttObjectId + mqttHassDiscoveryTopic; }
String mqttHelper::getTopicData(void)                             { return mqttGeneralBaseTopic + mqttData; }
String mqttHelper::getTopicMqttDiscoveryTrigger(void)             { return mqttDiscoveryTrigger; }

String mqttHelper::getTopicHassDiscoverySensor(sensor_t sensor)
{
   String topic = "void";

   switch (sensor)
   {
      case sTemp:
         topic = mqttPrefix + mqttCompSensor + loweredMqttNodeId + mqttObjectId + "Temp" + mqttHassDiscoveryTopic;
      break;
      case sHum:
         topic = mqttPrefix + mqttCompSensor + loweredMqttNodeId + mqttObjectId + "Hum" + mqttHassDiscoveryTopic;
      break;
      case sIP:
         topic = mqttPrefix + mqttCompSensor + loweredMqttNodeId + mqttObjectId + "IP" + mqttHassDiscoveryTopic;
      break;
      case sCalibF:
         topic = mqttPrefix + mqttCompSensor + loweredMqttNodeId + mqttObjectId + "CalibF" + mqttHassDiscoveryTopic;
      break;
      case sCalibO:
         topic = mqttPrefix + mqttCompSensor + loweredMqttNodeId + mqttObjectId + "CalibO" + mqttHassDiscoveryTopic;
      break;
      case sFW:
         topic = mqttPrefix + mqttCompSensor + loweredMqttNodeId + mqttObjectId + "FW" + mqttHassDiscoveryTopic;
      break;
      default:
      break;
   }
  return topic; 
}

String mqttHelper::getTopicHassDiscoverySwitch(switch_t switches)
{
   String topic = "void";

   switch (switches)
   {
      case swReset:
         topic = mqttPrefix + mqttCompSwitch + loweredMqttNodeId + mqttObjectId + "Reset" + mqttHassDiscoveryTopic;
      break;
      case swUpdate:
         topic = mqttPrefix + mqttCompSwitch + loweredMqttNodeId + mqttObjectId + "Update" + mqttHassDiscoveryTopic;
      break;
      default:
      break;
   }
  return topic; 
}