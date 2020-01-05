#include "main.h"
#include "config.h"
#include "ArduinoJson.h"

/*
{
  "name":"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "state":"true",
  "tTemp":"200",
  "tHyst":"4",
  "calibF":"95",
  "calibO":"20",
  "ssid":"xxxxxxxxxxxxxxxx",
  "wifiPwd":"xxxxxxxxxxxxxxxxx",
  "mqttHost":"123.456.789.012",
  "mqttPort":"1234",
  "mqttUser":"xxxxxxxxxxxxx",
  "mqttPwd":"xxxxxxxxxxxxx",
  "updServer":"http://192.168.178.12:88/firmware/thermostat/firmware.bin",
  "sensUpdInterval":"20",
  "mqttPubCycle":"5",
  "sensor":"0"
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
  StaticJsonDocument<768> jsonDoc;

  DeserializationError error = deserializeJson(jsonDoc, file);

    if (error) {
      #ifdef CFG_DEBUG
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      #endif /* CFG_DEBUG */
      return;
  } else {
      serializeJsonPretty(jsonDoc, Serial);
      Serial.println();
  }

  file.close();

  if (jsonDoc.isNull()) {
    #ifdef CFG_DEBUG
    Serial.println("Failed to read file, using default configuration");
    #endif /* CFG_DEBUG */
  }

  // Copy values from the jsonDoc  to the Config, if the key doesn't exist, load the default config
  strlcpy(config.name,          jsonDoc["name"]                  | "unknown",         sizeof(config.name));
  config.mode =                 jsonDoc["mode"]                  | true;
  strlcpy(config.ssid,          jsonDoc["ssid"]                  | WIFI_SSID,         sizeof(config.ssid));
  strlcpy(config.wifiPwd,       jsonDoc["wifiPwd"]               | WIFI_PWD ,         sizeof(config.wifiPwd));
  strlcpy(config.mqttHost,      jsonDoc["mqttHost"]              | LOCAL_MQTT_HOST,   sizeof(config.mqttHost));
  config.mqttPort =             jsonDoc["mqttPort"]              | LOCAL_MQTT_PORT;
  strlcpy(config.mqttUser,      jsonDoc["mqttUser"]              | LOCAL_MQTT_USER,   sizeof(config.mqttUser));
  strlcpy(config.mqttPwd,       jsonDoc["mqttPwd"]               | LOCAL_MQTT_PWD,    sizeof(config.mqttPwd));
  config.tTemp =                jsonDoc["tTemp"]                 | 200;
  config.tHyst =                jsonDoc["tHyst"]                 | THERMOSTAT_HYSTERESIS;
  config.calibF =               jsonDoc["calibF"]                | 100;
  config.calibO =               jsonDoc["calibO"]                | 0;
  strlcpy(config.updServer,     jsonDoc["updServer"]             | DEVICE_BINARY, sizeof(config.updServer));
  config.sensUpdInterval =      jsonDoc["sensUpdInterval"]       | SENSOR_UPDATE_INTERVAL;
  config.mqttPubCycle =         jsonDoc["mqttPubCycle"]          | 5;
  config.inputMethod =          jsonDoc["inputMethod"]           | false;
  config.sensor =               jsonDoc["sensor"]                | cDHT22;
}

// Saves the configuration to a file
bool saveConfiguration(const configuration &config) {
  bool ret = true;
  bool writeFile = false;
  StaticJsonDocument<768> jsonDoc;
  StaticJsonDocument<768> jsonDocNew;

  if (SPIFFS.exists(filename)) {
    // Delete existing file, otherwise the configuration is appended to the file
    File file = SPIFFS.open(filename, "r");

    DeserializationError error = deserializeJson(jsonDoc, file);

    if (error) {
        #ifdef CFG_DEBUG
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(error.c_str());
        #endif
    }

    #ifdef CFG_DEBUG
    Serial.println("SPIFFS content:");
    serializeJsonPretty(jsonDoc, Serial);
    Serial.println();
    Serial.println("Check SPIFFS vs. current config, 0 is equal, 1 is diff.");
    Serial.print((config.name ==                 jsonDoc["name"]) ? false : true);
    Serial.print((config.mode ==                 jsonDoc["mode"]) ? false : true);
    Serial.print((config.ssid ==                 jsonDoc["ssid"]) ? false : true);
    Serial.print((config.wifiPwd ==              jsonDoc["wifiPwd"]) ? false : true);
    Serial.print((config.mqttHost ==             jsonDoc["mqttHost"]) ? false : true);
    Serial.print((config.mqttPort ==             jsonDoc["mqttPort"]) ? false : true);
    Serial.print((config.mqttUser ==             jsonDoc["mqttUser"]) ? false : true);
    Serial.print((config.mqttPwd ==              jsonDoc["mqttPwd"]) ? false : true);
    Serial.print((config.tTemp ==                jsonDoc["tTemp"]) ? false : true);
    Serial.print((config.tHyst ==                jsonDoc["tHyst"]) ? false : true);
    Serial.print((config.calibF ==               jsonDoc["calibF"]) ? false : true);
    Serial.print((config.calibO ==               jsonDoc["calibO"]) ? false : true);
    Serial.print((config.updServer ==            jsonDoc["updServer"]) ? false : true);
    Serial.print((config.sensUpdInterval ==      jsonDoc["sensUpdInterval"]) ? false : true);
    Serial.print((config.mqttPubCycle ==         jsonDoc["mqttPubCycle"]) ? false : true);
    Serial.print((config.inputMethod ==          jsonDoc["inputMethod"]) ? false : true);
    Serial.print((config.sensor ==               jsonDoc["sensor"]) ? false : true);
    Serial.println();
    #endif /* CFG_DEBUG */

    /* check if SPIFFS content is equal to avoid delete and write */

    writeFile |= (config.name ==                 jsonDoc["name"]) ? false : true;
    writeFile |= (config.mode ==                 jsonDoc["mode"]) ? false : true;
    writeFile |= (config.ssid ==                 jsonDoc["ssid"]) ? false : true;
    writeFile |= (config.wifiPwd ==              jsonDoc["wifiPwd"]) ? false : true;
    writeFile |= (config.mqttHost ==             jsonDoc["mqttHost"]) ? false : true;
    writeFile |= (config.mqttPort ==             jsonDoc["mqttPort"]) ? false : true;
    writeFile |= (config.mqttUser ==             jsonDoc["mqttUser"]) ? false : true;
    writeFile |= (config.mqttPwd ==              jsonDoc["mqttPwd"]) ? false : true;
    writeFile |= (config.tTemp ==                jsonDoc["tTemp"]) ? false : true;
    writeFile |= (config.tHyst ==                jsonDoc["tHyst"]) ? false : true;
    writeFile |= (config.calibF ==               jsonDoc["calibF"]) ? false : true;
    writeFile |= (config.calibO ==               jsonDoc["calibO"]) ? false : true;
    writeFile |= (config.updServer ==            jsonDoc["updServer"]) ? false : true;
    writeFile |= (config.sensUpdInterval ==      jsonDoc["sensUpdInterval"]) ? false : true;
    writeFile |= (config.mqttPubCycle ==         jsonDoc["mqttPubCycle"]) ? false : true;
    writeFile |= (config.inputMethod ==          jsonDoc["inputMethod"]) ? false : true;
    writeFile |= (config.sensor ==               jsonDoc["sensor"]) ? false : true;

    file.close();
  } else {
    /* file does not exist */
    writeFile = true;
  }

  /* if SPIFFS content differs current configuration, delete SPIFFS and write new content*/
  if (writeFile == true) {
    if (SPIFFS.remove(filename)) {
      #ifdef CFG_DEBUG
      Serial.println("Removing SPIFFS file succeeded");
      #endif /* CFG_DEBUG */
    } else {
      #ifdef CFG_DEBUG
      Serial.println("Removing SPIFFS file failed");
      #endif /* CFG_DEBUG */
    }

    File file = SPIFFS.open(filename, "w");

    jsonDocNew["name"] =                   config.name;
    jsonDocNew["mode"] =                   config.mode;
    jsonDocNew["ssid"] =                   config.ssid;
    jsonDocNew["wifiPwd"] =                config.wifiPwd;
    jsonDocNew["mqttHost"] =               config.mqttHost;
    jsonDocNew["mqttPort"] =               config.mqttPort;
    jsonDocNew["mqttUser"] =               config.mqttUser;
    jsonDocNew["mqttPwd"] =                config.mqttPwd;
    jsonDocNew["tTemp"] =                  config.tTemp;
    jsonDocNew["tHyst"] =                  config.tHyst;
    jsonDocNew["calibF"] =                 config.calibF;
    jsonDocNew["calibO"] =                 config.calibO;
    jsonDocNew["updServer"] =              config.updServer;
    jsonDocNew["sensUpdInterval"] =        config.sensUpdInterval;
    jsonDocNew["mqttPubCycle"] =           config.mqttPubCycle;
    jsonDocNew["inputMethod"] =            config.inputMethod;
    jsonDocNew["sensor"] =                 config.sensor;

    // Serialize JSON to file
    if (serializeJson(jsonDocNew, file) == 0) {
      ret = false;
      #ifdef CFG_DEBUG
      Serial.println("Failed to write to file");
      #endif /* CFG_DEBUG */
    } else {
      #ifdef CFG_DEBUG
      Serial.println("new SPIFFS content:");
      serializeJsonPretty(jsonDocNew, Serial);
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
      #ifdef CFG_DEBUG
      Serial.println("oh no, failed to open file: " + file);
      #endif
      /* TODO: Error handling */
    } else {
      fileContent = f.readStringUntil('\n');
      f.close();

      #ifdef CFG_DEBUG
      if (fileContent == "") {
        Serial.println("File " + file + " is empty");
      }
      #endif
    }
  }
  return fileContent;
}
