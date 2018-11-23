#include "main.h"
#include "config.h"
#include "ArduinoJson.h"
/*
{
  "name":"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "tTemp":"200",
  "calibF":"95",
  "calibO":"20",
  "ssid":"xxxxxxxxxxxxxxxx",
  "wifiPwd":"xxxxxxxxxxxxxxxxx",
  "mqttHost":"123.456.789.012",
  "mqttPort":"1234",
  "mqttUser":"xxxxxxxxxxxxx",
  "mqttPwd":"xxxxxxxxxxxxx",
  "updServer":"http://192.168.178.12:88/firmware/thermostat/firmware.bin",
  "sensUpdInterval":"20000",
  "mqttPubCycleInterval":"5"
}
*/

#define filename "/configJSON"

// Loads the configuration from a file
void loadConfiguration(configuration &config)
{
   if (SPIFFS.exists(filename))
   {
      // Open file for reading
      File file = SPIFFS.open(filename, "r");

      // Allocate the memory pool on the stack.
      // Don't forget to change the capacity to match your JSON document.
      // Use arduinojson.org/assistant to compute the capacity.
      StaticJsonBuffer<768> jsonBuffer;
      // Parse the root object
      JsonObject &root = jsonBuffer.parseObject(file);
      #ifdef CFG_DEBUG
      root.prettyPrintTo(Serial);
      Serial.println();
      #endif /* CFG_DEBUG */

      if (!root.success())
      {
         Serial.println(F("Failed to read file, using default configuration"));
      }

      // Copy values from the JsonObject to the Config
      strlcpy(config.name, root["name"], sizeof(config.name));
      strlcpy(config.ssid, root["ssid"], sizeof(config.ssid));
      strlcpy(config.wifiPwd, root["wifiPwd"], sizeof(config.wifiPwd));
      strlcpy(config.mqttHost, root["mqttHost"], sizeof(config.mqttHost));
      config.mqttPort = root["mqttPort"];
      strlcpy(config.mqttUser, root["mqttUser"], sizeof(config.mqttUser));
      strlcpy(config.mqttPwd, root["mqttPwd"], sizeof(config.mqttPwd));
      config.tTemp = root["tTemp"];
      config.calibF = root["calibF"];
      config.calibO = root["calibO"];
      strlcpy(config.updServer, root["updServer"], sizeof(config.updServer));
      config.sensUpdInterval = root["sensUpdInterval"];
      config.mqttPubCycleInterval = root["mqttPubCycleInterval"];

      // Close the file (File's destructor doesn't close the file)
      file.close();
   }
   else
   {
      /* config does not exist, fallback to config.h */
      strlcpy(config.name, "unknown", sizeof(config.name));
      strlcpy(config.ssid, WIFI_SSID, sizeof(config.ssid));
      strlcpy(config.wifiPwd, WIFI_PWD, sizeof(config.wifiPwd));
      strlcpy(config.mqttHost, LOCAL_MQTT_HOST, sizeof(config.mqttHost));
      config.mqttPort             = LOCAL_MQTT_PORT;
      strlcpy(config.mqttUser, LOCAL_MQTT_USER, sizeof(config.mqttUser));
      strlcpy(config.mqttPwd, LOCAL_MQTT_PWD, sizeof(config.mqttPwd));
      config.tTemp                = 200;
      config.calibF               = 100;
      config.calibO               = 0;
      strlcpy(config.updServer, THERMOSTAT_BINARY, sizeof(config.updServer));
      config.sensUpdInterval      = SENSOR_UPDATE_INTERVAL;
      config.mqttPubCycleInterval = 5;
   }
}

// Saves the configuration to a file
bool saveConfiguration(const configuration &config)
{
   bool ret = true;
   bool writeFile = false;
   StaticJsonBuffer<768> jsonBuffer;

   if (SPIFFS.exists(filename))
   {
      // Delete existing file, otherwise the configuration is appended to the file
      File file = SPIFFS.open(filename, "r");
      JsonObject &root = jsonBuffer.parseObject(file);

      #ifdef CFG_DEBUG
      Serial.println("SPIFFS content:");
      root.prettyPrintTo(Serial);
      Serial.println();
      Serial.println("Check SPIFFS vs. current config, 0 is equal, 1 is diff.");
      Serial.print((config.name == root["name"]) ? false : true);
      Serial.print((config.ssid == root["ssid"]) ? false : true);
      Serial.print((config.wifiPwd == root["wifiPwd"]) ? false : true);
      Serial.print((config.mqttHost == root["mqttHost"]) ? false : true);
      Serial.print((config.mqttPort == root["mqttPort"]) ? false : true);
      Serial.print((config.mqttUser == root["mqttUser"]) ? false : true);
      Serial.print((config.mqttPwd == root["mqttPwd"]) ? false : true);
      Serial.print((config.tTemp == root["tTemp"]) ? false : true);
      Serial.print((config.calibF == root["calibF"]) ? false : true);
      Serial.print((config.calibO == root["calibO"]) ? false : true);
      Serial.print((config.updServer == root["updServer"]) ? false : true);
      Serial.print((config.sensUpdInterval == root["sensUpdInterval"]) ? false : true);
      Serial.print((config.mqttPubCycleInterval == root["mqttPubCycleInterval"]) ? false : true);
      #endif /* CFG_DEBUG */
      
      /* check if SPIFFS content is equal to avoid delete and write */
      writeFile |= (config.name == root["name"]) ? false : true;
      writeFile |= (config.ssid == root["ssid"]) ? false : true;
      writeFile |= (config.wifiPwd == root["wifiPwd"]) ? false : true;
      writeFile |= (config.mqttHost == root["mqttHost"]) ? false : true;
      writeFile |= (config.mqttPort == root["mqttPort"]) ? false : true;
      writeFile |= (config.mqttUser == root["mqttUser"]) ? false : true;
      writeFile |= (config.mqttPwd == root["mqttPwd"]) ? false : true;
      writeFile |= (config.tTemp == root["tTemp"]) ? false : true;
      writeFile |= (config.calibF == root["calibF"]) ? false : true;
      writeFile |= (config.calibO == root["calibO"]) ? false : true;
      writeFile |= (config.updServer == root["updServer"]) ? false : true;
      writeFile |= (config.sensUpdInterval == root["sensUpdInterval"]) ? false : true;
      writeFile |= (config.mqttPubCycleInterval == root["mqttPubCycleInterval"]) ? false : true;

      file.close();
   }
   else
   {
      /* file does not exist */
      writeFile = true;
   }

   /* if SPIFFS content differs current configuration, delete SPIFFS and write new content*/
   if (writeFile == true)
   {
      SPIFFS.remove(filename);

      File file = SPIFFS.open(filename, "w");

      JsonObject &root = jsonBuffer.createObject();

      root["name"] = config.name;
      root["ssid"] = config.ssid;
      root["wifiPwd"] = config.wifiPwd;
      root["mqttHost"] = config.mqttHost;
      root["mqttPort"] = config.mqttPort;
      root["mqttUser"] = config.mqttUser;
      root["mqttPwd"] = config.mqttPwd;
      root["tTemp"] = config.tTemp;
      root["calibF"] = config.calibF;
      root["calibO"] = config.calibO;
      root["updServer"] = config.updServer;
      root["sensUpdInterval"] = config.sensUpdInterval;
      root["mqttPubCycleInterval"] = config.mqttPubCycleInterval;

      // Serialize JSON to file
      if (root.printTo(file) == 0)
      {
         ret = false;
         #ifdef CFG_DEBUG
         Serial.println("Failed to write to file");
         #endif /* CFG_DEBUG */
      }
      else
      {
         #ifdef CFG_DEBUG
         Serial.println("new SPIFFS content:");
         root.prettyPrintTo(Serial);
         Serial.println();
         #endif /* CFG_DEBUG */
      }
      file.close();
   }
   else
   {
      #ifdef CFG_DEBUG
      Serial.println("SPIFFS content equals current config, no SPIFFS write necessary");
      #endif /* CFG_DEBUG */
   }
   return ret;
}

/* read SPIFFS */
String readSpiffs(String file)
{
   String fileContent = "";

   if (SPIFFS.exists(file))
   {
      File f = SPIFFS.open(file, "r");
      if (!f)
      {
         Serial.println("oh no, failed to open file: " + file);
         /* TODO: Error handling */
      }
      else
      {
         fileContent = f.readStringUntil('\n');
         f.close();

         if (fileContent == "")
         {
            Serial.println("File " + file + " is empty");
         }
      }
   }

   return fileContent;
}

/* write SPIFFS */
boolean writeSpiffs(String file, String newFileContent)
{
   boolean success = false; /* return success = true if no error occured and the newFileContent equals the file content */
   String currentFileContent = "";

   if (SPIFFS.exists(file))
   {
#ifdef CFG_DEBUG
      File f = SPIFFS.open(file, "r");
      currentFileContent = f.readStringUntil('\n');
      f.close();
#endif
   }

   if (currentFileContent == newFileContent)
   {
      success = true; /* nothing to do, consider write as successful */

#ifdef CFG_DEBUG
      Serial.println("Content of '" + file + "' before writing: " + currentFileContent + " equals new content: " + newFileContent + ". Nothing to do");
#endif
   }
   else
   {
#ifdef CFG_DEBUG
      Serial.println("Content of '" + file + "' before writing: " + currentFileContent);
#endif

      SPIFFS.remove(file);

      File f = SPIFFS.open(file, "w");
      if (!f)
      {
#ifdef CFG_DEBUG
         Serial.println("Failed to open file: " + file + " for writing");
#endif /* CFG_DEBUG */

         /* TODO: Error handling */
      }
      else
      {
#ifdef CFG_DEBUG
         Serial.println("New content of '" + file + "' is: " + newFileContent);
#endif /* CFG_DEBUG */

         f.print(newFileContent);
         f.close();
         success = true; /* new content written */
         delay(1000);
      }
   }
   return success;
}