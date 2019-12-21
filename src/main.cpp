/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

#include <DHTesp.h>
#include "UserFonts.h"
#include <SSD1306.h>

#include "main.h"
#include "version.h"
#include "cMQTT.h"

#include "cThermostat.h"

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
#define SDA_PIN              4 /* I²C */
#define SCL_PIN              5 /* I²C */
#define PHYS_INPUT_1_PIN    12 /* rotary left/right OR down/up */
#define PHYS_INPUT_2_PIN    13 /* rotary left/right OR down/up */
#define PHYS_INPUT_3_PIN    14 /* on/off/ok/reset */
#define RELAY_PIN           16 /* relay control */

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

/* 3 PushButtons */
uint32_t  upButtonDebounceTime           = 0;
uint32_t  downButtonDebounceTime         = 0;
/* rotary encoder */
uint32_t  onOffButtonDebounceTime        = 0;
#define rotLeft                                -1
#define rotRight                                1
#define rotInit                                 0
volatile int16_t lastEncoded                      = 0b11;        /* initial state of the rotary encoders gray code */
volatile int16_t rotaryEncoderDirectionInts       = rotInit;     /* initialize rotary encoder with no direction */
uint32_t buttonDebounceInterval         = 250;
uint32_t onOffButtonSystemResetTime     = 0;
uint32_t onOffButtonSystemResetInterval = 10000;

/* thermostat */
#define tempStep              5
#define displayTemp           0
#define displayHumid          1
/* sensor */
uint32_t readSensorScheduled = 0;
/* Display */
#define drawTempYOffset       16
#define drawTargetTempYOffset 0
#define drawHeating drawXbm(0, drawTempYOffset, myThermo_width, myThermo_height, myThermo)
/* classes */
DHTesp            myDHT;
SSD1306           myDisplay(0x3c, SDA_PIN, SCL_PIN);
Thermostat        myThermostat;
WiFiClient        myWiFiClient;
mqttHelper        myMqttHelper;
ESP8266WebServer  webServer(80);
#if CFG_MQTT_LIB == cPubSubClient
PubSubClient      myMqttClient(LOCAL_MQTT_HOST, LOCAL_MQTT_PORT, messageReceived, myWiFiClient);
#else
MQTTClient        myMqttClient(2048); /* 2048 byte buffer */
#endif

bool     systemRestartRequest = false;
bool     requestSaveToSpiffs = false;
bool     requestSaveToSpiffsWithRestart = false;
uint32_t wifiReconnectTimer = 30000;

#define  SPIFFS_MQTT_ID_FILE        String("/itsme")       // for migration only
#define  SPIFFS_SENSOR_CALIB_FILE   String("/sensor")      // for migration only
#define  SPIFFS_TARGET_TEMP_FILE    String("/targetTemp")  // for migration only
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
  GPIO_CONFIG();                                                                                   /* configure GPIOs */
  myThermostat.setup(RELAY_PIN, myConfig.tTemp, myConfig.calibF, myConfig.calibO, myConfig.tHyst, myConfig.mode); /* GPIO to switch the connected relay, initial target temperature, sensor calbigration, thermostat hysteresis and last mode */
  myDHT.setup(DHT_PIN, DHTesp::DHT22);                                                             /* init DHT sensor */
  DISPLAY_INIT();                                                                                  /* init Display */
  WIFI_CONNECT();                                                                                  /* connect to WiFi */
  OTA_INIT();
  myMqttHelper.setup();                                                       /* build MQTT topics based on the defined device name */
  MQTT_CONNECT(); /* connect to MQTT host and build subscriptions, must be called after SPIFFS_INIT()*/

  MDNS.begin(myConfig.name);

  webServer.begin();
  webServer.on("/", handleWebServerClient);
  webServer.on("/restart", handleHttpReset);


  if (myConfig.inputMethod == cPUSH_BUTTONS) {
  attachInterrupt(PHYS_INPUT_1_PIN, upButton,   FALLING);
  attachInterrupt(PHYS_INPUT_2_PIN, downButton, FALLING);
  attachInterrupt(PHYS_INPUT_3_PIN, onOffButton, FALLING);
  } else {
  /* enable interrupts on encoder pins to decode gray code and recognize switch event*/
  attachInterrupt(PHYS_INPUT_1_PIN, updateEncoder, CHANGE);
  attachInterrupt(PHYS_INPUT_2_PIN, updateEncoder, CHANGE);
  attachInterrupt(PHYS_INPUT_3_PIN, onOffButton,   FALLING);
  }

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

  Serial.println("My name is: " + String(myConfig.name));
}

void GPIO_CONFIG(void) {  /* initialize encoder / push button pins */
  pinMode(PHYS_INPUT_1_PIN, INPUT_PULLUP);
  pinMode(PHYS_INPUT_2_PIN, INPUT_PULLUP);
  pinMode(PHYS_INPUT_3_PIN, INPUT_PULLUP);
}

void DISPLAY_INIT(void) {
  #ifdef CFG_DEBUG
  Serial.println("Initialize display");
  #endif
  Wire.begin();   /* needed for I²C communication with display */
  myDisplay.init();
  myDisplay.flipScreenVertically();
  myDisplay.clear();
  myDisplay.setFont(Roboto_Condensed_16);
  myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
  myDisplay.drawString(64, 4, "Initialize");
  myDisplay.drawString(64, 24, String(myConfig.name));
  myDisplay.drawString(64, 44, FW);
  myDisplay.display();
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
  ArduinoOTA.setHostname(myConfig.name);

  ArduinoOTA.onStart([]() {
    myMqttClient.disconnect();
    OTA_UPDATE = TH_OTA_ACTIVE;
    DRAW_DISPLAY_MAIN();
    Serial.println("Start");
  });

  ArduinoOTA.onEnd([]() {
    OTA_UPDATE = TH_OTA_FINISHED;
    DRAW_DISPLAY_MAIN();
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    OTA_UPDATE = TH_OTA_ERROR;
    DRAW_DISPLAY_MAIN();
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
      (void)myMqttClient.subscribe(myMqttHelper.getTopicTargetTempCmd().c_str(),         MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicThermostatModeCmd().c_str(),     MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicUpdateFirmware().c_str(),        MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeName().c_str(),            MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeHysteresis().c_str(),      MQTT_QOS);
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
    myThermostat.loop();     /* control relay for heating */
    DRAW_DISPLAY_MAIN();     /* draw display each loop */
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
  /* check if button is pushed for 10 seconds to request a reset */
  if (LOW == digitalRead(PHYS_INPUT_3_PIN)) {
    #ifdef CFG_DEBUG_ENCODER
    Serial.println("Encoder Button pressed: "+ String(digitalRead(PHYS_INPUT_3_PIN)));
    Serial.println("Target Time: "+ String(onOffButtonSystemResetTime + onOffButtonSystemResetInterval));
    Serial.println("Current time: "+ String(millis()));
    #endif

    if (onOffButtonSystemResetTime + onOffButtonSystemResetInterval < millis()) {
      systemRestartRequest = true;
    } else {
      /* waiting */
    }
  } else {
    onOffButtonSystemResetTime = millis();
  }

  /* restart handling */
  if (systemRestartRequest == true) {
    DRAW_DISPLAY_MAIN();
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
      myThermostat.setLastSensorReadFailed(true);   /* set failure flag and exit SENSOR_MAIN() */
      #ifdef CFG_DEBUG
      Serial.println("Failed to read from DHT sensor! Failure counter: " + String(myThermostat.getSensorFailureCounter()));
      #endif
    } else {
      myThermostat.setLastSensorReadFailed(false);   /* set no failure during read sensor */
      myThermostat.setCurrentHumidity((int16_t)(10* dhtHumid));     /* read value and convert to one decimal precision integer */
      myThermostat.setCurrentTemperature((int16_t)(10* dhtTemp));   /* read value and convert to one decimal precision integer */

      #ifdef CFG_DEBUG
      Serial.print("Temperature: ");
      Serial.print(intToFloat(myThermostat.getCurrentTemperature()), 1);
      Serial.println(" *C ");
      Serial.print("Filtered temperature: ");
      Serial.print(intToFloat(myThermostat.getFilteredTemperature()), 1);
      Serial.println(" *C ");

      Serial.print("Humidity: ");
      Serial.print(intToFloat(myThermostat.getCurrentHumidity()), 1);
      Serial.println(" %");
      Serial.print("Filtered humidity: ");
      Serial.print(intToFloat(myThermostat.getFilteredHumidity()), 1);
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

void DRAW_DISPLAY_MAIN(void) {
  myDisplay.clear();
  myDisplay.setContrast(80);

  if (systemRestartRequest == true) {
    myDisplay.setFont(Roboto_Condensed_16);
    myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
    myDisplay.drawString(64, 22, "Restart");
  } else if (OTA_UPDATE != TH_OTA_IDLE) { /* OTA */
    myDisplay.setFont(Roboto_Condensed_16);
    myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
    switch (OTA_UPDATE) {
    case TH_OTA_ACTIVE:
      myDisplay.drawString(64, 22, "Update");
      break;
    case TH_OTA_FINISHED:
      myDisplay.drawString(64, 11, "Finished,");
      myDisplay.drawString(64, 33, "restarting");
      break;
    case TH_OTA_ERROR:
      myDisplay.drawString(64, 22, "Error");
      break;
    case TH_OTA_IDLE:
      break;
    }
  } else if (fetchUpdate == true) { /* HTTP Update */
    myDisplay.setFont(Roboto_Condensed_16);
    myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
    myDisplay.drawString(64, 22, "Update");
  } else {
    myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
    myDisplay.setFont(Roboto_Condensed_32);

    if (myThermostat.getSensorError()) {
      myDisplay.drawString(128, drawTempYOffset, "err");
    } else {
      myDisplay.drawString(128, drawTempYOffset, String(intToFloat(myThermostat.getFilteredTemperature()), 1));
    }

    /* do not display target temperature if heating is not allowed */
    if (myThermostat.getThermostatMode() == TH_HEAT) {
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.drawString(128, drawTargetTempYOffset, String(intToFloat(myThermostat.getTargetTemperature()), 1));

      if (myThermostat.getActualState()) { /* heating */
        myDisplay.drawHeating;
      }
    }
  }
  #ifdef CFG_DEBUG_DISPLAY_VERSION
  myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
  myDisplay.setFont(Roboto_Condensed_16);
  myDisplay.drawString(0, drawTargetTempYOffset, String(VERSION));
  #endif

  myDisplay.display();
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
    if ( TimeReached(mqttPubCycleTime) || myThermostat.getNewData() ) {
        SetNextTimeInterval(mqttPubCycleTime, myConfig.mqttPubCycle * minutesToMillisecondsFactor);
        mqttPubState();
        myThermostat.resetNewData();
    }
    if (myMqttHelper.getTriggerDiscovery()) {
      homeAssistantDiscovery();  /* make HA discover/update necessary devices at runtime e.g. after name change */
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

    DRAW_DISPLAY_MAIN();
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
  /* force SPIFFS write with restart, this is necessary for SW reconfiguration in certain cases like changing the input method or changing the decives name */

  if (requestSaveToSpiffsWithRestart == true) {
    if (saveConfiguration(myConfig)) {
      requestSaveToSpiffsWithRestart = false;
      requestSaveToSpiffs = false;  /* reset non reset request as well */
      systemRestartRequest = true;
      } else {
      /* write failed, retry next loop */
    }
  }
  if (requestSaveToSpiffs == true) {
    if (saveConfiguration(myConfig)) {
      requestSaveToSpiffs = false;
      } else {
      /* write failed, retry next loop */
    }
  }
  /* avoid extensive writing to SPIFFS, therefore check if the target temperature didn't change for a certain time before writing. */
  if ( (myThermostat.getNewCalib()) || (myThermostat.getTargetTemperature() != myConfig.tTemp) || (myThermostat.getThermostatHysteresis() != myConfig.tHyst) || (myThermostat.getThermostatMode() != myConfig.mode) ) {
    SPIFFS_REFERENCE_TIME = millis();  // ToDo: handle wrap around
    myConfig.tTemp  = myThermostat.getTargetTemperature();
    myConfig.calibF = myThermostat.getSensorCalibFactor();
    myConfig.calibO = myThermostat.getSensorCalibOffset();
    myConfig.tHyst  = myThermostat.getThermostatHysteresis();
    myConfig.mode   = myThermostat.getThermostatMode();
    myThermostat.resetNewCalib();
    SPIFFS_WRITTEN = false;
    #if defined CFG_DEBUG
    Serial.println("SPIFFS to be stored after debounce time: " + String(SPIFFS_WRITE_DEBOUNCE));
    #endif /* CFG_DEBUG */
  } else {
    /* no spiffs data changed this loop */
  }
  if (SPIFFS_WRITTEN == true) {
    /* do nothing, last change was already stored in SPIFFS */
  } else { /* latest change not stored in SPIFFS */
    if (SPIFFS_REFERENCE_TIME + SPIFFS_WRITE_DEBOUNCE < millis()) { /* check debounce */
      /* debounce expired -> write */
      if (saveConfiguration(myConfig)) {
        SPIFFS_WRITTEN = true;
      } else {
        /* SPIFFS not written, retry next loop */
        #if defined CFG_DEBUG
        Serial.println("SPIFFS write failed");
        #endif /* CFG_DEBUG */
      }
    } else {
      /* debounce SPIFFS write */
    }
  }
}
/*===================================================================================================================*/
/* callback, interrupt, timer, other functions */
/*===================================================================================================================*/

void ICACHE_RAM_ATTR onOffButton(void) {
  /* debouncing routine for encoder switch */
  if (TimeReached(onOffButtonDebounceTime)) {
    SetNextTimeInterval(onOffButtonDebounceTime, buttonDebounceInterval);
    myThermostat.toggleThermostatMode();
  }
}

/* Push Buttons */
void ICACHE_RAM_ATTR upButton(void) {
  /* debouncing routine for push button */
  if (TimeReached(upButtonDebounceTime)) {
    SetNextTimeInterval(upButtonDebounceTime, buttonDebounceInterval);
    myThermostat.increaseTargetTemperature(tempStep);
  }
}

void ICACHE_RAM_ATTR downButton(void) {
  /* debouncing routine for push button */
  if (TimeReached(downButtonDebounceTime)) {
    SetNextTimeInterval(downButtonDebounceTime, buttonDebounceInterval);
    myThermostat.decreaseTargetTemperature(tempStep);
  }
}
/* Rotary Encoder */
void ICACHE_RAM_ATTR updateEncoder(void) {
  int16_t MSB = digitalRead(PHYS_INPUT_1_PIN);
  int16_t LSB = digitalRead(PHYS_INPUT_2_PIN);

  int16_t encoded = (MSB << 1) |LSB;  /* converting the 2 pin value to single number */

  int16_t sum  = (lastEncoded << 2) | encoded; /* adding it to the previous encoded value */

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) { /* gray patterns for rotation to right */
    rotaryEncoderDirectionInts += rotRight; /* count rotation interrupts to left and right, this shall make the encoder routine more robust against bouncing */
  }
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) { /* gray patterns for rotation to left */
    rotaryEncoderDirectionInts += rotLeft;
  }

  if (encoded == 0b11) { /* if the encoder is in an end position, evaluate the number of interrupts to left/right. simple majority wins */
    #ifdef CFG_DEBUG_ENCODER
    Serial.println("Rotary encoder interrupts: " + String(rotaryEncoderDirectionInts));
    #endif

    if (rotaryEncoderDirectionInts > rotRight) { /* if there was a higher amount of interrupts to the right, consider the encoder was turned to the right */
      myThermostat.increaseTargetTemperature(tempStep);
    } else if (rotaryEncoderDirectionInts < rotLeft) { /* if there was a higher amount of interrupts to the left, consider the encoder was turned to the left */
      myThermostat.decreaseTargetTemperature(tempStep);
    } else {
      /* do nothing here, left/right interrupts have occurred with same amount -> should never happen */
    }
    rotaryEncoderDirectionInts = rotInit; /* reset interrupt count for next sequence */
  }
  lastEncoded = encoded; /* store this value for next time */
}

/* Home Assistant discovery on connect; used to define entities in HA to communicate with*/
void homeAssistantDiscovery(void) {
  #if CFG_MQTT_LIB == cArduinoMQTT
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate(),                   myMqttHelper.buildHassDiscoveryClimate(String(myConfig.name), String(FW)),         true, MQTT_QOS);    // make HA discover the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(bsSensFail),    myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name), bsSensFail),    true, MQTT_QOS);    // make HA discover the binary_sensor for sensor failure
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(bsState),       myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name), bsState),       true, MQTT_QOS);    // make HA discover the binary_sensor for thermostat state
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sTemp),               myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sTemp),               true, MQTT_QOS);    // make HA discover the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHum),                myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sHum),                true, MQTT_QOS);    // make HA discover the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sIP),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sIP),                 true, MQTT_QOS);    // make HA discover the IP sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibF),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sCalibF),             true, MQTT_QOS);    // make HA discover the scaling sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibO),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sCalibO),             true, MQTT_QOS);    // make HA discover the offset sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHysteresis),         myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sHysteresis),         true, MQTT_QOS);    // make HA discover the hysteresis sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sFW),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name), sFW),                 true, MQTT_QOS);    // make HA discover the firmware version sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swRestart),           myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name), swRestart),           true, MQTT_QOS);    // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swUpdate),            myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name), swUpdate),            true, MQTT_QOS);    // make HA discover the update switch
  #else
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate().c_str(),                   myMqttHelper.buildHassDiscoveryClimate(String(myConfig.name).c_str(), String(FW)).c_str(),         true);    // make HA discover the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(bsSensFail).c_str(),    myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name).c_str(), bsSensFail).c_str(),    true);    // make HA discover the binary_sensor for sensor failure
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(bsState).c_str(),       myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name).c_str(), bsState).c_str(),       true);    // make HA discover the binary_sensor for thermostat state
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sTemp).c_str(),               myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sTemp).c_str(),               true);    // make HA discover the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHum).c_str(),                myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sHum).c_str(),                true);    // make HA discover the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sIP).c_str(),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sIP).c_str(),                 true);    // make HA discover the IP sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibF).c_str(),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sCalibF).c_str(),             true);    // make HA discover the scaling sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibO).c_str(),             myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sCalibO).c_str(),             true);    // make HA discover the offset sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHysteresis).c_str(),         myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sHysteresis).c_str(),         true);    // make HA discover the hysteresis sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sFW).c_str(),                 myMqttHelper.buildHassDiscoverySensor(String(myConfig.name).c_str(), sFW).c_str(),                 true);    // make HA discover the firmware version sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swRestart).c_str(),           myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name).c_str(), swRestart).c_str(),           true);    // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swUpdate).c_str(),            myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name).c_str(), swUpdate).c_str(),            true);    // make HA discover the update switch
  #endif
}

/* publish state topic in JSON format */
void mqttPubState(void) {
  String payload = myMqttHelper.buildStateJSON( /* build JSON payload */\
      String(myConfig.name), \
      String(intToFloat(myThermostat.getFilteredTemperature()), 1), \
      String(intToFloat(myThermostat.getFilteredHumidity()), 1), \
      String(intToFloat(myThermostat.getThermostatHysteresis()), 1), \
      String(boolToStringOnOff(myThermostat.getActualState())), \
      String(intToFloat(myThermostat.getTargetTemperature()), 1), \
      String(myThermostat.getSensorError()), \
      String(boolToStringHeatOff(myThermostat.getThermostatMode())), \
      String(myThermostat.getSensorCalibFactor()), \
      String(intToFloat(myThermostat.getSensorCalibOffset()), 0), \
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
  String IPaddress = WiFi.localIP().toString();
  float rssiInPercent = WiFi.RSSI();
  rssiInPercent = isnan(rssiInPercent) ? -100.0 : min(max(2 * (rssiInPercent + 100.0), 0.0), 100.0);

  String webpage = "<!DOCTYPE html> <html>\n";
  /* HEAD */
  webpage +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  webpage +="<title> "+ String(myConfig.name) + "</title>\n";
  webpage +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}\n";
  webpage +="body {background-color: #202226; color: #A0A2A8}\n";
  webpage +="p {color: #2686c1; margin-bottom: 10px;}\n";
  webpage +="table {color: #A0A2A8;}\n";
  webpage +="</style>\n";
  webpage +="</head>\n";
  /* BODY */
  webpage +="<body>\n";
  webpage +="<p><b>System information</b></p>";
  webpage +="<table>";

  /*= KEY ======================================||= VALUE ================================*/
  webpageTableAppend(String("Name"),              String(myConfig.name));
  webpageTableAppend(String("Chip ID"),           String(ESP.getChipId(), HEX));
  webpageTableAppend(String("IPv4"),              IPaddress);
  webpageTableAppend(String("FW version"),        String(FW));
  if (myConfig.inputMethod == cPUSH_BUTTONS) {
    webpageTableAppend(String("Input Method"),      String("Push Buttons"));
  } else {
    webpageTableAppend(String("Input Method"),      String("Rotary Encoder"));
  }
  webpageTableAppend(String("Arduino Core"),      ESP.getCoreVersion());
  webpageTableAppend(String("Reset Reason"),      ESP.getResetReason());
  webpageTableAppend(String("Time since Reset"),  String(millisFormatted()));
  webpageTableAppend(String("Flash Size"),        String(ESP.getFlashChipRealSize()));
  webpageTableAppend(String("Sketch Size"),       String(ESP.getSketchSize()));
  webpageTableAppend(String("Free for Sketch"),   String(ESP.getFreeSketchSpace()));
  webpageTableAppend(String("Free Heap"),         String(ESP.getFreeHeap()));
  webpageTableAppend(String("Vcc"),               String(ESP.getVcc()/1000.0));
  webpageTableAppend(String("WiFi Status"),       wifiStatusToString(WiFi.status()));
  webpageTableAppend(String("WiFi Strength"),     String(rssiInPercent, 0) + " %");
  webpageTableAppend(String("WiFi Connects"),     String(WiFiConnectCounter));
  webpageTableAppend(String("MQTT Status"),       String((myMqttClient.connected()) == true ? "connected" : "disconnected"));
  webpageTableAppend(String("MQTT Connects"),     String(MQTTConnectCounter));
  webpage +="</table>";

  webpage +="<p><b>Change Name</b></p>";
  webpage +="<form method='POST' autocomplete='off'>";
  webpage +="<input type='text' name='newName' value="+ String(myConfig.name) + ">&nbsp;<input type='submit' value='Submit'>";
  webpage +="</form>";

  webpage +="<p><b>Change Update Server</b></p>";
  webpage +="<form method='POST' autocomplete='off'>";
  webpage +="<input type='text' name='updServer' value="+ String(myConfig.updServer) + ">&nbsp;<input type='submit' value='Submit'>";
  webpage +="</form>";

  webpage +="<p><b>Change Input Method</b></p>";
  webpage +="<form method='POST' autocomplete='off'>";
  webpage +="<select name='InputMethod'> <option value='0'>Rotary Encoder </option> <option value='1'> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>";
  webpage +="</form>";

  webpage +="<p><b>Restart Device</b></p>";
  if (systemRestartRequest == false) {
    webpage +="<button onclick=\"window.location.href='/restart'\"> Restart </button>";
  } else {
    webpage +="<button onclick=\"window.location.href='/'\"> Restarting, reload site </button>";
  }
  webpage +="</body>\n";
  webpage +="</html>\n";

  webServer.send(200, "text/html", webpage);  /* Send a response to the client asking for input */

  if (webServer.args() > 0) {  /* Arguments were received */
    for ( uint8_t i = 0; i < webServer.args(); i++ ) {
      #ifdef CFG_DEBUG
      Serial.println("HTTP arg(" + String(i) + "): " + webServer.argName(i) + " = " + webServer.arg(i));  /* Display each argument */
      #endif /* CFG_DEBUG */

      if (webServer.argName(i) == "newName") {  /* check for dedicated arguments */
        if (webServer.arg(i) != myConfig.name) {  /* change name if it differs from the current one */
          #ifdef CFG_DEBUG
          Serial.println("Request SPIFFS write with restart.");
          #endif
          strlcpy(myConfig.name, webServer.arg(i).c_str(), sizeof(myConfig.name));
          requestSaveToSpiffsWithRestart = true;
        } else {
          #ifdef CFG_DEBUG
          Serial.println("Configuration unchanged, do nothing");
          #endif
        }
      }
      if (webServer.argName(i) == "InputMethod") {  /* check for dedicated arguments */
        if (webServer.arg(i) != String(myConfig.inputMethod)) {
          #ifdef CFG_DEBUG
          Serial.println("Request SPIFFS write with restart.");
          #endif
          myConfig.inputMethod = boolean(webServer.arg(i).toInt());
          requestSaveToSpiffsWithRestart = true;
        } else {
          #ifdef CFG_DEBUG
          Serial.println("Configuration unchanged, do nothing");
          #endif
        }
      }
      if (webServer.argName(i) == "updServer") {  /* check for dedicated arguments */
        if (webServer.arg(i) != myConfig.updServer) {
          #ifdef CFG_DEBUG
          Serial.println("Request SPIFFS write.");
          #endif
          strlcpy(myConfig.updServer, webServer.arg(i).c_str(), sizeof(myConfig.updServer));
          requestSaveToSpiffs = true;
        } else {
          #ifdef CFG_DEBUG
          Serial.println("Configuration unchanged, do nothing");
          #endif
        }
      }
    }
  }
}

void handleHttpReset(void) {
  systemRestartRequest = true;
  handleWebServerClient();
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
  } else if (topic == myMqttHelper.getTopicTargetTempCmd()) { /* check incoming target temperature, don't set same target temperature as new*/
    if (myThermostat.getTargetTemperature() != floatToInt(payload.toFloat())) {
      /* set new target temperature for heating control, min/max limitation inside (string to float to int16_t) */
      myThermostat.setTargetTemperature(floatToInt(payload.toFloat()));
    }
  } else if (topic == myMqttHelper.getTopicThermostatModeCmd()) {
    if (String("heat") == payload) {
      myThermostat.setThermostatMode(TH_HEAT);
    } else if (String("off") == payload) {
      myThermostat.setThermostatMode(TH_OFF);
    } else {
      /* do nothing */
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
      requestSaveToSpiffsWithRestart = true;
    }
  } else if (topic == myMqttHelper.getTopicChangeSensorCalib()) {
    #ifdef CFG_DEBUG
    Serial.println("New sensor calibration parameters received: " + payload);
    #endif

    int16_t offset;
    int16_t factor;

    if (splitSensorDataString(payload, &offset, &factor)) {
      myThermostat.setSensorCalibData(factor, offset, true);
    }
  } else if (topic == myMqttHelper.getTopicChangeHysteresis()) {
    #ifdef CFG_DEBUG
    Serial.println("New hysteresis parameter received: " + payload);
    #endif

    myThermostat.setThermostatHysteresis(payload.toInt());
  }
}
