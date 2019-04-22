/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

#include <SPI.h>
#include <RCSwitch.h>

#include "main.h"
#include "version.h"
#include "cMQTT.h"

#include "PubSubClient.h"

/*===================================================================================================================*/
/* GPIO config */
/*===================================================================================================================*/
ADC_MODE(ADC_VCC);             /* measure Vcc */
#define RF_TX_PIN    13
#define RF_RX_PIN    16
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

#define rfPulseLength    189
#define rfRepeatTxCount   10
String mqttRfCommand = "";
RCSwitch          mySwitch;
WiFiClient        myWiFiClient;
mqttHelper        myMqttHelper;
ESP8266WebServer  webServer(80);

PubSubClient      myMqttClient(LOCAL_MQTT_HOST, LOCAL_MQTT_PORT, messageReceived, myWiFiClient);

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

  SPIFFS_INIT();             /* read stuff from SPIFFS */
  GPIO_CONFIG();             /* configure GPIOs */
  WIFI_CONNECT();            /* connect to WiFi */
  OTA_INIT();
  myMqttHelper.setup();      /* build MQTT topics based on the defined device name */
  MQTT_CONNECT();            /* connect to MQTT host and build subscriptions, must be called after SPIFFS_INIT()*/

  MDNS.begin(myConfig.name);
  webServer.begin();
  webServer.on("/", handleWebServerClient);

  /* RCSwitch init */
  mySwitch = RCSwitch();
  mySwitch.enableTransmit(RF_TX_PIN);             // RF Transmitter is connected to Arduino Pin #10
  mySwitch.setRepeatTransmit(rfRepeatTxCount);    // increase transmit repeat to avoid lost of rf sendings
  mySwitch.enableReceive(RF_RX_PIN);              // Receiver on inerrupt 0 => that is pin #2
  mySwitch.setPulseLength(rfPulseLength);         // RF Pulse Length, varies per device.

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

void GPIO_CONFIG(void) {  /* GPIOs */
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
      myMqttClient.setCallback(messageReceived);
      (void)myMqttClient.connect(myConfig.name, myConfig.mqttUser, myConfig.mqttPwd, myMqttHelper.getTopicLastWill().c_str(), MQTT_QOS, true, "offline");

      homeAssistantDiscovery();  /* make HA discover necessary devices */

      myMqttClient.publish(myMqttHelper.getTopicLastWill().c_str(),             "online", true);   /* publish online in will topic */
      myMqttClient.publish(myMqttHelper.getTopicSystemRestartRequest().c_str(), "0",      false);   /* publish restart = false on connect */
      /* subscribe topics */
      (void)myMqttClient.subscribe(myMqttHelper.getTopicRfCommand().c_str(),             MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicUpdateFirmware().c_str(),        MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeName().c_str(),            MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicSystemRestartRequest().c_str(),  MQTT_QOS);
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
  }
  /* call every 500 ms */
  if (TimeReached(loop500msMillis)) {
    SetNextTimeInterval(loop500msMillis, loop500ms);
    /* nothing yet */
  }
  /* call every second */
  if (TimeReached(loop1000msMillis)) {
    SetNextTimeInterval(loop1000msMillis, loop1000ms);
    SPIFFS_MAIN();          /* check for new data and write SPIFFS is necessary */
    HANDLE_HTTP_UPDATE();   /* pull update from server if it was requested via MQTT*/
  }

  HANDLE_SYSTEM_STATE();   /* handle connectivity and trigger reconnects */
  MQTT_MAIN();             /* handle MQTT */

  if (mqttRfCommand != "") {                          // check if command was received via mqtt
    mySwitch.send(atol(mqttRfCommand.c_str()), 24);   // transmit
    mqttRfCommand = "";                               // reset command
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

void MQTT_MAIN(void) {
  if (myMqttClient.connected() != true) {
    if (TimeReached(mqttReconnectTime)) {  /* try reconnect to MQTT broker after mqttReconnectTime expired */
        SetNextTimeInterval(mqttReconnectTime, mqttReconnectInterval); /* reset interval */
        MQTT_CONNECT();
    } else {
      /* just wait */
    }
  } else {  /* check if there is new data to publish */
    if (mySwitch.available()) {
      #ifdef CFG_DEBUG
      Serial.println("RF signal available"+ String(mySwitch.getReceivedValue()));
      #endif
      mqttPubState();
      mySwitch.resetAvailable();
    }
    if (myMqttHelper.getTriggerDiscovery()) {
      homeAssistantDiscovery();  /* make HA discover/update necessary devices at runtime e.g. after name change */
    }
  }
}

void HANDLE_HTTP_UPDATE(void) {
  if (fetchUpdate == true) {
    /* publish and loop here before fetching the update */
    myMqttClient.publish(myMqttHelper.getTopicUpdateFirmwareAccepted().c_str(), "false", false); /* publish accepted update with value false to reset the switch in Home Assistant */
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
      myMqttHelper.setTriggerDiscovery(true);
      } else {
      /* write failed, retry next loop */
    }
  }
}
/*===================================================================================================================*/
/* callback, interrupt, timer, other functions */
/*===================================================================================================================*/


/* Home Assistant discovery on connect; used to define entities in HA to communicate with*/
void homeAssistantDiscovery(void) {
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryDevice().c_str(),            myMqttHelper.buildHassDiscoveryDevice(String(myConfig.name).c_str(), String(FW)).c_str(),          true);    // make HA discover the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sIP).c_str(),         myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sIP).c_str(),                 true);    // make HA discover the IP sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sFW).c_str(),         myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sFW).c_str(),                 true);    // make HA discover the firmware version sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swRestart).c_str(),   myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name).c_str(), swRestart).c_str(),           true);    // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swUpdate).c_str(),    myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name).c_str(), swUpdate).c_str(),            true);    // make HA discover the update switch
}

/* publish state topic in JSON format */
void mqttPubState(void) {
  String payload = myMqttHelper.buildStateJSON( /* build JSON payload */\
      String(myConfig.name), \
      String(mySwitch.getReceivedValue(), 1), \
      WiFi.localIP().toString(), \
      String(FW) );

  myMqttClient.publish( \
    myMqttHelper.getTopicData().c_str(), /* get topic */ \
    payload.c_str(), \
    false); /* retain */
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
void messageReceived(char* c_topic, byte* data, unsigned int data_len) {
  char dataBuf[data_len+1];  // NOLINT: no need to allocate more buffer than necessary
  memcpy(dataBuf, data, data_len);
  dataBuf[sizeof(dataBuf)-1] = 0;
  String payload = String(dataBuf);
  String topic = String(c_topic);

  #ifdef CFG_DEBUG
  Serial.println("received: " + topic + " : " + payload);
  #endif

  if (topic == myMqttHelper.getTopicRfCommand()) {
    #ifdef CFG_DEBUG
    Serial.println("RfCommand received: " + payload);
    #endif
    mqttRfCommand = payload;
  } else if (topic == myMqttHelper.getTopicSystemRestartRequest()) {
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
  }
}
