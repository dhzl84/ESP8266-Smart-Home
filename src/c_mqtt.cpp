#include "c_mqtt.h"
#include <ArduinoJson.h>  // Required for DynamicJsonDocument, JsonObject, JsonArray

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

int mqttHelper::buildStateJSON(char* buf, size_t size, const char* name, const char* temp, const char* humid, const char* hysteresis, const char* actState, const char* tarTemp, const char* sensError, const char* thermoMode, int16_t calibration_factor, const char* calibration_offset, const char* ip, const char* firmware) {

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
    Serial.println("State JSON truncated, name too long for the buffer");
    #endif
    written = -1;
  }
  return written;
}

/* 
 * buildHassDiscoveryClimate - Zero-allocation using ArduinoJson DynamicJsonDocument
 * Instead of building a String with repeated += operations (causes heap fragmentation),
 * we use DynamicJsonDocument which reuses buffer memory across calls. This prevents:
 *   - Heap fragmentation from frequent String allocations  
 *   - "Reallocating large String" warnings in ESP8266
 *   - Potential crashes under load from memory pressure
 */
String mqttHelper::buildHassDiscoveryClimate(String name, String firmware, String model) {
  // DynamicJsonDocument reuses the same buffer, avoiding heap fragmentation
  DynamicJsonDocument doc(1024);  // Increased from 512 - climate discovery can be larger
  
  JsonObject root = doc.to<JsonObject>();
  
  // Create device object FIRST (Home Assistant requires "device" with identifiers)
  JsonObject devObj = root.createNestedObject("device");  // HA expects "device", not "dev"
  
  // Populate device object immediately
  JsonArray idsArr = devObj.createNestedArray("identifiers");  // Home Assistant field name
  idsArr.add(mqttNodeId_);
  
  devObj["model"] = model + " Thermostat";
  devObj["name"] = "Thermostat " + name;
  devObj["manufacturer"] = "dhzl84";
  devObj["config_url"] = "http://" + name + "/";
  
  // Populate root properties
  root["~"] = mqttGeneralBaseTopic_;
  root["name"] = name;
  root["mode_cmd_t"] = "~" + mqttThermostatModeCmd_;
  root["mode_stat_t"] = "~" + mqttData_;
  root["mode_stat_tpl"] = "{{value_json.mode}}";
  root["avty_t"] = "~" + mqttWill_;
  root["pl_avail"] = "online";
  root["pl_not_avail"] = "offline";
  root["temp_cmd_t"] = "~" + mqttTargetTempCmd_;
  root["temp_stat_t"] = "~" + mqttData_;
  root["temp_stat_tpl"] = "{{value_json.target_temp}}";
  root["curr_temp_t"] = "~" + mqttData_;
  root["curr_temp_tpl"] = "{{value_json.current_temp}}";
  root["min_temp"] = "15";
  root["max_temp"] = "25";
  root["temp_step"] = "0.5";
  // Build modes array - ArduinoJson requires chaining properly
  JsonArray modesArr = root.createNestedArray("modes");
  modesArr.add("heat");
  modesArr.add("off");
  root["act_t"] = "~" + mqttData_;
  root["act_tpl"] = "{{value_json.state}}";
  root["json_attr_t"] = "~" + mqttData_;
  root["uniq_id"] = mqttNodeId_ + "_climate";
  root["ic"] = "mdi:thermostat-box";
  
  // Serialize to char buffer, then convert to String for MQTT publish (single allocation)
  char jsonBuffer[1200];  // Increased from 600 - larger payload needs more space
  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

/* 
 * buildHassDiscoveryBinarySensor - Zero-allocation using ArduinoJson DynamicJsonDocument
 */
String mqttHelper::buildHassDiscoveryBinarySensor(BinarySensor_t binarySensor) {
  DynamicJsonDocument doc(512);  // Increased from 256 - device object needs space
  
  JsonObject root = doc.to<JsonObject>();
  
  // Add device object with identifiers (Home Assistant requires at least one identifier)
  JsonObject devObj = root.createNestedObject("device");
  
  switch (binarySensor) {
    case kThermostatState:
    {
      root["~"] = mqttGeneralBaseTopic_;
      root["name"] = "State";
      root["dev_cla"] = "heat";
      root["stat_t"] = "~" + mqttData_;
      root["val_tpl"] = "{{value_json.state}}";
      root["pl_on"] = "heating";
      root["pl_off"] = "off";
      root["avty_t"] = "~" + mqttWill_;
      root["pl_avail"] = "online";
      root["pl_not_avail"] = "offline";
      root["json_attr_t"] = "~" + mqttData_;
      root["uniq_id"] = mqttNodeId_ + "_state";
      root["ic"] = "mdi:radiator";
      
      JsonArray idsArr = devObj.createNestedArray("identifiers");
      idsArr.add(mqttNodeId_);
      break;
    }
    default:
      return String("");  // No binary sensor discovery for this type
  }
  
  char jsonBuffer[800];
  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

/* 
 * buildHassDiscoverySensor - Zero-allocation using ArduinoJson DynamicJsonDocument
 */
String mqttHelper::buildHassDiscoverySensor(String name, Sensor_t sensor) {
  DynamicJsonDocument doc(600);  // Increased from 300 - device object needs space
  
  JsonObject root = doc.to<JsonObject>();
  
  // Add device object with identifiers (Home Assistant requires at least one identifier)
  JsonObject devObj = root.createNestedObject("device");
  
  switch (sensor) {
    case kTemp:
    {
      root["~"] = mqttGeneralBaseTopic_;
      root["name"] = "Temperatur " + name;
      root["dev_cla"] = "temperature";
      root["stat_t"] = "~" + mqttData_;
      root["val_tpl"] = "{{value_json.current_temp}}";
      root["unit_of_meas"] = "°C";
      root["avty_t"] = "~" + mqttWill_;
      root["pl_avail"] = "online";
      root["pl_not_avail"] = "offline";
      root["json_attr_t"] = "~" + mqttData_;
      root["uniq_id"] = mqttNodeId_ + "_sensTemp";
      root["ic"] = "mdi:thermometer";
      
      JsonArray idsArr = devObj.createNestedArray("identifiers");
      idsArr.add(mqttNodeId_);
      break;
    }
    case kHum:
    {
      root["~"] = mqttGeneralBaseTopic_;
      root["name"] = "Luftfeuchtigkeit " + name;
      root["dev_cla"] = "humidity";
      root["stat_t"] = "~" + mqttData_;
      root["val_tpl"] = "{{value_json.humidity}}";
      root["unit_of_meas"] = "%";
      root["avty_t"] = "~" + mqttWill_;
      root["pl_avail"] = "online";
      root["pl_not_avail"] = "offline";
      root["json_attr_t"] = "~" + mqttData_;
      root["uniq_id"] = mqttNodeId_ + "_sensHum";
      root["ic"] = "mdi:water-percent";
      
      JsonArray idsArr = devObj.createNestedArray("identifiers");
      idsArr.add(mqttNodeId_);
      break;
    }
    default:
      return String("");  // No discovery for this sensor type
  }
  
  char jsonBuffer[1000];
  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
}

/* 
 * buildHassDiscoveryButton - Zero-allocation using ArduinoJson DynamicJsonDocument
 */
String mqttHelper::buildHassDiscoveryButton(String name, Button_t buttons) {
  DynamicJsonDocument doc(512);  // Increased from 256 - device object needs space
  
  JsonObject root = doc.to<JsonObject>();
  
  // Add device object with identifiers (Home Assistant requires at least one identifier)
  JsonObject devObj = root.createNestedObject("device");
  
  switch (buttons) {
    case kRestart_button:
    {
      root["~"] = mqttGeneralBaseTopic_;
      root["name"] = "Neustart " + name;
      root["cmd_t"] = "~" + mqttSystemRestartRequest_;
      root["avty_t"] = "~" + mqttWill_;
      root["pl_avail"] = "online";
      root["pl_not_avail"] = "offline";
      root["qos"] = "1";
      root["json_attr_t"] = "~" + mqttData_;
      root["uniq_id"] = mqttNodeId_ + "_swRestart";
      root["ic"] = "mdi:restart";
      
      JsonArray idsArr = devObj.createNestedArray("identifiers");
      idsArr.add(mqttNodeId_);
      break;
    }
    case kUpdate_button:
    {
      root["~"] = mqttGeneralBaseTopic_;
      root["name"] = "Firmwareupdate " + name;
      root["cmd_t"] = "~" + mqttUpdateFirmware_;
      root["avty_t"] = "~" + mqttWill_;
      root["pl_avail"] = "online";
      root["pl_not_avail"] = "offline";
      root["qos"] = "1";
      root["json_attr_t"] = "~" + mqttData_;
      root["uniq_id"] = mqttNodeId_ + "_swUpdate";
      root["ic"] = "mdi:cellphone-arrow-down";
      
      JsonArray idsArr = devObj.createNestedArray("identifiers");
      idsArr.add(mqttNodeId_);
      break;
    }
    default:
      return String("");  // No discovery for this button type
  }
  
  char jsonBuffer[800];
  serializeJson(doc, jsonBuffer);
  return String(jsonBuffer);
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

/* 
 * mqttLastErrorToString - Zero-allocation error string lookup
 * Uses a static array instead of building String objects for each error code.
 * This prevents heap allocation on every MQTT callback.
 */
const char* mqttHelper::mqttLastErrorToString(int8_t last_error) {
  static const char* error_strings[] = {
    "LWMQTT_SUCCESS",                     // 0
    "LWMQTT_BUFFER_TOO_SHORT",            // -1
    "LWMQTT_VARNUM_OVERFLOW",             // -2
    "LWMQTT_NETWORK_FAILED_CONNECT",      // -3
    "LWMQTT_NETWORK_TIMEOUT",             // -4
    "LWMQTT_NETWORK_FAILED_READ",         // -5
    "LWMQTT_NETWORK_FAILED_WRITE",        // -6
    "LWMQTT_REMAINING_LENGTH_OVERFLOW",   // -7
    "LWMQTT_REMAINING_LENGTH_MISMATCH",   // -8
    "LWMQTT_MISSING_OR_WRONG_PACKET",     // -9
    "LWMQTT_CONNECTION_DENIED",           // -10
    "LWMQTT_FAILED_SUBSCRIPTION",         // -11
    "LWMQTT_SUBACK_ARRAY_OVERFLOW",       // -12
    "LWMQTT_PONG_TIMEOUT",                // -13
    "UNKNOWN"                             // default
  };
  
  if (last_error >= 0 && last_error < 14) {
    return error_strings[last_error];
  } else if (last_error < 0 && last_error > -14) {
    return error_strings[13 + (-last_error)];
  }
  return error_strings[14];
}
