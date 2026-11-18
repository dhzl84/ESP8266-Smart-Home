#include "main.h"

#include "ArduinoJson.h"

/*
{
  "name":"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "state":"true",
  "tTemp":"200",
  "tHyst":"4",
  "calibF":"100",                           // factor in percent
  "calibO":"0",                             // offset in 0.1 *C
  "ssid":"xxxxxxxxxxxxxxxx",
  "wifiPwd":"xxxxxxxxxxxxxxxxx",
  "mqttHost":"123.456.789.012",
  "mqttPort":"1234",
  "mqttUser":"xxxxxxxxxxxxx",
  "mqttPwd":"xxxxxxxxxxxxx",
  "updServer":"http://192.168.178.12:88/firmware/thermostat",
  "sensUpdInterval":"20",
  "mqttPubCycle":"5",
  "sensor":"0"
  "dispBrightn":"50",
  "discovery":0
  "tt":"CET-1CEST,M3.5.0,M10.5.0/3"
  "dispEna":1
  "autoUpd":1
}
*/

#define filename "/configJSON"

/* Capacity of the JSON document holding the configuration: 23 members * 16 byte slots plus all
   key and value strings, which are copied when reading from a file. With every string field at
   its maximum length (update server 255, name/ssid/passwords/host/user/timezone 63 chars each)
   this is ~1.2 kB. 768 bytes, as used before, failed with NoMemory for long values, so the
   config was loaded only partially and saved incompletely.
   The document lives on the heap: on the 4 kB cont stack that runs loop() a document of this
   size plus the LittleFS and MQTT call chains overflows the stack. */
#define cCONFIG_JSON_SIZE 1536

// Loads the configuration from a file
void loadConfiguration(Configuration &config) { // NOLINT: pass by reference
  // Open file for reading
  File file = FileSystem.open(filename, "r");
  delay(1);  // let the UART/WiFi interrupt work after LittleFS access

  DynamicJsonDocument jsonDoc(cCONFIG_JSON_SIZE);  /* heap, see cCONFIG_JSON_SIZE */
  if (!file) {
    #ifdef CFG_DEBUG
    Serial.println("Failed to open configuration, using defaults");
    #endif
    jsonDoc.clear();
  } else if (jsonDoc.capacity() < cCONFIG_JSON_SIZE) {
    file.close();
    delay(1);  // let the UART/WiFi interrupt work after LittleFS access
    #ifdef CFG_DEBUG
    Serial.println("Not enough heap for configuration, using defaults");
    #endif
    jsonDoc.clear();
  } else {
    DeserializationError error = deserializeJson(jsonDoc, file);
    file.close();
    delay(1);  // let the UART/WiFi interrupt work after LittleFS access

    if (error || jsonDoc.overflowed()) {
      #ifdef CFG_DEBUG
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error ? error.c_str() : "NoMemory");
      Serial.println("Using default configuration");
      #endif /* CFG_DEBUG */
      jsonDoc.clear();
    }
  }

  #ifdef CFG_DEBUG
  serializeJsonPretty(jsonDoc, Serial);
  Serial.println();
  #endif /* CFG_DEBUG */

  // Copy values from the jsonDoc to the Config, if the key doesn't exist, load the default config
  strlcpy(config.name,                          jsonDoc["name"]                  | getEspChipId().c_str(), sizeof(config.name));
  config.thermostat_mode =                      jsonDoc["mode"]                  | true;
  strlcpy(config.ssid,                          jsonDoc["ssid"]                  | WIFI_SSID,         sizeof(config.ssid));
  strlcpy(config.wifi_password,                 jsonDoc["wifiPwd"]               | WIFI_PWD ,         sizeof(config.wifi_password));
  strlcpy(config.mqtt_host,                     jsonDoc["mqttHost"]              | LOCAL_MQTT_HOST,   sizeof(config.mqtt_host));
  config.mqtt_port =                            jsonDoc["mqttPort"]              | LOCAL_MQTT_PORT;
  strlcpy(config.mqtt_user,                     jsonDoc["mqttUser"]              | LOCAL_MQTT_USER,   sizeof(config.mqtt_user));
  strlcpy(config.mqtt_password,                 jsonDoc["mqttPwd"]               | LOCAL_MQTT_PWD,    sizeof(config.mqtt_password));
  config.target_temperature =                   jsonDoc["tTemp"]                 | 200;
  config.temperature_hysteresis =               jsonDoc["tHyst"]                 | 4;
  config.calibration_factor =                   jsonDoc["calibF"]                | 100;
  config.calibration_offset =                   jsonDoc["calibO"]                | 0;
  strlcpy(config.update_server_address,         jsonDoc["updServer"]             | DEVICE_BINARY, sizeof(config.update_server_address));
  config.sensor_update_interval =               jsonDoc["sensUpdInterval"]       | 30;
  config.mqtt_publish_cycle =                   jsonDoc["mqttPubCycle"]          | 5;
  config.input_method =                         jsonDoc["inputMethod"]           | false;
  config.sensor_type =                          jsonDoc["sensor"]                | cDHT22;
  config.display_brightness =                   jsonDoc["dispBrightn"]           | 100;
  config.discovery_enabled =                    jsonDoc["discovery"]             | false;
  strlcpy(config.timezone,                      jsonDoc["tz"]                    | TIMEZONE, sizeof(config.timezone));
  config.display_enabled =                      jsonDoc["dispEna"]               | true;
  config.auto_update =                          jsonDoc["autoUpd"]               | true;
  config.planned_restart =                      jsonDoc["planRestart"]           | true;
}

// Saves the Configuration to a file
bool saveConfiguration(const Configuration &config) {
  bool ret = true;
  bool writeFile = false;
  DynamicJsonDocument jsonDoc(cCONFIG_JSON_SIZE);  /* heap, see cCONFIG_JSON_SIZE */

  if (jsonDoc.capacity() < cCONFIG_JSON_SIZE) {
    #ifdef CFG_DEBUG
    Serial.println("Not enough heap for configuration, file not written");
    #endif
    return false;
  }

  if (FileSystem.exists(filename)) {
    File file = FileSystem.open(filename, "r");
    delay(1);  // let the UART/WiFi interrupt work after LittleFS access
    if (!file) {
      #ifdef CFG_DEBUG
      Serial.println("Failed to open configuration, file not written");
      #endif
      return false;
    }

    DeserializationError error = deserializeJson(jsonDoc, file);
    file.close();
    delay(1);  // let the UART/WiFi interrupt work after LittleFS access

    if (error || jsonDoc.overflowed()) {
      /* Do not compare fields from a partial document. Keep the file and retry later. */
      jsonDoc.clear();
      #ifdef CFG_DEBUG
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error ? error.c_str() : "NoMemory");
      #endif
      return false;
    }

    #ifdef CFG_DEBUG
    Serial.println("FileSystem content:");
    serializeJsonPretty(jsonDoc, Serial);
    Serial.println();
    Serial.println("Check FileSystem vs. current config, 0 is equal, 1 is diff.");
    Serial.print((config.name ==                    jsonDoc["name"]) ? false : true);
    Serial.print((config.thermostat_mode ==         jsonDoc["mode"]) ? false : true);
    Serial.print((config.ssid ==                    jsonDoc["ssid"]) ? false : true);
    Serial.print((config.wifi_password ==           jsonDoc["wifiPwd"]) ? false : true);
    Serial.print((config.mqtt_host ==               jsonDoc["mqttHost"]) ? false : true);
    Serial.print((config.mqtt_port ==               jsonDoc["mqttPort"]) ? false : true);
    Serial.print((config.mqtt_user ==               jsonDoc["mqttUser"]) ? false : true);
    Serial.print((config.mqtt_password ==           jsonDoc["mqttPwd"]) ? false : true);
    Serial.print((config.target_temperature ==      jsonDoc["tTemp"]) ? false : true);
    Serial.print((config.temperature_hysteresis ==  jsonDoc["tHyst"]) ? false : true);
    Serial.print((config.calibration_factor ==      jsonDoc["calibF"]) ? false : true);
    Serial.print((config.calibration_offset ==      jsonDoc["calibO"]) ? false : true);
    Serial.print((config.update_server_address ==   jsonDoc["updServer"]) ? false : true);
    Serial.print((config.sensor_update_interval ==  jsonDoc["sensUpdInterval"]) ? false : true);
    Serial.print((config.mqtt_publish_cycle ==      jsonDoc["mqttPubCycle"]) ? false : true);
    Serial.print((config.input_method ==            jsonDoc["inputMethod"]) ? false : true);
    Serial.print((config.sensor_type ==             jsonDoc["sensor"]) ? false : true);
    Serial.print((config.display_brightness ==      jsonDoc["dispBrightn"]) ? false : true);
    Serial.print((config.discovery_enabled ==       jsonDoc["discovery"]) ? false : true);
    Serial.print((config.timezone ==                jsonDoc["tz"]) ? false : true);
    Serial.print((config.display_enabled ==         jsonDoc["dispEna"]) ? false : true);
    Serial.print((config.auto_update ==             jsonDoc["autoUpd"]) ? false : true);
    Serial.print((config.planned_restart ==         jsonDoc["planRestart"]) ? false : true);
    Serial.println();
    #endif /* CFG_DEBUG */

    /* check if FileSystem content is equal to avoid delete and write */

    writeFile |= (config.name ==                    jsonDoc["name"]) ? false : true;
    writeFile |= (config.thermostat_mode ==         jsonDoc["mode"]) ? false : true;
    writeFile |= (config.ssid ==                    jsonDoc["ssid"]) ? false : true;
    writeFile |= (config.wifi_password ==           jsonDoc["wifiPwd"]) ? false : true;
    writeFile |= (config.mqtt_host ==               jsonDoc["mqttHost"]) ? false : true;
    writeFile |= (config.mqtt_port ==               jsonDoc["mqttPort"]) ? false : true;
    writeFile |= (config.mqtt_user ==               jsonDoc["mqttUser"]) ? false : true;
    writeFile |= (config.mqtt_password ==           jsonDoc["mqttPwd"]) ? false : true;
    writeFile |= (config.target_temperature ==      jsonDoc["tTemp"]) ? false : true;
    writeFile |= (config.temperature_hysteresis ==  jsonDoc["tHyst"]) ? false : true;
    writeFile |= (config.calibration_factor ==      jsonDoc["calibF"]) ? false : true;
    writeFile |= (config.calibration_offset ==      jsonDoc["calibO"]) ? false : true;
    writeFile |= (config.update_server_address ==   jsonDoc["updServer"]) ? false : true;
    writeFile |= (config.sensor_update_interval ==  jsonDoc["sensUpdInterval"]) ? false : true;
    writeFile |= (config.mqtt_publish_cycle ==      jsonDoc["mqttPubCycle"]) ? false : true;
    writeFile |= (config.input_method ==            jsonDoc["inputMethod"]) ? false : true;
    writeFile |= (config.sensor_type ==             jsonDoc["sensor"]) ? false : true;
    writeFile |= (config.display_brightness ==      jsonDoc["dispBrightn"]) ? false : true;
    writeFile |= (config.discovery_enabled ==       jsonDoc["discovery"]) ? false : true;
    writeFile |= (config.timezone ==                jsonDoc["tz"]) ? false : true;
    writeFile |= (config.display_enabled ==         jsonDoc["dispEna"]) ? false : true;
    writeFile |= (config.auto_update ==             jsonDoc["autoUpd"]) ? false : true;
    writeFile |= (config.planned_restart ==         jsonDoc["planRestart"]) ? false : true;

  } else {
    /* file does not exist */
    writeFile = true;
  }

  /* if FileSystem content differs current configuration, delete FileSystem and write new content*/
  if (writeFile == true) {
    /* Build and validate the entire new document before replacing the current file. */
    jsonDoc.clear();

    jsonDoc["name"] =                   config.name;
    jsonDoc["mode"] =                   config.thermostat_mode;
    jsonDoc["ssid"] =                   config.ssid;
    jsonDoc["wifiPwd"] =                config.wifi_password;
    jsonDoc["mqttHost"] =               config.mqtt_host;
    jsonDoc["mqttPort"] =               config.mqtt_port;
    jsonDoc["mqttUser"] =               config.mqtt_user;
    jsonDoc["mqttPwd"] =                config.mqtt_password;
    jsonDoc["tTemp"] =                  config.target_temperature;
    jsonDoc["tHyst"] =                  config.temperature_hysteresis;
    jsonDoc["calibF"] =                 config.calibration_factor;
    jsonDoc["calibO"] =                 config.calibration_offset;
    jsonDoc["updServer"] =              config.update_server_address;
    jsonDoc["sensUpdInterval"] =        config.sensor_update_interval;
    jsonDoc["mqttPubCycle"] =           config.mqtt_publish_cycle;
    jsonDoc["inputMethod"] =            config.input_method;
    jsonDoc["sensor"] =                 config.sensor_type;
    jsonDoc["dispBrightn"] =            config.display_brightness;
    jsonDoc["discovery"] =              config.discovery_enabled;
    jsonDoc["tz"] =                     config.timezone;
    jsonDoc["dispEna"] =                config.display_enabled;
    jsonDoc["autoUpd"] =                config.auto_update;
    jsonDoc["planRestart"] =            config.planned_restart;

    if (jsonDoc.overflowed()) {
      /* incomplete document, keep the old file instead of storing half a configuration */
      ret = false;
      #ifdef CFG_DEBUG
      Serial.println("Configuration does not fit into JSON document, file not written");
      #endif /* CFG_DEBUG */
    } else {
      if (FileSystem.remove(filename)) {
        #ifdef CFG_DEBUG
        Serial.println("Removing FileSystem file succeeded");
        #endif /* CFG_DEBUG */
      } else {
        #ifdef CFG_DEBUG
        Serial.println("Removing FileSystem file failed");
        #endif /* CFG_DEBUG */
      }

      delay(1);  // let the UART/WiFi interrupt work after LittleFS access

      File file = FileSystem.open(filename, "w");
      delay(1);  // let the UART/WiFi interrupt work after LittleFS access

      // Serialize JSON to file
      if (serializeJson(jsonDoc, file) == 0) {
        ret = false;
        #ifdef CFG_DEBUG
        Serial.println("Failed to write to file");
        #endif /* CFG_DEBUG */
      } else {
        #ifdef CFG_DEBUG
        Serial.println("new FileSystem content:");
        serializeJsonPretty(jsonDoc, Serial);
        Serial.println();
        #endif /* CFG_DEBUG */
      }
      file.close();
      delay(1);  // let the UART/WiFi interrupt work after LittleFS access
    }
  } else {
    #ifdef CFG_DEBUG
    Serial.println("FileSystem content equals current config, no FileSystem write necessary");
    #endif /* CFG_DEBUG */
  }
  return ret;
}

/* read FileSystem */
String readSpiffs(String file) {
  String fileContent = "";

  if (FileSystem.exists(file)) {
    File f = FileSystem.open(file, "r");
    yield();  // yield after LittleFS open()
    if (!f) {
      #ifdef CFG_DEBUG
      Serial.println("oh no, failed to open file: " + file);
      #endif
      /* TODO: Error handling */
    } else {
      fileContent = f.readStringUntil('\n');
      f.close();
      delay(1);  // let the UART/WiFi interrupt work after LittleFS access

      #ifdef CFG_DEBUG
      if (fileContent == "") {
        Serial.println("File " + file + " is empty");
      }
      #endif
    }
  }
  return fileContent;
}
