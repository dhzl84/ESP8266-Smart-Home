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
  "updServer":"http://192.168.178.12:88/firmware/thermostat/firmware.bin",
  "sensUpdInterval":"20",
  "mqttPubCycle":"5",
  "sensor":"0"
  "dispBrightn":"50",
  "discovery":0
  "utcOffset":1
  "dst":0,
  "dispEna":1
}
*/

#define filename "/configJSON"

// Loads the configuration from a file
void loadConfiguration(Configuration &config) { // NOLINT: pass by reference
  // Open file for reading
  File file = FileSystem.open(filename, "r");

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<768> jsonDoc;

  DeserializationError error = deserializeJson(jsonDoc, file);

  if (error) {
    #ifdef CFG_DEBUG
    mySerial.print(F("deserializeJson() failed with code "));
    mySerial.println(error.c_str());
    #endif /* CFG_DEBUG */
    return;
  } else {
      serializeJsonPretty(jsonDoc, mySerial);
      mySerial.println();
  }

  file.close();

  if (jsonDoc.isNull()) {
    #ifdef CFG_DEBUG
    mySerial.println("Failed to read file, using default configuration");
    #endif /* CFG_DEBUG */
  }

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
  config.utc_offset =                           jsonDoc["utcOffset"]             | 1;
  config.daylight_saving_time =                 jsonDoc["dst"]                   | false;
  config.display_enabled =                      jsonDoc["dispEna"]               | true;
}

// Saves the Configuration to a file
bool saveConfiguration(const Configuration &config) {
  bool ret = true;
  bool writeFile = false;
  StaticJsonDocument<768> jsonDoc;
  StaticJsonDocument<768> jsonDocNew;

  if (FileSystem.exists(filename)) {
    // Delete existing file, otherwise the configuration is appended to the file
    File file = FileSystem.open(filename, "r");

    DeserializationError error = deserializeJson(jsonDoc, file);

    if (error) {
        #ifdef CFG_DEBUG
        mySerial.print(F("deserializeJson() failed with code "));
        mySerial.println(error.c_str());
        #endif
    }

    #ifdef CFG_DEBUG
    mySerial.println("FileSystem content:");
    serializeJsonPretty(jsonDoc, mySerial);
    mySerial.println();
    mySerial.println("Check FileSystem vs. current config, 0 is equal, 1 is diff.");
    mySerial.print((config.name ==                    jsonDoc["name"]) ? false : true);
    mySerial.print((config.thermostat_mode ==         jsonDoc["mode"]) ? false : true);
    mySerial.print((config.ssid ==                    jsonDoc["ssid"]) ? false : true);
    mySerial.print((config.wifi_password ==           jsonDoc["wifiPwd"]) ? false : true);
    mySerial.print((config.mqtt_host ==               jsonDoc["mqttHost"]) ? false : true);
    mySerial.print((config.mqtt_port ==               jsonDoc["mqttPort"]) ? false : true);
    mySerial.print((config.mqtt_user ==               jsonDoc["mqttUser"]) ? false : true);
    mySerial.print((config.mqtt_password ==           jsonDoc["mqttPwd"]) ? false : true);
    mySerial.print((config.target_temperature ==      jsonDoc["tTemp"]) ? false : true);
    mySerial.print((config.temperature_hysteresis ==  jsonDoc["tHyst"]) ? false : true);
    mySerial.print((config.calibration_factor ==      jsonDoc["calibF"]) ? false : true);
    mySerial.print((config.calibration_offset ==      jsonDoc["calibO"]) ? false : true);
    mySerial.print((config.update_server_address ==   jsonDoc["updServer"]) ? false : true);
    mySerial.print((config.sensor_update_interval ==  jsonDoc["sensUpdInterval"]) ? false : true);
    mySerial.print((config.mqtt_publish_cycle ==      jsonDoc["mqttPubCycle"]) ? false : true);
    mySerial.print((config.input_method ==            jsonDoc["inputMethod"]) ? false : true);
    mySerial.print((config.sensor_type ==             jsonDoc["sensor"]) ? false : true);
    mySerial.print((config.display_brightness ==      jsonDoc["dispBrightn"]) ? false : true);
    mySerial.print((config.discovery_enabled ==       jsonDoc["discovery"]) ? false : true);
    mySerial.print((config.utc_offset ==              jsonDoc["utcOffset"]) ? false : true);
    mySerial.print((config.daylight_saving_time ==    jsonDoc["dst"]) ? false : true);
    mySerial.print((config.display_enabled ==         jsonDoc["dispEna"]) ? false : true);
    mySerial.println();
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
    writeFile |= (config.utc_offset ==              jsonDoc["utcOffset"]) ? false : true;
    writeFile |= (config.daylight_saving_time ==    jsonDoc["dst"]) ? false : true;
    writeFile |= (config.display_enabled ==         jsonDoc["dispEna"]) ? false : true;

    file.close();
  } else {
    /* file does not exist */
    writeFile = true;
  }

  /* if FileSystem content differs current configuration, delete FileSystem and write new content*/
  if (writeFile == true) {
    if (FileSystem.remove(filename)) {
      #ifdef CFG_DEBUG
      mySerial.println("Removing FileSystem file succeeded");
      #endif /* CFG_DEBUG */
    } else {
      #ifdef CFG_DEBUG
      mySerial.println("Removing FileSystem file failed");
      #endif /* CFG_DEBUG */
    }

    File file = FileSystem.open(filename, "w");

    jsonDocNew["name"] =                   config.name;
    jsonDocNew["mode"] =                   config.thermostat_mode;
    jsonDocNew["ssid"] =                   config.ssid;
    jsonDocNew["wifiPwd"] =                config.wifi_password;
    jsonDocNew["mqttHost"] =               config.mqtt_host;
    jsonDocNew["mqttPort"] =               config.mqtt_port;
    jsonDocNew["mqttUser"] =               config.mqtt_user;
    jsonDocNew["mqttPwd"] =                config.mqtt_password;
    jsonDocNew["tTemp"] =                  config.target_temperature;
    jsonDocNew["tHyst"] =                  config.temperature_hysteresis;
    jsonDocNew["calibF"] =                 config.calibration_factor;
    jsonDocNew["calibO"] =                 config.calibration_offset;
    jsonDocNew["updServer"] =              config.update_server_address;
    jsonDocNew["sensUpdInterval"] =        config.sensor_update_interval;
    jsonDocNew["mqttPubCycle"] =           config.mqtt_publish_cycle;
    jsonDocNew["inputMethod"] =            config.input_method;
    jsonDocNew["sensor"] =                 config.sensor_type;
    jsonDocNew["dispBrightn"] =            config.display_brightness;
    jsonDocNew["discovery"] =              config.discovery_enabled;
    jsonDocNew["utcOffset"] =              config.utc_offset;
    jsonDocNew["dst"] =                    config.daylight_saving_time;
    jsonDocNew["dispEna"] =                config.display_enabled;

    // Serialize JSON to file
    if (serializeJson(jsonDocNew, file) == 0) {
      ret = false;
      #ifdef CFG_DEBUG
      mySerial.println("Failed to write to file");
      #endif /* CFG_DEBUG */
    } else {
      #ifdef CFG_DEBUG
      mySerial.println("new FileSystem content:");
      serializeJsonPretty(jsonDocNew, mySerial);
      mySerial.println();
      #endif /* CFG_DEBUG */
    }
    file.close();
  } else {
    #ifdef CFG_DEBUG
    mySerial.println("FileSystem content equals current config, no FileSystem write necessary");
    #endif /* CFG_DEBUG */
  }
  return ret;
}

/* read FileSystem */
String readSpiffs(String file) {
  String fileContent = "";

  if (FileSystem.exists(file)) {
    File f = FileSystem.open(file, "r");
    if (!f) {
      #ifdef CFG_DEBUG
      mySerial.println("oh no, failed to open file: " + file);
      #endif
      /* TODO: Error handling */
    } else {
      fileContent = f.readStringUntil('\n');
      f.close();

      #ifdef CFG_DEBUG
      if (fileContent == "") {
        mySerial.println("File " + file + " is empty");
      }
      #endif
    }
  }
  return fileContent;
}
