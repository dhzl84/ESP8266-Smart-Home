/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include <ArduinoOTA.h>

#include <DHTesp.h>
#include <Wire.h> /* for BME280 */
#include <SPI.h>  /* for BME280 */
#include <BME280I2C.h>
#include "user_fonts.h"
#include <SSD1306.h>
#include "main.h"
#include "c_mqtt.h"
#include "c_thermostat.h"
#include "MQTTClient.h"
#include <Bounce2.h>
#include "time.h"

/*===================================================================================================================*/
/* GPIO config */
/*===================================================================================================================*/
#if CFG_BOARD_ESP8266
ADC_MODE(ADC_VCC);             /* measure Vcc */
#elif CFG_BOARD_ESP32
/* no SW only solution available */
#else
#endif

/*===================================================================================================================*/
/* variables, constants, types, classes, etc. definitions */
/*===================================================================================================================*/
uint32_t wifi_connect_counter = 0;
uint32_t mqtt_connect_counter = 0;
struct Configuration myConfig;
#ifndef MQTT_QOS
  #define MQTT_QOS 1 /* valid values are 0, 1 and 2 */
#endif
uint32_t mqttReconnectTime       = 0;
uint32_t mqttReconnectInterval   = secondsToMilliseconds(5); /* 5 s in milliseconds */
uint32_t mqttPubCycleTime        = 0;
/* loop handler */
#define loop50ms        50
#define loop100ms      100
#define loop500ms      500
#define loop1000ms     secondsToMilliseconds(1)
#define loop1m         minutesToMilliseconds(1)
uint32_t loop50msMillis    =  0;
uint32_t loop100msMillis   = 13; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop500msMillis   = 17; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop1000msMillis  = 19; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop1minuteMillis = 23; /* start loops with some offset to avoid calling all loops every second */
/* HTTP Update */
bool fetch_update = false;       /* global variable used to decide whether an update shall be fetched from server or not */
/* OTA */
typedef enum otaUpdate {
  TH_OTA_IDLE,
  TH_OTA_ACTIVE,
  TH_OTA_FINISHED,
  TH_OTA_ERROR
} OtaUpdate_t; /* global variable used to change display in case OTA update is initiated */
OtaUpdate_t OTA_UPDATE = TH_OTA_IDLE;

/* for critical section lock during interrupt */

#define rotLeft -1
#define rotRight 1
#define rotInit  0
volatile int16_t lastEncoded = 0b11;                   /* initial state of the rotary encoders gray code */
volatile int16_t rotaryEncoderDirectionInts = rotInit; /* initialize rotary encoder with no direction */
const uint32_t buttonDebounceInterval = 25;
uint32_t onOffButtonSystemResetTime = 0;
uint32_t onOffButtonSystemResetInterval = secondsToMilliseconds(5);

/* thermostat */
#define tempStep              5
#define displayTemp           0
#define displayHumid          1
/* sensor */
uint32_t readSensorScheduled = 0;
/* Display */
#define drawTempYOffset       16
#define drawHeating drawXbm(0, drawTempYOffset, myThermo_width, myThermo_height, myThermo)
/* BME 280 settings */
BME280I2C::Settings settings(
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::OSR_X1,
  BME280::Mode_Forced,
  BME280::StandbyTime_1000ms,
  BME280::Filter_Off,
  BME280::SpiEnable_False,
  BME280I2C::I2CAddr_0x77
);

/* classes */
DHTesp            myDHT22;
BME280I2C         myBME280(settings);
SSD1306           myDisplay(0x3c, SDA_PIN, SCL_PIN);
Thermostat        myThermostat;
WiFiClient        myWiFiClient;
mqttHelper        myMqttHelper;
#if CFG_BOARD_ESP8266
ESP8266WebServer  webServer(80);
ESP8266HTTPUpdate myHttpUpdate;
#elif CFG_BOARD_ESP32
WebServer         webServer(80);
HTTPUpdate        myHttpUpdate;
#else
#endif
MQTTClient        myMqttClient(2048); /* 2048 byte buffer */
Bounce            debounceOnOff = Bounce();
Bounce            debounceUp = Bounce();
Bounce            debounceDown = Bounce();
DiffTime          MyLooptime;

uint32_t display_enabled_temporary_reference_time = 0;
#define  display_enabled_temporary_interval secondsToMilliseconds(10)
bool     display_enabled_temporary = false;
bool     systemRestartRequest = false;
bool     requestSaveToSpiffs = false;
bool     requestSaveToSpiffsWithRestart = false;
uint32_t wifiReconnectTimer = secondsToMilliseconds(30);
#define WIFI_RECONNECT_TIME 30

#define  FS_MQTT_ID_FILE        String("/itsme")       // for migration only
#define  FS_SENSOR_CALIB_FILE   String("/sensor")      // for migration only
#define  FS_TARGET_TEMP_FILE    String("/targetTemp")  // for migration only
#define  FS_WRITE_DEBOUNCE      secondsToMilliseconds(20) /* write target temperature to spiffs if it wasn't changed for 20 s (time in ms) */
bool     FS_WRITTEN =           true;
uint32_t FS_REFERENCE_TIME      = 0;

/* Time */
struct tm time_info;
char time_buffer[6];  /* hold the current time in format "%H:%M" */

/*===================================================================================================================*/
/* The setup function is called once at startup of the sketch */
/*===================================================================================================================*/
void setup() {
  #ifdef CFG_DEBUG
  mySerial.begin(115200);
  mySerial.println("SETUP STARTED");
  #endif
  Wire.begin(SDA_PIN, SCL_PIN);   /* needed for I²C communication with display and BME280 */
  FS_INIT();                  /* read stuff from FileSystem */
  GPIO_CONFIG();                  /* configure GPIOs */

  myThermostat.setup(RELAY_PIN,
                     myConfig.target_temperature,
                     myConfig.calibration_factor,
                     myConfig.calibration_offset,
                     myConfig.temperature_hysteresis,
                     myConfig.thermostat_mode);

  SENSOR_INIT();   /* init sensors */
  DISPLAY_INIT();  /* init Display */
  WIFI_CONNECT();  /* connect to WiFi */
  OTA_INIT();      /* init over the air update */
  MQTT_CONNECT();  /* connect to MQTT host and build subscriptions, must be called after FS_INIT()*/
  NTP();            /* init network time protocol, must be called after WIFI_CONNECT() and FS_INIT() */
  MDNS.begin(myConfig.name);

  webServer.begin();
  webServer.on("/", handleWebServerClient);
  webServer.on("/restart", handleHttpReset);

/* button debounce */
  debounceOnOff.attach(PHYS_INPUT_3_PIN);
  debounceOnOff.interval(buttonDebounceInterval);

  if (myConfig.input_method == cPUSH_BUTTONS) {
    debounceUp.attach(PHYS_INPUT_1_PIN);
    debounceUp.interval(buttonDebounceInterval);
    debounceDown.attach(PHYS_INPUT_2_PIN);
    debounceDown.interval(buttonDebounceInterval);
  } else {
  /* enable interrupts on encoder pins to decode gray code and recognize switch event*/
    attachInterrupt(PHYS_INPUT_1_PIN, updateEncoder, CHANGE);
    attachInterrupt(PHYS_INPUT_2_PIN, updateEncoder, CHANGE);
  }

  #ifdef CFG_DEBUG
  #if CFG_BOARD_ESP8266
  mySerial.printf("Vcc: %f", (ESP.getVcc() / 1000.0f));
  mySerial.printf("Reset Reason: %s\n", ESP.getResetReason().c_str());
  mySerial.printf("lash Size: %d\n", ESP.getFlashChipRealSize());
  #elif CFG_BOARD_ESP32
  mySerial.println("CPU Frequency: " + String(ESP.getCpuFreqMHz()));
  mySerial.println("CPU 0 reset reason: " + getEspResetReason(rtc_get_reset_reason(0)));
  mySerial.println("CPU 1 reset reason: " + getEspResetReason(rtc_get_reset_reason(1)));
  #else
  #endif
  mySerial.println("Sketch Size: " + String(ESP.getSketchSize()));
  mySerial.println("Free for Sketch: " + String(ESP.getFreeSketchSpace()));
  mySerial.println("Free Heap: " + String(ESP.getFreeHeap()));
  mySerial.println("SETUP COMPLETE - ENTER LOOP");
  #endif

  SENSOR_MAIN();  /* acquire first sensor data before staring loop() to avoid false value reporting due to current temperature, etc. being the init value until first sensor value is read */
}

/*===================================================================================================================*/
/* functions called by setup()                                                                                       */
/*===================================================================================================================*/

void FS_INIT(void) {  // initializes the FileSystem when first used and loads the configuration from FileSystem to RAM
  FileSystem.begin();

  if (!FileSystem.exists("/formatted")) {
    /* This code is only run once to format the FileSystem before first usage */
    #ifdef CFG_DEBUG
    mySerial.println("Formatting FileSystem, this takes some time");
    #endif
    FileSystem.format();
    #ifdef CFG_DEBUG
    mySerial.println("Formatting FileSystem finished");
    mySerial.println("Open file '/formatted' in write mode");
    #endif
    File f = FileSystem.open("/formatted", "w");
    if (!f) {
      #ifdef CFG_DEBUG
      mySerial.println("file open failed");
      #endif
    } else {
      f.close();
      delay(5000);
      if (!FileSystem.exists("/formatted")) {
        #ifdef CFG_DEBUG
        mySerial.println("That didn't work!");
        #endif
      } else {
        #ifdef CFG_DEBUG
        mySerial.println("Cool, working!");
        #endif
      }
    }
  } else {
    #ifdef CFG_DEBUG
    mySerial.println("Found '/formatted' >> FileSystem ready to use");
    #endif
  }
  #ifdef CFG_DEBUG
  mySerial.println("Check if I remember who I am ... ");
  #endif

  #ifdef CFG_DEBUG
  #if CFG_BOARD_ESP8266
  Dir dir = FileSystem.openDir("/");
  while (dir.next()) {
    mySerial.print("FileSystem file found: " + dir.fileName() + " - Size in byte: ");
    File f = dir.openFile("r");
    mySerial.println(f.size());
  }
  #elif CFG_BOARD_ESP32
  listDir(FileSystem, "/", 0);
  #else
  #endif
  #endif  /* CFG_DEBUG */

  loadConfiguration(myConfig);  // load config

  #ifdef CFG_DEBUG
  mySerial.println("My name is: " + String(myConfig.name));
  #endif
}

void GPIO_CONFIG(void) {  /* initialize encoder / push button pins */
  pinMode(PHYS_INPUT_1_PIN, INPUT_PULLUP);
  pinMode(PHYS_INPUT_2_PIN, INPUT_PULLUP);
  pinMode(PHYS_INPUT_3_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
}

void DISPLAY_INIT(void) {
  #ifdef CFG_DEBUG
  mySerial.println("Initialize display");
  #endif
  myDisplay.init();
  myDisplay.flipScreenVertically();
  myDisplay.setBrightness(myConfig.display_brightness);
  myDisplay.clear();
  myDisplay.setFont(Roboto_Condensed_16);
  myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
  myDisplay.drawString(64, 4, "Initialize");
  myDisplay.drawString(64, 24, String(myConfig.name));
  myDisplay.drawString(64, 44, FW);
  myDisplay.display();
}

void WIFI_CONNECT(void) {
  wifi_connect_counter++;
  if (WiFi.status() != WL_CONNECTED) {
    #ifdef CFG_DEBUG
    mySerial.println("Initialize WiFi ");
    #endif

    WiFi.mode(WIFI_STA);
    #if CFG_BOARD_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.hostname(myConfig.name);
    #elif CFG_BOARD_ESP32
    WiFi.setHostname(myConfig.name);
    WiFi.setSleep(false);
    #endif
    WiFi.begin(myConfig.ssid, myConfig.wifi_password);

    /* try to connect to WiFi, proceed offline if not connecting here*/
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      #ifdef CFG_DEBUG
      mySerial.println("Failed to connect to WiFi, continue offline");
      #endif
    }
  }

  #ifdef CFG_DEBUG
  mySerial.println("WiFi Status: "+ wifiStatusToString(WiFi.status()));
  WiFi.printDiag(mySerial);
  mySerial.println("Local IP: "+ WiFi.localIP().toString());
  mySerial.println("Gateway IP: " + WiFi.gatewayIP().toString());
  mySerial.println("DNS IP: " + WiFi.dnsIP().toString());
  #endif
}

void OTA_INIT(void) {
  ArduinoOTA.setHostname(myConfig.name);
  ArduinoOTA.setPort(8266);
  ArduinoOTA.onStart([]() {
    myMqttClient.disconnect();
    OTA_UPDATE = TH_OTA_ACTIVE;
    DISPLAY_MAIN();
    #ifdef CFG_DEBUG
    mySerial.println("Start OTA");
    #endif
  });

  ArduinoOTA.onEnd([]() {
    OTA_UPDATE = TH_OTA_FINISHED;
    DISPLAY_MAIN();
    #ifdef CFG_DEBUG
    mySerial.println("\nEnd OTA");
    #endif
  });

  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total) {
    #ifdef CFG_DEBUG
    mySerial.printf("Progress: %u%%\r", (progress / (total / 100)));
    #endif
  });

  ArduinoOTA.onError([](ota_error_t error) {
    OTA_UPDATE = TH_OTA_ERROR;
    DISPLAY_MAIN();
    #ifdef CFG_DEBUG
    mySerial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) mySerial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) mySerial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) mySerial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) mySerial.println("Receive Failed");
    else if (error == OTA_END_ERROR) mySerial.println("End Failed");
    #endif
    systemRestartRequest = true;
  });
  ArduinoOTA.begin();
}

void MQTT_CONNECT(void) {
  myMqttHelper.setup(getEspChipId());        /* build MQTT topics based on the defined device name */
  mqtt_connect_counter++;
  if (WiFi.status() == WL_CONNECTED) {
    if (!myMqttClient.connected()) {
      myMqttClient.disconnect();
      myMqttClient.setOptions(30, true, secondsToMilliseconds(30));
      myMqttClient.begin(myConfig.mqtt_host, myConfig.mqtt_port, myWiFiClient);
      myMqttClient.setWill((myMqttHelper.getTopicLastWill()).c_str(),   "offline", true, MQTT_QOS);     /* broker shall publish 'offline' on ungraceful disconnect >> Last Will */
      myMqttClient.onMessage(messageReceived);                                                        /* register callback */
      (void)myMqttClient.connect(myConfig.name, myConfig.mqtt_user, myConfig.mqtt_password);

      homeAssistantRemoveDiscoveredObsolete();  /* for migration from 0.13.x to later versions only */ /* DEPRECATED */
      if (myConfig.discovery_enabled == true) {
        homeAssistantDiscovery();  /* make HA discover necessary devices */
      }

      myMqttClient.publish(myMqttHelper.getTopicLastWill(),             "online", true,  MQTT_QOS);   /* publish online in will topic */
      /* subscribe topics */
      (void)myMqttClient.subscribe(myMqttHelper.getTopicTargetTempCmd(),         MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicThermostatModeCmd(),     MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicUpdateFirmware(),        MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeName(),            MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeHysteresis(),      MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeSensorCalib(),     MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicSystemRestartRequest(),  MQTT_QOS);
      (void)myMqttClient.subscribe(myMqttHelper.getTopicOutsideTemperature(),    MQTT_QOS);
    }
  }
}

/*===================================================================================================================*/
/* The loop function is called in an endless loop */
/*===================================================================================================================*/
void loop() {
  MyLooptime.set_time_start();
  /* call every 50 ms */
  if (TimeReached(loop50msMillis)) {
    SetNextTimeInterval(&loop50msMillis, loop50ms);
    /* nothing yet */
  }
  /* call every 100 ms */
  if (TimeReached(loop100msMillis)) {
    SetNextTimeInterval(&loop100msMillis, loop100ms);
    HANDLE_SYSTEM_STATE();   /* handle connectivity and trigger reconnects */
    myThermostat.loop();     /* control relay for heating */
    DISPLAY_MAIN();          /* draw display each loop */
    MQTT_MAIN();             /* handle MQTT each loop */
  }
  /* call every 500 ms */
  if (TimeReached(loop500msMillis)) {
    SetNextTimeInterval(&loop500msMillis, loop500ms);
    /* nothing yet */
  }
  /* call every second */
  if (TimeReached(loop1000msMillis)) {
    SetNextTimeInterval(&loop1000msMillis, loop1000ms);
    SENSOR_MAIN();          /* get sensor data */
    FS_MAIN();          /* check for new data and write FileSystem is necessary */
    HANDLE_HTTP_UPDATE();   /* pull update from server if it was requested via MQTT*/
  }

  /* call every minute */
  if (TimeReached(loop1minuteMillis)) {
    SetNextTimeInterval(&loop1minuteMillis, loop1m);
    NTP();                 /* check time regularly and change DST is necessary */
  }
  /* debounce buttons */
  debounceOnOff.update();
  if (myConfig.input_method == cPUSH_BUTTONS) {
    debounceUp.update();
    debounceDown.update();
  }
  if (debounceOnOff.fell()) {
    if (myConfig.display_enabled || display_enabled_temporary) {
      myThermostat.toggleThermostatMode();
    }
    SetNextTimeInterval(&display_enabled_temporary_reference_time, display_enabled_temporary_interval);
  }
  if (debounceUp.fell()) {
    if (myConfig.display_enabled || display_enabled_temporary) {
      myThermostat.increaseTargetTemperature(tempStep);
    }
    SetNextTimeInterval(&display_enabled_temporary_reference_time, display_enabled_temporary_interval);
  }
  if (debounceDown.fell()) {
    if (myConfig.display_enabled || display_enabled_temporary) {
      myThermostat.decreaseTargetTemperature(tempStep);
    }
    SetNextTimeInterval(&display_enabled_temporary_reference_time, display_enabled_temporary_interval);
  }
  if (TimeReached(display_enabled_temporary_reference_time)) {
    display_enabled_temporary = false;
  } else {
    display_enabled_temporary = true;
  }

  webServer.handleClient();
  myMqttClient.loop();
  ArduinoOTA.handle();

  MyLooptime.set_time_end();
}

/*===================================================================================================================*/
/* functions called by loop() */
/*===================================================================================================================*/
void HANDLE_SYSTEM_STATE(void) {
  /* check WiFi connection every 30 seconds*/
  if (TimeReached(wifiReconnectTimer)) {
    SetNextTimeInterval(&wifiReconnectTimer, (secondsToMilliseconds(WIFI_RECONNECT_TIME)));
    if (WiFi.status() != WL_CONNECTED) {
      #ifdef CFG_DEBUG
      mySerial.println("Lost WiFi; Status: "+ String(WiFi.status()));
      #endif
      /* try to come online, debounced to avoid reconnect each loop*/
      WIFI_CONNECT();
    }
  }
  /* check if button is pushed for 10 seconds to request a reset */
  if (LOW == digitalRead(PHYS_INPUT_3_PIN)) {
    #ifdef CFG_DEBUG_ENCODER
    mySerial.println("Encoder Button pressed: "+ String(digitalRead(PHYS_INPUT_3_PIN)));
    mySerial.println("Target Time: "+ String(onOffButtonSystemResetTime + onOffButtonSystemResetInterval));
    mySerial.println("Current time: "+ String(millis()));
    #endif

    if ((onOffButtonSystemResetTime + onOffButtonSystemResetInterval) < millis()) {
      systemRestartRequest = true;
    } else {
      /* waiting */
    }
  } else {
    onOffButtonSystemResetTime = millis();
  }

  /* restart handling */
  if (systemRestartRequest == true) {
    DISPLAY_MAIN();
    myMqttClient.publish(myMqttHelper.getTopicSystemRestartRequest(), "false",      false, MQTT_QOS);   /* publish restart = false on connect */
    myMqttClient.loop();
    systemRestartRequest = false;
    #ifdef CFG_DEBUG
    mySerial.println("Restarting in 3 seconds");
    #endif
    myMqttClient.disconnect();
    delay(secondsToMilliseconds(3));
    ESP.restart();
  }
}

void NTP(void) {
  const char* ntp_server[] = { "fritz.box", "0.de.pool.ntp.org", "0.ch.pool.ntp.org" };  // WiFi.gatewayIP().toString().c_str()
  #ifdef CFG_DEBUG_SNTP
  mySerial.print("NTP Servers: ");
  for (auto server : ntp_server) {
    mySerial.printf("%s ", server);
  }
  mySerial.println();
  mySerial.println("UTC Offset: " + String(myConfig.utc_offset));
  mySerial.println("DST : " + String(myConfig.daylight_saving_time));
  #endif  /* CFG_DEBUG_SNTP */

  /* UTC and DST are defined in hours, configTime expects seconds, thus multiply with 3600 */
  configTime((myConfig.utc_offset * 3600), (static_cast<uint8_t>(myConfig.daylight_saving_time) * 3600), ntp_server[0], ntp_server[1], ntp_server[2]);

  #ifdef CFG_BOARD_ESP8266
  /* try every second to get an answer from the NTP server for max. 15 seconds */
  bool server_reachable = false;
  uint32_t ntp_start_time = millis();
  uint32_t millis_delta = millis() - ntp_start_time;
  do {
    delay(25);
    millis_delta = millis() - ntp_start_time;
    if (millis_delta >= 1000) {
      #ifdef CFG_DEBUG_SNTP
      mySerial.printf("Waiting for NTP-Answer %02u sec\n", millis_delta / 1000);
      #endif  /* CFG_DEBUG_SNTP */
      delay(975);
    }

    for (uint8_t server_id = 0; server_id < 3 ; server_id++) {
      if (sntp_getreachability(server_id)) {
        #ifdef CFG_DEBUG_SNTP
        mySerial.printf("NTP Server %s not reachable!\n", ntp_server[server_id]);
        #endif  /* CFG_DEBUG_SNTP */
      } else {
        server_reachable = true;
      }
    }
  } while ((millis_delta <= (secondsToMilliseconds(30))) && !server_reachable);

  #ifdef CFG_DEBUG_SNTP
  // mySerial.printf("SNTP connected to: %s\n", sntp_getserver());
  #endif  /* CFG_DEBUG_SNTP */
  #endif  /* CFG_BOARD_ESP8266 */

  // In the EU, EFTA and associated countries, European Summer Time begins at 01:00 UTC/WET (02:00 CET, 03:00 EET) on the last Sunday in March and ends at 01:00 UTC (02:00 WEST, 03:00 CEST, 04:00 EEST) on the last Sunday in October each year */
  // Sunday: time_info.tm_wday == 7
  // Last Sunday: time_info.tm_mday >= 25 (must be in the range of 25th to 31st)
  // March: time_info.tm_mon == 2
  // Hour: time_info.tm_hour - myConfig.utc_offset == 1

  // variable used for determination of actual DST
  bool local_daylight_saving_time = myConfig.daylight_saving_time;
  bool local_ntp_time_received = false;

  // calculate if the last Sunday is still to come
  // positive values: last sunday is still to come
  // zero: is is Sunday
  // negative values: last sunday is already gone
  int8_t day_of_dst_change = ((31 - (time_info.tm_mday - 1)) - time_info.tm_wday > 0);

  if (time_info.tm_year >= (2016 - 1900)) {
    local_ntp_time_received = true;
  } else {
    #ifdef CFG_DEBUG
    mySerial.println("Failed to obtain time from NTP");
    #endif /* CFG_DEBUG */
  }

  #ifdef CFG_DEBUG_SNTP
  mySerial.println("DST: " + String(time_info.tm_isdst));
  mySerial.println("YDay: " + String(time_info.tm_yday));
  mySerial.println("WDay: " + String(time_info.tm_wday));
  mySerial.println("Year: " + String(time_info.tm_year));
  mySerial.println("Month: " + String(time_info.tm_mon));
  mySerial.println("Day: " + String(time_info.tm_mday));
  mySerial.println("Hour: " + String(time_info.tm_hour));
  mySerial.println("Min: " + String(time_info.tm_min));
  mySerial.println("Sec: " + String(time_info.tm_sec));
  mySerial.println("Time: " + String(time_buffer));
  #endif  /* CFG_DEBUG_SNTP */

  if (local_ntp_time_received == true) {
    // April to September is always DST
    if (time_info.tm_mon > 2 && time_info.tm_mon < 9) {
      local_daylight_saving_time = 1;
      #ifdef CFG_DEBUG_SNTP
      mySerial.println("April to September - DST: " + String(local_daylight_saving_time));
      #endif  /* CFG_DEBUG_SNTP */
    // November to February is always not DST
    } else if (time_info.tm_mon > 9 || time_info.tm_mon < 2) {
      local_daylight_saving_time = 0;
      #ifdef CFG_DEBUG_SNTP
      mySerial.println("November to February - DST: " + String(local_daylight_saving_time));
      #endif  /* CFG_DEBUG_SNTP */
    // March
    } else if (time_info.tm_mon == 2) {
      #ifdef CFG_DEBUG_SNTP
      mySerial.print("March");
      #endif  /* CFG_DEBUG_SNTP */
      // DST coming
      if (day_of_dst_change > 0) {
        #ifdef CFG_DEBUG_SNTP
        mySerial.print(" before DST change");
        #endif  /* CFG_DEBUG_SNTP */
        local_daylight_saving_time = 0;
      // DST gone
      } else if (day_of_dst_change < 0) {
        #ifdef CFG_DEBUG_SNTP
        mySerial.print(" after DST change");
        #endif  /* CFG_DEBUG_SNTP */
        local_daylight_saving_time = 1;
      // DST change today
      } else {
        // UTC hour is 1 or later
        #ifdef CFG_DEBUG_SNTP
        mySerial.print(" DST change today");
        #endif  /* CFG_DEBUG_SNTP */
        if ((time_info.tm_hour - myConfig.utc_offset - static_cast<uint8_t>(myConfig.daylight_saving_time)) >= 1) {
          #ifdef CFG_DEBUG_SNTP
          mySerial.print(" - pending");
          #endif  /* CFG_DEBUG_SNTP */
          local_daylight_saving_time = 1;
        // UTC hour is before 1
        } else {
          #ifdef CFG_DEBUG_SNTP
          mySerial.print(" - done");
          #endif  /* CFG_DEBUG_SNTP */
          local_daylight_saving_time = 0;
        }
      }
      #ifdef CFG_DEBUG_SNTP
      mySerial.println("");
      #endif  /* CFG_DEBUG_SNTP */
    // October
    } else if (time_info.tm_mon == 9) {
      #ifdef CFG_DEBUG_SNTP
      mySerial.print("October");
      #endif  /* CFG_DEBUG_SNTP */
      // not DST coming
      if (day_of_dst_change > 0) {
        #ifdef CFG_DEBUG_SNTP
        mySerial.print(" before DST change");
        #endif  /* CFG_DEBUG_SNTP */
        local_daylight_saving_time = 1;
      // not DST gone
      } else if (day_of_dst_change < 0) {
        #ifdef CFG_DEBUG_SNTP
        mySerial.print(" after DST change");
        #endif  /* CFG_DEBUG_SNTP */
        local_daylight_saving_time = 0;
      // not DST change today
      } else {
        #ifdef CFG_DEBUG_SNTP
        mySerial.print(" DST change today");
        #endif  /* CFG_DEBUG_SNTP */
        // UTC hour is 1 or later
        if ((time_info.tm_hour - myConfig.utc_offset - static_cast<uint8_t>(myConfig.daylight_saving_time)) >= 1) {
          #ifdef CFG_DEBUG_SNTP
          mySerial.print(" - pending");
          #endif  /* CFG_DEBUG_SNTP */
          local_daylight_saving_time = 0;
        // UTC hour is before 1
        } else {
          #ifdef CFG_DEBUG_SNTP
          mySerial.print(" - done");
          #endif  /* CFG_DEBUG_SNTP */
          local_daylight_saving_time = 1;
        }
      }
      #ifdef CFG_DEBUG_SNTP
      mySerial.println("");
      #endif  /* CFG_DEBUG_SNTP */
    }
  }

  // if actual DST differs from the stored one, change it, persist it and re-initialze NTP with new DST flag
  if (myConfig.daylight_saving_time != local_daylight_saving_time) {
    #ifdef CFG_DEBUG_SNTP
    mySerial.println("DST change calculated");
    mySerial.println("DST old: " + String(myConfig.daylight_saving_time));
    mySerial.println("DST new: " + String(local_daylight_saving_time));
    #endif  /* CFG_DEBUG_SNTP */
    myConfig.daylight_saving_time = local_daylight_saving_time;
    requestSaveToSpiffs = true;
    /* UTC and DST are defined in hours, configTime expects seconds, thus multiply with 3600 */
    configTime((myConfig.utc_offset * 3600), (static_cast<uint8_t>(myConfig.daylight_saving_time) * 3600), ntp_server[0], ntp_server[1], ntp_server[2]);
  }
  updateTimeBuffer();
}

void SENSOR_INIT() {
  if (myConfig.sensor_type == cBME280) {
    if (!myBME280.begin()) { /* init BMP sensor */
      #ifdef CFG_DEBUG
      mySerial.println("Could not find a valid BME sensor!");
      #endif
    } else {
      myBME280.setSettings(settings);
    }
  } else if (myConfig.sensor_type == cDHT22) {
      myDHT22.setup(DHT_PIN, DHTesp::DHT22); /* init DHT sensor */
      mySerial.println("DHT22 init: " + String(myDHT22.getStatusString()));
  } else {
      #ifdef CFG_DEBUG
      mySerial.println("Sensor misconfiguration!");
      #endif
  }
}

void SENSOR_MAIN() {
  /* schedule routine for sensor read */
  if (TimeReached(readSensorScheduled)) {
    SetNextTimeInterval(&readSensorScheduled, (secondsToMilliseconds(myConfig.sensor_update_interval)));

    float sensTemp(NAN), sensHumid(NAN);

    if (myConfig.sensor_type == cDHT22) {
      sensHumid = myDHT22.getHumidity();
      sensTemp  = myDHT22.getTemperature();
    } else if (myConfig.sensor_type == cBME280) {  /* BME280 */
      BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
      BME280::PresUnit presUnit(BME280::PresUnit_hPa);
      float sensPres(NAN);  /* TODO: implement pressure from BME280 */
      myBME280.read(sensPres, sensTemp, sensHumid, tempUnit, presUnit);
    } else {
      /* TODO: Houston we have a problem! */
    }

    /* Check if any reads failed */
    if (isnan(sensHumid) || isnan(sensTemp)) {
      myThermostat.setLastSensorReadFailed(true);   /* set failure flag and exit SENSOR_MAIN() */
      #ifdef CFG_DEBUG
      mySerial.println("Failed to read from sensor! Failure counter: " + String(myThermostat.getSensorFailureCounter()));
      #endif
    } else {
      myThermostat.setLastSensorReadFailed(false);   /* set no failure during read sensor */
      myThermostat.setCurrentHumidity((int16_t)(10* sensHumid));     /* read value and convert to one decimal precision integer */
      myThermostat.setCurrentTemperature((int16_t)(10* sensTemp));   /* read value and convert to one decimal precision integer */

      #ifdef CFG_DEBUG_SENSOR_VALUES
      mySerial.print("Temperature: ");
      mySerial.print(intToFloat(myThermostat.getCurrentTemperature()), 1);
      mySerial.println(" *C ");
      mySerial.print("Filtered temperature: ");
      mySerial.print(intToFloat(myThermostat.getFilteredTemperature()), 1);
      mySerial.println(" *C ");

      mySerial.print("Humidity: ");
      mySerial.print(intToFloat(myThermostat.getCurrentHumidity()), 1);
      mySerial.println(" %");
      mySerial.print("Filtered humidity: ");
      mySerial.print(intToFloat(myThermostat.getFilteredHumidity()), 1);
      mySerial.println(" %");
      #endif  /* CFG_DEBUG_SENSOR_VALUES */

      #ifdef CFG_PRINT_TEMPERATURE_QUEUE
      for (int16_t i = 0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
        int16_t temperature;
        int16_t humidity;
        if ( (myTemperatureFilter.getRawSampleValue(i, &temperature)) && (myHumidityFilter.getRawSampleValue(i, &humidity)) ) {
          mySerial.print("Sample ");
          mySerial.print(i);
          mySerial.print(": ");
          mySerial.print(intToFloat(temperature), 1);
          mySerial.print(", ");
          mySerial.println(intToFloat(humidity), 1);
        }
      }
      #endif
    }
  }
}

void DISPLAY_MAIN(void) {
  myDisplay.clear();
  if (myConfig.display_enabled || display_enabled_temporary) {
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
    } else if (fetch_update == true) { /* HTTP Update */
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      myDisplay.drawString(64, 22, "Update");
    } else {
      myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
      myDisplay.setFont(Roboto_Condensed_32);

      if (myThermostat.getSensorError()) {
        myDisplay.drawString(128, drawTempYOffset, "err");
      } else if (myThermostat.getFilteredTemperature() == INT16_MIN) {
        myDisplay.drawString(128, drawTempYOffset, "init");
      } else {
        myDisplay.drawString(128, drawTempYOffset, String(intToFloat(myThermostat.getFilteredTemperature()), 1));
      }

      /* display outside temperature in top left corner if available, INT16_MIN is the initial value and pretty unlikely to be a real value */
      if (myThermostat.getOutsideTemperature() != INT16_MIN) {
        myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
        myDisplay.setFont(Roboto_Condensed_16);
        myDisplay.drawString(0, 0, String(intToFloat(myThermostat.getOutsideTemperature()), 1));
      }

      /* do not display target temperature if heating is not allowed */
      if (myThermostat.getThermostatMode() == TH_HEAT) {
        myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
        myDisplay.setFont(Roboto_Condensed_16);
        myDisplay.drawString(128, 0, String(intToFloat(myThermostat.getTargetTemperature()), 1));

        if (myThermostat.getActualState()) { /* heating */
          myDisplay.drawHeating;
        }
      }
    }

    /* display current time */
    myDisplay.setFont(Roboto_Condensed_16);
    myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
    myDisplay.drawString(0, 48, String(time_buffer));

    #ifdef CFG_DEBUG_DISPLAY_VERSION
    myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
    myDisplay.setFont(Roboto_Condensed_16);
    myDisplay.drawString(0, 16, String(VERSION));
    myDisplay.drawString(0, 32, String(BUILD_NUMBER));
    myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
    myDisplay.drawString(128, 48, String("IPv4: " + (WiFi.localIP().toString()).substring(((WiFi.localIP().toString()).lastIndexOf(".") + 1), (WiFi.localIP().toString()).length())));
    #endif
  }
  myDisplay.display();
}

void MQTT_MAIN(void) {
  if (myMqttClient.connected() != true) {
    if (TimeReached(mqttReconnectTime)) {  /* try reconnect to MQTT broker after mqttReconnectTime expired */
        SetNextTimeInterval(&mqttReconnectTime, mqttReconnectInterval); /* reset interval */
        MQTT_CONNECT();
        mqttPubState();
    } else {
      /* just wait */
    }
  } else {  /* check if there is new data to publish and shift PubCycle if data is published on event, else publish every PubCycleInterval */
    if ( TimeReached(mqttPubCycleTime) || myThermostat.getNewData() ) {
        SetNextTimeInterval(&mqttPubCycleTime, minutesToMilliseconds(myConfig.mqtt_publish_cycle));
        mqttPubState();
        myThermostat.resetNewData();
    }
    if (myMqttHelper.getTriggerDiscovery()) {
      homeAssistantDiscovery();  /* make HA discover/update necessary devices at runtime e.g. after name change */
      myMqttHelper.setTriggerDiscovery(false);
    }
    if (myMqttHelper.getTriggerRemoveDiscovered()) {
      homeAssistantRemoveDiscovered();  /* make HA remove entities */
      myMqttHelper.setTriggerRemoveDiscovered(false);
    }
  }
}

void HANDLE_HTTP_UPDATE(void) {
  if (fetch_update == true) {
    /* publish and loop here before fetching the update */
    myMqttClient.publish(myMqttHelper.getTopicUpdateFirmwareAccepted(), String(false), false, MQTT_QOS); /* publish accepted update with value false to reset the switch in Home Assistant */
    myMqttClient.loop();

    DISPLAY_MAIN();
    fetch_update = false;
    #ifdef CFG_DEBUG
    mySerial.println("Remote update started");
    #endif
    WiFiClient client;
    t_httpUpdate_return ret = myHttpUpdate.update(client, myConfig.update_server_address, FW);

    switch (ret) {
    case HTTP_UPDATE_FAILED:
      #ifdef CFG_DEBUG
      mySerial.printf("HTTP_UPDATE_FAILD Error (%d): %s \n", myHttpUpdate.getLastError(), myHttpUpdate.getLastErrorString().c_str());
      #endif
      break;

    case HTTP_UPDATE_NO_UPDATES:
      #ifdef CFG_DEBUG
      mySerial.println("HTTP_UPDATE_NO_UPDATES");
      #endif
      break;

    case HTTP_UPDATE_OK:
      #ifdef CFG_DEBUG
      mySerial.println("HTTP_UPDATE_OK");
      #endif
      break;
    }
  }
}

void FS_MAIN(void) {
  /* force FileSystem write with restart, this is necessary for SW reconfiguration in certain cases like changing the input method or changing the decives name */

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
  /* avoid extensive writing to FileSystem, therefore check if the target temperature didn't change for a certain time before writing. */
  if ( (myThermostat.getNewCalib()) || (myThermostat.getTargetTemperature() != myConfig.target_temperature) || (myThermostat.getThermostatHysteresis() != myConfig.temperature_hysteresis) || (myThermostat.getThermostatMode() != myConfig.thermostat_mode) ) {
    FS_REFERENCE_TIME = millis();  // ToDo: handle wrap around
    myConfig.target_temperature  = myThermostat.getTargetTemperature();
    myConfig.calibration_factor = myThermostat.getSensorCalibFactor();
    myConfig.calibration_offset = myThermostat.getSensorCalibOffset();
    myConfig.temperature_hysteresis  = myThermostat.getThermostatHysteresis();
    myConfig.thermostat_mode   = myThermostat.getThermostatMode();
    myThermostat.resetNewCalib();
    FS_WRITTEN = false;
    #ifdef CFG_DEBUG
    mySerial.println("FileSystem to be stored after debounce time: " + String(FS_WRITE_DEBOUNCE));
    #endif /* CFG_DEBUG */
  } else {
    /* no spiffs data changed this loop */
  }
  if (FS_WRITTEN == true) {
    /* do nothing, last change was already stored in FileSystem */
  } else { /* latest change not stored in FileSystem */
    if (FS_REFERENCE_TIME + FS_WRITE_DEBOUNCE < millis()) { /* check debounce */
      /* debounce expired -> write */
      if (saveConfiguration(myConfig)) {
        FS_WRITTEN = true;
      } else {
        /* FileSystem not written, retry next loop */
        #ifdef CFG_DEBUG
        mySerial.println("FileSystem write failed");
        #endif /* CFG_DEBUG */
      }
    } else {
      /* debounce FileSystem write */
    }
  }
}
/*===================================================================================================================*/
/* callback, interrupt, timer, other functions */
/*===================================================================================================================*/

/* Rotary Encoder */
void ICACHE_RAM_ATTR updateEncoder(void) {
  int16_t MSB = digitalRead(PHYS_INPUT_1_PIN);
  int16_t LSB = digitalRead(PHYS_INPUT_2_PIN);
  int16_t encoded = (MSB << 1) |LSB;  /* converting the 2 pin value to single number */
  int16_t sum  = (lastEncoded << 2) | encoded; /* adding it to the previous encoded value */

  if (myConfig.display_enabled || display_enabled_temporary) {
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) { /* gray patterns for rotation to right */
      rotaryEncoderDirectionInts += rotRight; /* count rotation interrupts to left and right, this shall make the encoder routine more robust against bouncing */
    }
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) { /* gray patterns for rotation to left */
      rotaryEncoderDirectionInts += rotLeft;
    }

    if (encoded == 0b11) { /* if the encoder is in an end position, evaluate the number of interrupts to left/right. simple majority wins */
      #ifdef CFG_DEBUG_ENCODER
      mySerial.println("Rotary encoder interrupts: " + String(rotaryEncoderDirectionInts));
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
  SetNextTimeInterval(&display_enabled_temporary_reference_time, display_enabled_temporary_interval);
}

/* Home Assistant discovery_enabled on connect; used to define entities in HA to communicate with*/
void homeAssistantDiscovery(void) {
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate(),                         myMqttHelper.buildHassDiscoveryClimate(String(myConfig.name), String(FW), String(BOARD)),    true, MQTT_QOS);    // make HA discover the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(kThermostatState),    myMqttHelper.buildHassDiscoveryBinarySensor(String(myConfig.name), kThermostatState),        true, MQTT_QOS);    // make HA discover the binary_sensor for thermostat state
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kTemp),                     myMqttHelper.buildHassDiscoverySensor(String(myConfig.name),  kTemp),                        true, MQTT_QOS);    // make HA discover the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kHum),                      myMqttHelper.buildHassDiscoverySensor(String(myConfig.name),  kHum),                         true, MQTT_QOS);    // make HA discover the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(kRestart),                  myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name),  kRestart),                     true, MQTT_QOS);    // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(kUpdate),                   myMqttHelper.buildHassDiscoverySwitch(String(myConfig.name),  kUpdate),                      true, MQTT_QOS);    // make HA discover the update switch
}

/* Make Home Assistant forget the discovered entities on demand */
void homeAssistantRemoveDiscovered(void) {
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate(),                         String(""),         true, MQTT_QOS);    // make HA forget the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(kThermostatState),    String(""),         true, MQTT_QOS);    // make HA discover the binary_sensor for thermostat state
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kTemp),                     String(""),         true, MQTT_QOS);    // make HA forget the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kHum),                      String(""),         true, MQTT_QOS);    // make HA forget the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(kRestart),                  String(""),         true, MQTT_QOS);    // make HA forget the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(kUpdate),                   String(""),         true, MQTT_QOS);    // make HA forget the update switch
  homeAssistantRemoveDiscoveredObsolete();
}

/* DEPRECATED */
void homeAssistantRemoveDiscoveredObsolete(void) {
  /* for migration from 0.13.x to later versions only */
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(kSensFail),           String(""),         true, MQTT_QOS);    // make HA discover the binary_sensor for sensor failure
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kIP),                       String(""),         true, MQTT_QOS);    // make HA discover the IP sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kCalibF),                   String(""),         true, MQTT_QOS);    // make HA discover the scaling sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kCalibO),                   String(""),         true, MQTT_QOS);    // make HA discover the offset sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kHysteresis),               String(""),         true, MQTT_QOS);    // make HA discover the hysteresis sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kFW),                       String(""),         true, MQTT_QOS);    // make HA discover the firmware version sensor
}

/* publish state topic in JSON format */
void mqttPubState(void) {
  String payload = myMqttHelper.buildStateJSON( /* build JSON payload */\
      String(myConfig.name), \
      String(intToFloat(myThermostat.getFilteredTemperature()), 1), \
      String(intToFloat(myThermostat.getFilteredHumidity()), 1), \
      String(intToFloat(myThermostat.getThermostatHysteresis()), 1), \
      boolToStringOnOff(myThermostat.getActualState()), \
      String(intToFloat(myThermostat.getTargetTemperature()), 1), \
      sensErrorToString(myThermostat.getSensorError()), \
      boolToStringHeatOff(myThermostat.getThermostatMode()), \
      String(myThermostat.getSensorCalibFactor()), \
      String(intToFloat(myThermostat.getSensorCalibOffset()), 0), \
      WiFi.localIP().toString(), \
      String(FW) );

  myMqttClient.publish( \
    myMqttHelper.getTopicData(), /* get topic */ \
    payload, \
    true, /* retain */ \
    MQTT_QOS); /* QoS */
}

/* time_buffer holds the current time in the %H:%M format */
void updateTimeBuffer(void) {
  time_t now;
  time(&now);
  (void) localtime_r(&now, &time_info);
  const char* time_format = "%H:%M";
  strftime(time_buffer, sizeof(time_buffer), time_format, &time_info);
}

String buildHtml(void) {
  float rssiInPercent = WiFi.RSSI();
  rssiInPercent = isnan(rssiInPercent) ? -100.0 : min(max(2 * (rssiInPercent + 100.0), 0.0), 100.0);

  String webpage = "<!DOCTYPE html> <html>\n";
  /* HEAD */
  webpage +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  webpage +="<title> "+ String(myConfig.name) + "</title>\n";
  webpage +="<style>html { font-family: Helvetica; font-size: 16px; display: inline-block; margin: 0px auto; text-align: left;}\n";
  webpage +="body {background-color: #202226; color: #A0A2A8}\n";
  webpage +="p {color: #2686c1; margin-bottom: 10px;}\n";
  webpage +="table {color: #A0A2A8;}\n";
  webpage +="</style>\n";
  webpage +="</head>\n";
  /* BODY */
  webpage +="<body>\n";
  webpage +="<p><b>System information</b></p>";
  webpage +="<table>";
  /* TABLE */
  /*= KEY ======================================||= VALUE ================================*/
  webpageTableAppend2Cols(String("Name"),                 String(myConfig.name));
  webpageTableAppend2Cols(String("Chip ID"),              getEspChipId());
  #if CFG_BOARD_ESP8266
  webpageTableAppend2Cols(String("Vcc"),                  String(ESP.getVcc()/1000.0));
  webpageTableAppend2Cols(String("Arduino Core"),         ESP.getCoreVersion());
  webpageTableAppend2Cols(String("Reset Reason"),         ESP.getResetReason());
  webpageTableAppend2Cols(String("Flash Size"),           String(ESP.getFlashChipRealSize()));
  #elif CFG_BOARD_ESP32
  webpageTableAppend2Cols(String("Reset Reason CPU 0"),   getEspResetReason(rtc_get_reset_reason(0)));
  webpageTableAppend2Cols(String("Reset Reason CPU 1"),   getEspResetReason(rtc_get_reset_reason(1)));
  webpageTableAppend2Cols(String("CPU Frequency"),        String(ESP.getCpuFreqMHz() + String(" MHz")));
  webpageTableAppend2Cols(String("Flash Speed"),          String(ESP.getFlashChipSpeed()/1000000)  + String(" MHz"));
  webpageTableAppend2Cols(String("Flash Size"),           String(ESP.getFlashChipSize()));
  webpageTableAppend2Cols(String("Flash Mode"),           String(ESP.getFlashChipMode()));
  webpageTableAppend2Cols(String("SDK Verison"),          String(ESP.getSdkVersion()));
  #else
  #endif
  webpageTableAppend2Cols(String("Sketch Size"),          String(ESP.getSketchSize()/1024)  + String(" kB"));
  webpageTableAppend2Cols(String("Free for Sketch"),      String(ESP.getFreeSketchSpace()/1024)  + String(" kB"));
  webpageTableAppend2Cols(String("Free Heap"),            String(ESP.getFreeHeap()/1024)  + String(" kB"));
  webpageTableAppend2Cols(String("Time since Reset"),     millisFormatted());
  webpageTableAppend2Cols(String("FW version"),           String(FW));
  webpageTableAppend2Cols(String("IPv4"),                 WiFi.localIP().toString());
  webpageTableAppend2Cols(String("WiFi Status"),          wifiStatusToString(WiFi.status()));
  webpageTableAppend2Cols(String("WiFi Strength"),        String(rssiInPercent, 0) + " %");
  webpageTableAppend2Cols(String("WiFi Connects"),        String(wifi_connect_counter));
  webpageTableAppend2Cols(String("MQTT Status"),          String((myMqttClient.connected()) == true ? "connected" : "disconnected"));
  webpageTableAppend2Cols(String("MQTT Connects"),        String(mqtt_connect_counter));
  webpageTableAppend2Cols(String("Local Time"),           String(time_buffer));
  webpageTableAppend2Cols(String("Looptime mean [&micro;s]"),   String(MyLooptime.get_time_duration_mean()));
  webpageTableAppend2Cols(String("Looptime min [&micro;s]"),    String(MyLooptime.get_time_duration_min()));
  webpageTableAppend2Cols(String("Looptime max [&micro;s]"),    String(MyLooptime.get_time_duration_max()));
  webpage +="</table>";
  /* Change Input Method */
  webpage +="<p><b>Change Input Method</b></p>";
  webpage +="<form method='POST' autocomplete='off'>";

  if (myConfig.input_method == cPUSH_BUTTONS) {
    webpage +="<select name='InputMethod'> <option value='0'>Rotary Encoder </option> <option value='1' selected> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>";
  } else {
    webpage +="<select name='InputMethod'> <option value='0' selected> Rotary Encoder </option> <option value='1'> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>";
  }
  webpage +="</form>";
  /* Change sensor */
  webpage +="<p><b>Change Sensor</b></p>";
  webpage +="<form method='POST' autocomplete='off'>";
  webpage +="<select name='Sensor'> ";
  if (myConfig.sensor_type == cDHT22) {  /* select currently configured input method */
  webpage +="<option value='0' selected >DHT22 </option> <option value='1'> BME280 </option>";
  } else {
  webpage +="<option value='0'>DHT22 </option> <option value='1' selected > BME280 </option>";
  }
  webpage +="</select>&nbsp;<input type='submit' value='Submit'>";
  webpage +="</form>";

  /* Command Line */
  webpage +="<p><b>Command Line</b></p>";
  webpage +="<form method='POST' autocomplete='off'>";
  webpage +="<input type='text' name='cmd' value='key:value'>&nbsp;<input type='submit' value='Submit'>";
  webpage +="</form>";
  webpage +="<p><b>Commands</b></p>";
  /* COMMANDS TABLE */
  webpage +="<table style='font-size: 12px'>";
  webpageTableAppend4Cols(String("<b>Key</b>"),               String("<b>Value</b>"),                        String("<b>Current Value</b>"),             String("<b>Description</b>"));
  webpageTableAppend4Cols(String("discover"),                 String("0 | 1"),                               String("void"),                             String("MQTT OnDemand discovery_enabled: 1 = discover; 0 = remove"));
  webpageTableAppend4Cols(String("discovery_enabled"),        String("0 | 1"),                               String(myConfig.discovery_enabled),         String("MQTT Autodiscovery: 1 = enabled; 0 = disabled"));
  webpageTableAppend4Cols(String("name"),                     String("string"),                              String(myConfig.name),                      String("Define a name for this device"));
  webpageTableAppend4Cols(String("mqtt_server_address"),      String("string"),                              String(myConfig.mqtt_host),                 String("Address of the MQTT server"));
  webpageTableAppend4Cols(String("mqtt_server_port"),         String("1 .. 65535"),                          String(myConfig.mqtt_port),                 String("Port of the MQTT server"));
  webpageTableAppend4Cols(String("mqtt_user"),                String("string"),                              String(myConfig.mqtt_user),                 String("MQTT user"));
  webpageTableAppend4Cols(String("mqtt_pwd"),                 String("string"),                              String("***"),                              String("MQTT password"));
  webpageTableAppend4Cols(String("mqtt_pub_cycle"),           String("Range: 1 .. 255, LSB: 1 min"),         String(myConfig.mqtt_publish_cycle),        String("Publish cycle for MQTT messages in minutes"));
  webpageTableAppend4Cols(String("update_server_address"),    String("string"),                              String(myConfig.update_server_address),     String("Address of the update server"));
  webpageTableAppend4Cols(String("calibration_offset"),       String("Range: -50 .. +50, LSB: 0.1 &deg;C"),  String(myConfig.calibration_offset),        String("Offset calibration for temperature sensor."));
  webpageTableAppend4Cols(String("calibration_factor"),       String("Range: +50 .. +200, LSB: 1 %"),        String(myConfig.calibration_factor),        String("Linearity calibration for temperature sensor."));
  webpageTableAppend4Cols(String("display_enabled"),          String("0 | 1"),                               String(myConfig.display_enabled),           String("Display always on or only on user interaction"));
  webpageTableAppend4Cols(String("display_brightness"),       String("Range: 0 .. +255, LSB: 1 step"),       String(myConfig.display_brightness),        String("Brightness of OLED display"));
  webpageTableAppend4Cols(String("fetch_update"),             String("0 | 1"),                               String(fetch_update),                       String("Trigger download and install binary from update server: 1 = fetch; 0 = do nothing"));
  webpage +="</table>";
  /* Restart Device */
  webpage +="<p><b>Restart Device</b></p>";
  if (systemRestartRequest == false) {
    webpage +="<button onclick=\"window.location.href='/restart'\"> Restart </button>";
  } else {
    webpage +="<button onclick=\"window.location.href='/'\"> Restarting, reload site </button>";
  }
  webpage +="</body>\n";
  webpage +="</html>\n";

  return webpage;
}

void handleWebServerClient(void) {
  String webpage = buildHtml();

  webServer.send(200, "text/html", webpage);  /* Send a response to the client asking for input */

  /* Evaluate Received Arguments */
  if (webServer.args() > 0) {  /* Arguments were received */
    for ( uint8_t i = 0; i < webServer.args(); i++ ) {
      #ifdef CFG_DEBUG
      mySerial.println("HTTP arg(" + String(i) + "): " + webServer.argName(i) + " = " + webServer.arg(i));  /* Display each argument */
      #endif /* CFG_DEBUG */
      if (webServer.argName(i) == "cmd") {  /* check for cmd */
        String key = "";
        String value = "";
        if (splitHtmlCommand(webServer.arg(i), &key, &value)) {
          mySerial.println(key + " : " + value);
          if (key == "discover") {
            if ((value.toInt() & 1) == true) {
              myMqttHelper.setTriggerDiscovery(true);
            } else if ((value.toInt() & 1) == false) {
              myMqttHelper.setTriggerRemoveDiscovered(true);
            }
          } else if (key == "discovery_enabled") {
            if ((value.toInt() & 1) == true) {
              myConfig.discovery_enabled = true;
            } else if ((value.toInt() & 1) == false) {
              myConfig.discovery_enabled = false;
            }
            requestSaveToSpiffs = true;
          } else if (key == "name") {
            if ( (value != "") && (value != myConfig.name) ) {
              strlcpy(myConfig.name, value.c_str(), sizeof(myConfig.name));
              /* new name will create new entities in HA if discovery is enabled so do a restart then */
              if (myConfig.discovery_enabled == true) {
                requestSaveToSpiffsWithRestart = true;
                #ifdef CFG_DEBUG
                mySerial.println("Request FileSystem write with restart.");
                #endif
              } else {
                requestSaveToSpiffs = true;
                #ifdef CFG_DEBUG
                mySerial.println("Request FileSystem write.");
                #endif
              }
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "update_server_address") {
            if ( (value != "") && (value != myConfig.update_server_address) ) {
              #ifdef CFG_DEBUG
              mySerial.println("Request FileSystem write.");
              #endif
              strlcpy(myConfig.update_server_address, value.c_str(), sizeof(myConfig.update_server_address));
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "calibration_factor") {
            uint8_t u8_value = (uint8_t) value.toInt();
            if ((u8_value != myConfig.calibration_factor) && (u8_value >= 50) && (u8_value <= 200)) {
              #ifdef CFG_DEBUG
              mySerial.println("Request FileSystem write.");
              #endif
              myConfig.calibration_factor = int16_t(u8_value);
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "calibration_offset") {
            int16_t i16_value = (int16_t) value.toInt();
            if ((i16_value != myConfig.calibration_offset) && (i16_value >= -50) && (i16_value <= 50)) {
              #ifdef CFG_DEBUG
              mySerial.println("Request FileSystem write.");
              #endif
              myConfig.calibration_offset = int16_t(i16_value);
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "display_brightness") {
            uint8_t u8_value = (uint8_t) value.toInt();
            if ((u8_value != myConfig.display_brightness) && (u8_value <= 255)) {
              #ifdef CFG_DEBUG
              mySerial.println("Request FileSystem write.");
              #endif
              myConfig.display_brightness = u8_value;
              requestSaveToSpiffsWithRestart = true;
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "fetch_update") {
            if ((value.toInt() & 1) == true) {
              fetch_update = true;
              #ifdef CFG_DEBUG
              mySerial.println("HTTP Update triggered via webpage");
              #endif
            }
          } else if (key == "display_enabled") {
            if ((value.toInt() & 1) == true) {
              myConfig.display_enabled = true;
              #ifdef CFG_DEBUG
              mySerial.println("Display enabled via webpage");
              #endif
            } else if ((value.toInt() & 1) == false) {
              myConfig.display_enabled = false;
              #ifdef CFG_DEBUG
              mySerial.println("Display disabled via webpage");
              #endif
            }
            requestSaveToSpiffs = true;
          } else if (key == "mqtt_server_address") {
            if ( (value != "") && (value != myConfig.mqtt_host) ) {
              strlcpy(myConfig.mqtt_host, value.c_str(), sizeof(myConfig.mqtt_host));
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              mySerial.println("New MQTT server configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_server_port") {
            if ( (value.toInt() != myConfig.mqtt_port) && (value.toInt() > 0) && (value.toInt() <= UINT16_MAX) ) {
              myConfig.mqtt_port = value.toInt();
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              mySerial.println("New MQTT port configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_user") {
            if ( (value != "") && (value != myConfig.mqtt_user) ) {
              strlcpy(myConfig.mqtt_user, value.c_str(), sizeof(myConfig.mqtt_user));
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              mySerial.println("New MQTT user configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_pwd") {
            if ( (value != "") && (value != myConfig.mqtt_password) ) {
              strlcpy(myConfig.mqtt_password, value.c_str(), sizeof(myConfig.mqtt_password));
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              mySerial.println("New MQTT password configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_pub_cycle") {
            if ( (value.toInt() != myConfig.mqtt_publish_cycle) && (value.toInt() > 0) && (value.toInt() <= UINT8_MAX) ) {
              myConfig.mqtt_publish_cycle = value.toInt();
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              mySerial.println("New MQTT pyblish cycle configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              mySerial.println("Configuration unchanged, do nothing");
              #endif
            }
          }
        }
      }
      if (webServer.argName(i) == "InputMethod") {  /* check for dedicated arguments */
        if (webServer.arg(i) != String(myConfig.input_method) && (webServer.arg(i).toInt() >= 0) && (webServer.arg(i).toInt() < 2)) { /* check range and if changed at all */
          #ifdef CFG_DEBUG
          mySerial.println("Request FileSystem write with restart.");
          #endif
          myConfig.input_method = static_cast<bool>(webServer.arg(i).toInt());
          requestSaveToSpiffsWithRestart = true;
        } else {
          #ifdef CFG_DEBUG
          mySerial.println("Configuration unchanged, do nothing");
          #endif
        }
      }
      if (webServer.argName(i) == "Sensor") {  /* check for dedicated arguments */
        if (webServer.arg(i) != String(myConfig.sensor_type) && (webServer.arg(i).toInt() >= 0) && (webServer.arg(i).toInt() < 2)) { /* check range and if changed at all */
          #ifdef CFG_DEBUG
          mySerial.println("Request FileSystem write with restart.");
          #endif
          myConfig.sensor_type = webServer.arg(i).toInt();
          requestSaveToSpiffsWithRestart = true;
        } else {
          #ifdef CFG_DEBUG
          mySerial.println("Configuration unchanged, do nothing");
          #endif
        }
      }
    }
  }
}

void handleHttpReset(void) {
  requestSaveToSpiffsWithRestart = true;
  handleWebServerClient();
}

/* MQTT callback if a message was received */
// cppcheck-suppress constParameter ; messageReceived is a library function
void messageReceived(String &topic, String &payload) {  // NOLINT
  #ifdef CFG_DEBUG
  mySerial.println("received: " + topic + " : " + payload);
  #endif

  if (topic == myMqttHelper.getTopicSystemRestartRequest()) {
    #ifdef CFG_DEBUG
    mySerial.println("Restart request received with value: " + payload);
    #endif
    if ((payload == "true") || (payload == "1")) {
      systemRestartRequest = true;
    }
  } else if (topic == myMqttHelper.getTopicUpdateFirmware()) { /* HTTP Update */
    if ((payload == "true") || (payload == "1")) {
      #ifdef CFG_DEBUG
      mySerial.println("Firmware updated triggered via MQTT");
      #endif
      fetch_update = true;
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
    mySerial.println("New name received: " + payload);
    #endif
    if (payload != myConfig.name) {
      #ifdef CFG_DEBUG
      mySerial.println("Old name was: " + String(myConfig.name));
      #endif
      strlcpy(myConfig.name, payload.c_str(), sizeof(myConfig.name));
      requestSaveToSpiffsWithRestart = true;
    }
  } else if (topic == myMqttHelper.getTopicChangeSensorCalib()) {
    #ifdef CFG_DEBUG
    mySerial.println("New sensor calibration parameters received: " + payload);
    #endif

    int16_t offset;
    int16_t factor;

    if (splitSensorDataString(payload, &offset, &factor)) {
      myThermostat.setSensorCalibData(factor, offset, true);
    }
  } else if (topic == myMqttHelper.getTopicChangeHysteresis()) {
    #ifdef CFG_DEBUG
    mySerial.println("New hysteresis parameter received: " + payload);
    #endif

    myThermostat.setThermostatHysteresis(payload.toInt());
  } else if (topic == myMqttHelper.getTopicOutsideTemperature()) {
    #ifdef CFG_DEBUG
    mySerial.println("New outside temperature received: " + payload);
    #endif
    myThermostat.setOutsideTemperature(floatToInt(payload.toFloat()));
  }
}
