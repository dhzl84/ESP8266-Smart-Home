#include "main.h"
#include "config.h"
#include "ArduinoJson.h"
/*
{
  "name":"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "tTemp":"200",
  "tHyst":"4",
  "calibF":"95",
  "calibO":"20",
  "ssid":"xxxxxxxxxxxxxxxx",
  "wifiPwd":"xxxxxxxxxxxxxxxxx",
  "mqtt_host":"123.456.789.012",
  "mqtt_port":"1234",
  "mqtt_user":"xxxxxxxxxxxxx",
  "mqtt_password":"xxxxxxxxxxxxx",
  "update_server_address":"http://192.168.178.12:88/firmware/thermostat/firmware.bin",
  "sensUpdInterval":"20",
  "mqtt_publish_cycle":"5"
}
*/

#define filename "/configJSON"

// Loads the configuration from a file
void loadConfiguration(configuration &config) { // NOLINT: pass by reference
  // Open file for reading
  File file = SPIFFS.open(filename, "r");

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonBuffer<768> jsonBuffer;
  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);
  file.close();

  if (root.success()) {
    #ifdef CFG_DEBUG
    root.prettyPrintTo(Serial);
    Serial.println();
    #endif /* CFG_DEBUG */
  } else {
    Serial.println("Failed to read file, using default configuration");
  }

  // Copy values from the JsonObject to the Config, if the key doesn't exist, load the default config
  strlcpy(config.name,          root["name"]                  | "unknown",         sizeof(config.name));
  strlcpy(config.ssid,          root["ssid"]                  | WIFI_SSID,         sizeof(config.ssid));
  strlcpy(config.wifiPwd,       root["wifiPwd"]               | WIFI_PWD ,         sizeof(config.wifiPwd));
  strlcpy(config.mqtt_host,      root["mqtt_host"]              | LOCAL_MQTT_HOST,   sizeof(config.mqtt_host));
  config.mqtt_port =             root["mqtt_port"]              | LOCAL_MQTT_PORT;
  strlcpy(config.mqtt_user,      root["mqtt_user"]              | LOCAL_MQTT_USER,   sizeof(config.mqtt_user));
  strlcpy(config.mqtt_password,       root["mqtt_password"]               | LOCAL_MQTT_PWD,    sizeof(config.mqtt_password));
  strlcpy(config.update_server_address,     root["update_server_address"]             | DEVICE_BINARY,     sizeof(config.update_server_address));
}

// Saves the configuration to a file
bool saveConfiguration(const configuration &config) {
  bool ret = true;
  bool writeFile = false;
  StaticJsonBuffer<768> jsonBuffer;

  if (SPIFFS.exists(filename)) {
    // Delete existing file, otherwise the configuration is appended to the file
    File file = SPIFFS.open(filename, "r");
    JsonObject &root = jsonBuffer.parseObject(file);

    #ifdef CFG_DEBUG
    Serial.println("SPIFFS content:");
    root.prettyPrintTo(Serial);
    Serial.println();
    Serial.println("Check SPIFFS vs. current config, 0 is equal, 1 is diff.");
    Serial.print((config.name ==                 root["name"]) ? false : true);
    Serial.print((config.ssid ==                 root["ssid"]) ? false : true);
    Serial.print((config.wifiPwd ==              root["wifiPwd"]) ? false : true);
    Serial.print((config.mqtt_host ==             root["mqtt_host"]) ? false : true);
    Serial.print((config.mqtt_port ==             root["mqtt_port"]) ? false : true);
    Serial.print((config.mqtt_user ==             root["mqtt_user"]) ? false : true);
    Serial.print((config.mqtt_password ==              root["mqtt_password"]) ? false : true);
    Serial.print((config.update_server_address ==            root["update_server_address"]) ? false : true);
    Serial.println();
    #endif /* CFG_DEBUG */

    /* check if SPIFFS content is equal to avoid delete and write */
    writeFile |= (config.name ==                 root["name"]) ? false : true;
    writeFile |= (config.ssid ==                 root["ssid"]) ? false : true;
    writeFile |= (config.wifiPwd ==              root["wifiPwd"]) ? false : true;
    writeFile |= (config.mqtt_host ==             root["mqtt_host"]) ? false : true;
    writeFile |= (config.mqtt_port ==             root["mqtt_port"]) ? false : true;
    writeFile |= (config.mqtt_user ==             root["mqtt_user"]) ? false : true;
    writeFile |= (config.mqtt_password ==              root["mqtt_password"]) ? false : true;
    writeFile |= (config.update_server_address ==            root["update_server_address"]) ? false : true;

    file.close();
  } else {
    /* file does not exist */
    writeFile = true;
  }

  /* if SPIFFS content differs current configuration, delete SPIFFS and write new content*/
  if (writeFile == true) {
    SPIFFS.remove(filename);

    File file = SPIFFS.open(filename, "w");

    JsonObject &root = jsonBuffer.createObject();

    root["name"] =                   config.name;
    root["ssid"] =                   config.ssid;
    root["wifiPwd"] =                config.wifiPwd;
    root["mqtt_host"] =               config.mqtt_host;
    root["mqtt_port"] =               config.mqtt_port;
    root["mqtt_user"] =               config.mqtt_user;
    root["mqtt_password"] =                config.mqtt_password;
    root["update_server_address"] =              config.update_server_address;

    // Serialize JSON to file
    if (root.printTo(file) == 0) {
      ret = false;
      #ifdef CFG_DEBUG
      Serial.println("Failed to write to file");
      #endif /* CFG_DEBUG */
    } else {
      #ifdef CFG_DEBUG
      Serial.println("new SPIFFS content:");
      root.prettyPrintTo(Serial);
      Serial.println();
      #endif /* CFG_DEBUG */
    }
    file.close();
  } else {
    #ifdef CFG_DEBUG
    Serial.println("SPIFFS content equals current config, no SPIFFS write necessary");
    #endif /* CFG_DEBUG */
  }
  return ret;
}

/* read SPIFFS */
String readSpiffs(String file) {
  String fileContent = "";

  if (SPIFFS.exists(file)) {
    File f = SPIFFS.open(file, "r");
    if (!f) {
      Serial.println("oh no, failed to open file: " + file);
      /* TODO: Error handling */
    } else {
      fileContent = f.readStringUntil('\n');
      f.close();

      if (fileContent == "") {
        Serial.println("File " + file + " is empty");
      }
    }
  }
  return fileContent;
}
