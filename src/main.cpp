/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "ESP8266WebServer.h"
#include <ESP8266mDNS.h>
#include <DHTesp.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

#include "main.h"
#include "version.h"
#include "cSensor.h"
#include "cMQTT.h"

#if CFG_MQTT_LIB == cPubSubClient
#include "PubSubClient.h"
#else
#include "MQTTClient.h"
#endif

/*===================================================================================================================*/
/* GPIO config */
/*===================================================================================================================*/
ADC_MODE(ADC_VCC);             /* measure Vcc */
#define DHT_PIN              2 /* sensor */

/*===================================================================================================================*/
/* variables, constants, types, classes, etc. definitions */
/*===================================================================================================================*/
uint32_t WiFiConnectCounter = 0;
uint32_t MQTTConnectCounter = 0;
/* config */
struct configuration myConfig;
/* mqtt */
#ifndef MQTT_QOS
  #define MQTT_QOS 0 /* valid values are 0, 1 and 2 */
#endif
uint32_t mqttReconnectTime       = 0;
uint32_t mqttReconnectInterval   = MQTT_RECONNECT_TIME;
uint32_t mqttPubCycleTime        = 0;
#define secondsToMillisecondsFactor  1000
#define minutesToMillisecondsFactor 60000
/* loop handler */
#define loop50ms        50
#define loop100ms      100
#define loop500ms      500
#define loop1000ms    1000
uint32_t loop50msMillis   =  0;
uint32_t loop100msMillis  = 22; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop500msMillis  = 44; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop1000msMillis = 66; /* start loops with some offset to avoid calling all loops every second */
/* HTTP Update */
bool fetchUpdate = false;       /* global variable used to decide whether an update shall be fetched from server or not */
/* OTA */
typedef enum otaUpdate { TH_OTA_IDLE, TH_OTA_ACTIVE, TH_OTA_FINISHED, TH_OTA_ERROR } OtaUpdate_t;       /* global variable used to change display in case OTA update is initiated */
OtaUpdate_t OTA_UPDATE = TH_OTA_IDLE;
/* sensor */
uint32_t readSensorScheduled = 0;
/* classes */
DHTesp            myDHT;
WiFiClient        myWiFiClient;
Sensor            mySensor;
mqttHelper        myMqttHelper;
ESP8266WebServer  webServer(80);
#if CFG_MQTT_LIB == cPubSubClient
PubSubClient      myMqttClient(LOCAL_MQTT_HOST, LOCAL_MQTT_PORT, messageReceived, myWiFiClient);
#else
MQTTClient        myMqttClient(2048); /* 2048 byte buffer */
#endif

bool     systemRestartRequest = false;
bool     nameChanged = false;
uint32_t wifiReconnectTimer = 30000;

#define  SPIFFS_MQTT_ID_FILE        String("/itsme")
#define  SPIFFS_SENSOR_CALIB_FILE   String("/sensor")
#define  SPIFFS_TARGET_TEMP_FILE    String("/targetTemp")
#define  SPIFFS_WRITE_DEBOUNCE      20000 /* write target temperature to spiffs if it wasn't changed for 20 s (time in ms) */
boolean  SPIFFS_WRITTEN =           true;
uint32_t SPIFFS_REFERENCE_TIME;

/*===================================================================================================================*/
/* The setup function is called once at startup of the sketch */
/*===================================================================================================================*/
void setup() {
  #ifdef CFG_DEBUG
  Serial.begin(115200);
  Serial.println("Serial connection established");
  #endif

  SPIFFS_INIT();                                                                                   /* read stuff from SPIFFS */
  mySensor.setup(myConfig.calibF, myConfig.calibO);                                                /* sensor calibration */
  myDHT.setup(DHT_PIN, DHTesp::DHT22);                                                             /* init DHT sensor */
  WIFI_CONNECT();                                                                                  /* connect to WiFi */
  OTA_INIT();
  myMqttHelper.setup();                                                       /* build MQTT topics based on the defined device name */
  MQTT_CONNECT(); /* connect to MQTT host and build subscriptions, must be called after SPIFFS_INIT()*/

  MDNS.begin(myConfig.name);
  webServer.begin();
  webServer.on("/", handleWebServerClient);

  SENSOR_MAIN(); /* acquire first sensor data before staring loop() to avoid relay toggle due to current temperature being 0 °C (init value) until first sensor value is read */

  #ifdef CFG_DEBUG
  Serial.println("Reset Reason: " + String(ESP.getResetReason()));
  Serial.println("Flash Size: " + String(ESP.getFlashChipRealSize()));
  Serial.println("Sketch Size: " + String(ESP.getSketchSize()));
  Serial.println("Free for Sketch: " + String(ESP.getFreeSketchSpace()));
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
  Serial.println("Vcc: " + String(ESP.getVcc() / 1000.0));
  #endif
}

/*===================================================================================================================*/
/* functions called by setup()                                                                                       */
/*===================================================================================================================*/

void SPIFFS_INIT(void) {  // initializes the SPIFFS when first used and loads the configuration from SPIFFS to RAM
  SPIFFS.begin();

  if (!SPIFFS.exists("/formatted")) {
    /* This code is only run once to format the SPIFFS before first usage */
    #ifdef CFG_DEBUG
    Serial.println("Formatting SPIFFS, this takes some time");
    #endif
    SPIFFS.format();
    #ifdef CFG_DEBUG
    Serial.println("Formatting SPIFFS finished");
    Serial.println("Open file '/formatted' in write mode");
    #endif
    File f = SPIFFS.open("/formatted", "w");
    if (!f) {
      Serial.println("file open failed");
    } else {
      f.close();
      delay(5000);
      if (!SPIFFS.exists("/formatted")) {
        Serial.println("That didn't work!");
      } else {
        #ifdef CFG_DEBUG
        Serial.println("Cool, working!");
        #endif
      }
    }
  } else {
    #ifdef CFG_DEBUG
    Serial.println("Found '/formatted' >> SPIFFS ready to use");
    #endif
  }
  #ifdef CFG_DEBUG
  Serial.println("Check if I remember who I am ... ");
  #endif

  #ifdef CFG_DEBUG
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.print("SPIFFS file found: " + dir.fileName() + " - Size in byte: ");
    File f = dir.openFile("r");
    Serial.println(f.size());
  }
  #endif

  loadConfiguration(myConfig);  // load config

  /* code for SPIFFS migration from several files to one JSON file */
  if (SPIFFS.exists(SPIFFS_MQTT_ID_FILE)) {  // first time with new config
    String myName = readSpiffs(SPIFFS_MQTT_ID_FILE);

    if (myName != "") {
      strlcpy(myConfig.name, myName.c_str(), sizeof(myConfig.name));
    }

    String targetTemp = readSpiffs(SPIFFS_TARGET_TEMP_FILE);
    if (targetTemp != "") {
      myConfig.tTemp = targetTemp.toInt();     // set temperature from spiffs here
      SPIFFS.remove(SPIFFS_TARGET_TEMP_FILE);  // delete file for next startup
      Serial.println("File " + SPIFFS_TARGET_TEMP_FILE + " is available, proceed with value: " + String(myConfig.tTemp));
    }

    SPIFFS.remove(SPIFFS_MQTT_ID_FILE);       // delete file for next startup
    SPIFFS.remove(SPIFFS_SENSOR_CALIB_FILE);  // delete file for next startup
  }

  Serial.println("My name is: " + String(myConfig.name));
}


void WIFI_CONNECT(void) {
  WiFiConnectCounter++;
  if (WiFi.status() != WL_CONNECTED) {
    #ifdef CFG_DEBUG
    Serial.println("Initialize WiFi ");
    #endif

    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.hostname(myConfig.name);
    WiFi.begin(myConfig.ssid, myConfig.wifiPwd);

    /* try to connect to WiFi, proceed offline if not connecting here*/
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      #ifdef CFG_DEBUG
      Serial.println("Failed to connect to WiFi, continue offline");
      #endif
    }
  }

  #ifdef CFG_DEBUG
  Serial.println("WiFi Status: "+ String(WiFi.status()));
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
}

void OTA_INIT(void) {
  ArduinoOTA.setHostname(strlwr(myConfig.name));

  ArduinoOTA.onStart([]() {
    myMqttClient.disconnect();
    OTA_UPDATE = TH_OTA_ACTIVE;
    Serial.println("Start");
  });

  ArduinoOTA.onEnd([]() {
    OTA_UPDATE = TH_OTA_FINISHED;
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    OTA_UPDATE = TH_OTA_ERROR;
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    systemRestartRequest = true;
  });
  ArduinoOTA.begin();
}

void MQTT_CONNECT(void) {
  MQTTConnectCounter++;
  if (WiFi.status() == WL_CONNECTED) {
    if (!myMqttClient.connected()) {
      myMqttClient.disconnect();
      #if CFG_MQTT_LIB == cArduinoMQTT
      myMqttClient.setOptions(30, true, 30000);
      myMqttClient.begin(myConfig.mqttHost, myConfig.mqttPort, myWiFiClient);
      myMqttClient.setWill((myMqttHelper.getTopicLastWill()).c_str(),   "offline", true, MQTT_QOS);     /* broker shall publish 'offline' on ungraceful disconnect >> Last Will */
      myMqttClient.onMessage(messageReceived);                                                        /* register callback */
      (void)myMqttClient.connect(myConfig.name, myConfig.mqttUser, myConfig.mqttPwd);
      #else
      myMqttClient.setCallback(messageReceived);
      (void)myMqttClient.connect(myConfig.name, myConfig.mqttUser, myConfig.mqttPwd, myMqttHelper.getTopicLastWill().c_str(), MQTT_QOS, true, "offline");
      #endif

      homeAssistantDiscovery();  /* make HA discover necessary devices */

      #if CFG_MQTT_LIB == cArduinoMQTT
      myMqttClient.publish(myMqttHelper.getTopicLastWill(),             "online", true,  MQTT_QOS);   /* publish online in will topic */
      myMqttClient.publish(myMqttHelper.getTopicSystemRestartRequest(), "0",      false, MQTT_QOS);   /* publish restart = false on connect */
      /* subscribe topics */
      (void)myMqttClient.subscribe(myMqttHelper.getTopicTargetTempCmd(),         MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicThermostatModeCmd(),     MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicUpdateFirmware(),        MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeName(),            MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeHysteresis(),      MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeSensorCalib(),     MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicSystemRestartRequest(),  MQTT_QOS);
      #else
      myMqttClient.publish(myMqttHelper.getTopicLastWill().c_str(),             "online", true);   /* publish online in will topic */
      myMqttClient.publish(myMqttHelper.getTopicSystemRestartRequest().c_str(), "0",      false);   /* publish restart = false on connect */
      /* subscribe topics */
      (void)myMqttClient.subscribe(myMqttHelper.getTopicUpdateFirmware().c_str(),        MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeName().c_str(),            MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeSensorCalib().c_str(),     MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicSystemRestartRequest().c_str(),  MQTT_QOS);
      #endif
    }
  }
}

/*===================================================================================================================*/
/* The loop function is called in an endless loop */
/*===================================================================================================================*/
void loop() {
  /* call every 50 ms */
  if (TimeReached(loop50msMillis)) {
    SetNextTimeInterval(loop50msMillis, loop50ms);
    /* nothing yet */
  }
  /* call every 100 ms */
  if (TimeReached(loop100msMillis)) {
    SetNextTimeInterval(loop100msMillis, loop100ms);
    HANDLE_SYSTEM_STATE();   /* handle connectivity and trigger reconnects */
    MQTT_MAIN();             /* handle MQTT each loop */
  }
  /* call every 500 ms */
  if (TimeReached(loop500msMillis)) {
    SetNextTimeInterval(loop500msMillis, loop500ms);
    /* nothing yet */
  }
  /* call every second */
  if (TimeReached(loop1000msMillis)) {
    SetNextTimeInterval(loop1000msMillis, loop1000ms);
    SENSOR_MAIN();          /* get sensor data */
    SPIFFS_MAIN();          /* check for new data and write SPIFFS is necessary */
    HANDLE_HTTP_UPDATE();   /* pull update from server if it was requested via MQTT*/
  }

  webServer.handleClient();
  myMqttClient.loop();
  delay(10);
  ArduinoOTA.handle();
}

/*===================================================================================================================*/
/* functions called by loop() */
/*===================================================================================================================*/
void HANDLE_SYSTEM_STATE(void) {
  /* check WiFi connection every 30 seconds*/
  if (TimeReached(wifiReconnectTimer)) {
    SetNextTimeInterval(wifiReconnectTimer, (WIFI_RECONNECT_TIME * secondsToMillisecondsFactor));
    if (WiFi.status() != WL_CONNECTED) {
      #ifdef CFG_DEBUG
      Serial.println("Lost WiFi; Status: "+ String(WiFi.status()));
      #endif
      /* try to come online, debounced to avoid reconnect each loop*/
      WIFI_CONNECT();
    }
  }
  /* restart handling */
  if (systemRestartRequest == true) {
    systemRestartRequest = false;
    Serial.println("Restarting in 3 seconds");
    myMqttClient.disconnect();
    delay(3000);
    ESP.restart();
  }
}

void SENSOR_MAIN() {
  float dhtTemp;
  float dhtHumid;

  /* schedule routine for sensor read */
  if (TimeReached(readSensorScheduled)) {
    SetNextTimeInterval(readSensorScheduled, (myConfig.sensUpdInterval * secondsToMillisecondsFactor));

    /* Reading temperature or humidity takes about 250 milliseconds! */
    /* Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor) */
    dhtHumid = myDHT.getHumidity();
    dhtTemp  = myDHT.getTemperature();

    /* Check if any reads failed */
    if (isnan(dhtHumid) || isnan(dhtTemp)) {
      mySensor.setLastSensorReadFailed(true);   /* set failure flag and exit SENSOR_MAIN() */
      #ifdef CFG_DEBUG
      Serial.println("Failed to read from DHT sensor! Failure counter: " + String(mySensor.getSensorFailureCounter()));
      #endif
    } else {
      mySensor.setLastSensorReadFailed(false);   /* set no failure during read sensor */
      mySensor.setCurrentHumidity((int16_t)(10* dhtHumid));     /* read value and convert to one decimal precision integer */
      mySensor.setCurrentTemperature((int16_t)(10* dhtTemp));   /* read value and convert to one decimal precision integer */

      #ifdef CFG_DEBUG
      Serial.print("Temperature: ");
      Serial.print(intToFloat(mySensor.getCurrentTemperature()), 1);
      Serial.println(" *C ");
      Serial.print("Filtered temperature: ");
      Serial.print(intToFloat(mySensor.getFilteredTemperature()), 1);
      Serial.println(" *C ");

      Serial.print("Humidity: ");
      Serial.print(intToFloat(mySensor.getCurrentHumidity()), 1);
      Serial.println(" %");
      Serial.print("Filtered humidity: ");
      Serial.print(intToFloat(mySensor.getFilteredHumidity()), 1);
      Serial.println(" %");
      #endif

      #ifdef CFG_PRINT_TEMPERATURE_QUEUE
      for (int16_t i = 0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
        int16_t temperature;
        int16_t humidity;
        if ( (myTemperatureFilter.getRawSampleValue(i, &temperature)) && (myHumidityFilter.getRawSampleValue(i, &humidity)) ) {
          Serial.print("Sample ");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(intToFloat(temperature), 1);
          Serial.print(", ");
          Serial.println(intToFloat(humidity), 1);
        }
      }
      #endif
    }
  }
}

void MQTT_MAIN(void) {
  if (myMqttClient.connected() != true) {
    if (TimeReached(mqttReconnectTime)) {  /* try reconnect to MQTT broker after mqttReconnectTime expired */
        SetNextTimeInterval(mqttReconnectTime, mqttReconnectInterval); /* reset interval */
        MQTT_CONNECT();
        mqttPubState();
    } else {
      /* just wait */
    }
  } else {  /* check if there is new data to publish and shift PubCycle if data is published on event, else publish every PubCycleInterval */
    if ( TimeReached(mqttPubCycleTime) || mySensor.getNewData() ) {
        SetNextTimeInterval(mqttPubCycleTime, myConfig.mqttPubCycleInterval * minutesToMillisecondsFactor);
        mqttPubState();
        mySensor.resetNewData();
    }
  }
}

void HANDLE_HTTP_UPDATE(void) {
  if (fetchUpdate == true) {
    /* publish and loop here before fetching the update */
    #if CFG_MQTT_LIB == cArduinoMQTT
    myMqttClient.publish(myMqttHelper.getTopicUpdateFirmwareAccepted(), String(false), false, MQTT_QOS); /* publish accepted update with value false to reset the switch in Home Assistant */
    #else
    myMqttClient.publish(myMqttHelper.getTopicUpdateFirmwareAccepted().c_str(), "false", false); /* publish accepted update with value false to reset the switch in Home Assistant */
    #endif
    myMqttClient.loop();

    fetchUpdate = false;
    Serial.println("Remote update started");
    WiFiClient client;
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, myConfig.updServer, FW);

    switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s \n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
    }
  }
}

void SPIFFS_MAIN(void) {
  if (nameChanged == true) {
      if (saveConfiguration(myConfig)) {
      /* write successful, restart to rebuild MQTT topics etc. */
      nameChanged = false;
      } else {
      /* write failed, retry next loop */
    }
  }

  /* avoid extensive writing to SPIFFS, therefore check if the target temperature didn't change for a certain time before writing. */
  if (mySensor.getNewCalib()) {
    SPIFFS_REFERENCE_TIME = millis();  // ToDo: handle wrap around
    myConfig.calibF = mySensor.getSensorCalibFactor();
    myConfig.calibO = mySensor.getSensorCalibOffset();
    mySensor.resetNewCalib();
    SPIFFS_WRITTEN = false;
  } else { /* target temperature not changed this loop */
    if (SPIFFS_WRITTEN == true) {
      /* do nothing, last change was already stored in SPIFFS */
    } else { /* latest change not stored in SPIFFS */
      if (SPIFFS_REFERENCE_TIME + SPIFFS_WRITE_DEBOUNCE < millis()) { /* check debounce */
        /* debounce expired -> write */
        if (saveConfiguration(myConfig)) {
          SPIFFS_WRITTEN = true;
        } else {
          /* SPIFFS not written, retry next loop */
        }
      } else {
        /* debounce SPIFFS write */
      }
    }
  }
}
/*===================================================================================================================*/
/* callback, interrupt, timer, other functions */
/*===================================================================================================================*/

/* Home Assistant discovery on connect; used to define entities in HA to communicate with*/
void homeAssistantDiscovery(void) {
  #if CFG_MQTT_LIB == cArduinoMQTT
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryDevice(),                    myMqttHelper.buildTopicHassDiscoveryDevice(String(myConfig.name), String(FW)),     true, MQTT_QOS);       // make HA discover the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(bsSensFail),    myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name), bsSensFail),    true, MQTT_QOS);       // make HA discover the binary_sensor for sensor failure
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sTemp),               myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sTemp),               true, MQTT_QOS);       // make HA discover the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHum),                myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sHum),                true, MQTT_QOS);       // make HA discover the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sIP),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sIP),                 true, MQTT_QOS);       // make HA discover the IP sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibF),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sCalibF),             true, MQTT_QOS);       // make HA discover the scaling sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibO),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sCalibO),             true, MQTT_QOS);       // make HA discover the offset sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sFW),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sFW),                 true, MQTT_QOS);       // make HA discover the firmware version sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swRestart),           myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name), swRestart),           true, MQTT_QOS);       // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swUpdate),            myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name), swUpdate),            true, MQTT_QOS);       // make HA discover the update switch
  #else
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(bsSensFail).c_str(),    myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name).c_str(), bsSensFail).c_str(),    true);    // make HA discover the binary_sensor for sensor failure
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sTemp).c_str(),               myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sTemp).c_str(),               true);    // make HA discover the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHum).c_str(),                myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sHum).c_str(),                true);    // make HA discover the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sIP).c_str(),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sIP).c_str(),                 true);    // make HA discover the IP sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibF).c_str(),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sCalibF).c_str(),             true);    // make HA discover the scaling sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibO).c_str(),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sCalibO).c_str(),             true);    // make HA discover the offset sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sFW).c_str(),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sFW).c_str(),                 true);    // make HA discover the firmware version sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swRestart).c_str(),           myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name).c_str(), swRestart).c_str(),           true);    // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swUpdate).c_str(),            myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name).c_str(), swUpdate).c_str(),            true);    // make HA discover the update switch
  #endif
}

/* publish state topic in JSON format */
void mqttPubState(void) {
  String payload = myMqttHelper.buildStateJSON( /* build JSON payload */\
      String(myConfig.name), \
      String(intToFloat(mySensor.getFilteredTemperature()), 1), \
      String(intToFloat(mySensor.getFilteredHumidity()), 1), \
      String(mySensor.getSensorError()), \
      String(mySensor.getSensorCalibFactor()), \
      String(intToFloat(mySensor.getSensorCalibOffset()), 0), \
      WiFi.localIP().toString(), \
      String(FW) );
  #if CFG_MQTT_LIB == cArduinoMQTT
  myMqttClient.publish( \
    myMqttHelper.getTopicData(), /* get topic */ \
    payload, \
    true, /* retain */ \
    MQTT_QOS); /* QoS */
  #else
  myMqttClient.publish( \
    myMqttHelper.getTopicData().c_str(), /* get topic */ \
    payload.c_str(), \
    true); /* retain */
  #endif
}

void handleWebServerClient(void) {
  webServer.send(200, "text/plain", \
    "Name: "+ String(myConfig.name) + "\n" \
    "FW version: "+ String(FW) + "\n" \
    "Reset Reason: "+ String(ESP.getResetReason()) + "\n" \
    "Time since Reset: "+ String(millisFormatted()) + "\n" \
    "Flash Size: "+ String(ESP.getFlashChipRealSize()) + "\n" \
    "Sketch Size: "+ String(ESP.getSketchSize()) + "\n" \
    "Free for Sketch: "+ String(ESP.getFreeSketchSpace()) + "\n" \
    "Free Heap: "+ String(ESP.getFreeHeap()) + "\n" \
    "Vcc: "+ String(ESP.getVcc()/1000.0) + "\n" \
    "WiFi Status: " + String(WiFi.status()) + "\n" \
    "WiFi RSSI: " + String(WiFi.RSSI()) + "\n" \
    "WiFi connects: " + String(WiFiConnectCounter) + "\n" \
    "MQTT connection: " + String((myMqttClient.connected()) == true ? "connected" : "disconnected") + "\n" \
    "MQTT connects: " + String(MQTTConnectCounter));
}

/* MQTT callback if a message was received */
#if CFG_MQTT_LIB == cArduinoMQTT
void messageReceived(String &topic, String &payload) { //NOLINT
#else
void messageReceived(char* c_topic, byte* data, unsigned int data_len) {
  char dataBuf[data_len+1];  // NOLINT: no need to allocate more buffer than necessary
  memcpy(dataBuf, data, data_len);
  dataBuf[sizeof(dataBuf)-1] = 0;
  String payload = String(dataBuf);
  String topic = String(c_topic);
#endif

  #ifdef CFG_DEBUG
  Serial.println("received: " + topic + " : " + payload);
  #endif

  if (topic == myMqttHelper.getTopicSystemRestartRequest()) {
    #ifdef CFG_DEBUG
    Serial.println("Restart request received with value: " + payload);
    #endif
    if ((payload == "true") || (payload == "1")) {
      systemRestartRequest = true;
    }
  } else if (topic == myMqttHelper.getTopicUpdateFirmware()) { /* HTTP Update */
    if ((payload == "true") || (payload == "1")) {
      #ifdef CFG_DEBUG
      Serial.println("Firmware updated triggered via MQTT");
      #endif
      fetchUpdate = true;
    }
  } else if (topic == myMqttHelper.getTopicChangeName()) {
    #ifdef CFG_DEBUG
    Serial.println("New name received: " + payload);
    #endif
    if (payload != myConfig.name) {
      #ifdef CFG_DEBUG
      Serial.println("Old name was: " + String(myConfig.name));
      #endif
      strlcpy(myConfig.name, payload.c_str(), sizeof(myConfig.name));
      nameChanged = true;
    }
  } else if (topic == myMqttHelper.getTopicChangeSensorCalib()) {
    #ifdef CFG_DEBUG
    Serial.println("New sensor calibration parameters received: " + payload);
    #endif

    int16_t offset;
    int16_t factor;

    if (splitSensorDataString(payload, &offset, &factor)) {
      mySensor.setSensorCalibData(factor, offset, true);
    }
  }
}
