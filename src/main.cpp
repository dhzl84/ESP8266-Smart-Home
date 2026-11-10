/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include <ArduinoOTA.h>
#include "main.h"
#include <DHTesp.h>
#include <Wire.h> /* for BME280 */
#include <SPI.h>  /* for BME280 */
#include <BME280I2C.h>
#include "user_fonts.h"
#include <SSD1306.h>
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
uint32_t mqttReconnectInterval   = seconds_to_milliseconds(5); /* 5 s in milliseconds */
uint32_t mqttPubCycleTime        = 0;
/* loop handler */
#define loop_time_50_ms        50
#define loop_time_100_ms      100
#define loop_time_500_ms      500
#define loop_time_1000_ms     seconds_to_milliseconds(1)
#define loop_time_30_seconds  seconds_to_milliseconds(30)
#define loop_time_1_minute    minutes_to_milliseconds(1)
#define loop_time_5_minutes   minutes_to_milliseconds(5)
#define loop_time_30_minutes  minutes_to_milliseconds(30)
uint32_t loop_50_ms_in_millis      =  0;
uint32_t loop_100_ms_in_millis     = 13; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop_500_ms_in_millis     = 17; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop_1000_ms_in_millis    = 19; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop_30_seconds_in_millis = 23; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop_1_minute_in_millis   = 29; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop_5_minutes_in_millis  = 31; /* start loops with some offset to avoid calling all loops every second */
uint32_t loop_30_minutes_in_millis = 37; /* start loops with some offset to avoid calling all loops every second */
/* HTTP Update */
bool update_firmware = false;       /* global variable used to decide whether an update shall be fetched from server or not */
/* OTA */
typedef enum otaUpdate {
  TH_OTA_IDLE,
  TH_OTA_ACTIVE,
  TH_OTA_FINISHED,
  TH_OTA_ERROR
} OtaUpdate_t; /* global variable used to change display in case OTA update is initiated */
OtaUpdate_t OTA_UPDATE = TH_OTA_IDLE;
String ota_stub_address = "";
/* for critical section lock during interrupt */

#define rotLeft -1
#define rotRight 1
#define rotInit  0
volatile int16_t lastEncoded = 0b11;                   /* initial state of the rotary encoders gray code */
volatile int16_t rotaryEncoderDirectionInts = rotInit; /* initialize rotary encoder with no direction */
volatile bool encoderEventPending = false; /* encoder detent flag set by ISR; direction evaluated in loop() */
const uint32_t buttonDebounceInterval = 25;
uint32_t onOffButtonSystemResetTime = 0;
uint32_t onOffButtonSystemResetInterval = seconds_to_milliseconds(5);

/* thermostat */
#define tempStep              5
#define displayTemp           0
#define displayHumid          1
/* sensor */
uint32_t readSensorScheduled = 0;
/* Display */
#define drawTempYOffset       16

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
#define  display_enabled_temporary_interval seconds_to_milliseconds(10)
bool     display_enabled_temporary = false;
bool     systemRestartRequest = false;
bool     silentRestart = false;
bool     restartedToday = true;
bool     requestSaveToSpiffs = false;
bool     requestSaveToSpiffsWithRestart = false;
uint32_t wifiReconnectTimer = seconds_to_milliseconds(WIFI_RECONNECT_TIME);
uint32_t systemRestartTimer = minutes_to_milliseconds(SYSTEM_RESTART_TIME);
#ifndef WIFI_RECONNECT_TIME
  #define WIFI_RECONNECT_TIME 30
#endif

#define  FS_WRITE_DEBOUNCE      minutes_to_milliseconds(30) /* write FileSystem 30 minutes after last change */
bool     FS_WRITTEN =           true;
uint32_t FS_REFERENCE_TIME      = 0;

/* Time */
tm time_info;
char time_buffer[6] = "00:00";  /* hold the current time in format "%H:%M" */
#define NTP_SERVER_1 "0.de.pool.ntp.org"
#define NTP_SERVER_2 "1.de.pool.ntp.org"
#define NTP_SERVER_3 "pool.ntp.org"

/*===================================================================================================================*/
/* The setup function is called once at startup of the sketch */
/*===================================================================================================================*/
void setup() {
  #ifdef CFG_DEBUG
  Serial.begin(115200);
  Serial.println("SETUP STARTED");
  #endif
  Wire.begin(SDA_PIN, SCL_PIN);   /* needed for I²C communication with display and BME280 */
  FS_INIT();                  /* read stuff from FileSystem */
  myConfig.binary_version_address.concat(myConfig.update_server_address);
  myConfig.binary_version_address.concat("/version");
  myConfig.binary_address.concat(myConfig.update_server_address);
  myConfig.binary_address.concat("/firmware.bin");
  #ifdef CFG_DEBUG
  Serial.print("Update binary address: ");
  Serial.println(myConfig.binary_address);
  Serial.print("Update version file address: ");
  Serial.println(myConfig.binary_version_address);
  #endif /* CFG_DEBUG */

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
  NTP();           /* acquire time */
  OTA_INIT();      /* init over the air update */
  MQTT_CONNECT();  /* connect to MQTT host and build subscriptions, must be called after FS_INIT()*/
  #ifdef CFG_DEBUG
  Serial.printf("MQTT Server IP: %s\n", myConfig.mqtt_host);
  #endif /* CFG_DEBUG */
  MDNS.begin(myConfig.name);
  configTzTime(myConfig.timezone, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);  /* init network time protocol, must be called after WIFI_CONNECT() and FS_INIT() */
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
  Serial.printf("Vcc: %f\n", (ESP.getVcc() / 1000.0f));
  Serial.printf("Reset Reason: %s\n", ESP.getResetReason().c_str());
  Serial.printf("Flash Size: %d\n", ESP.getFlashChipRealSize());
  #elif CFG_BOARD_ESP32
  Serial.println("CPU Frequency: " + String(ESP.getCpuFreqMHz()));
  Serial.println("CPU 0 reset reason: " + getEspResetReason(rtc_get_reset_reason(0)));
  Serial.println("CPU 1 reset reason: " + getEspResetReason(rtc_get_reset_reason(1)));
  #else
  #endif
  Serial.println("Sketch Size: " + String(ESP.getSketchSize()));
  Serial.println("Free for Sketch: " + String(ESP.getFreeSketchSpace()));
  Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
  Serial.println("SETUP COMPLETE - ENTER LOOP");
  #endif

  SENSOR_MAIN();  /* acquire first sensor data before staring loop() to avoid false value reporting due to current temperature, etc. being the init value until first sensor value is read */
}

/*===================================================================================================================*/
/* functions called by setup()                                                                                       */
/*===================================================================================================================*/

void FS_INIT(void) {  // initializes the FileSystem when first used and loads the configuration from FileSystem to RAM
  FileSystem.begin();

  if (!FileSystem.exists(F("/formatted"))) {
    /* This code is only run once to format the FileSystem before first usage */
    #ifdef CFG_DEBUG
    Serial.println("Formatting FileSystem, this takes some time");
    #endif
    FileSystem.format();
    #ifdef CFG_DEBUG
    Serial.println("Formatting FileSystem finished");
    Serial.println("Open file '/formatted' in write mode");
    #endif
    File f = FileSystem.open("/formatted", "w");
    if (!f) {
      #ifdef CFG_DEBUG
      Serial.println("file open failed");
      #endif
    } else {
      f.close();
      delay(5000);
      if (!FileSystem.exists(F("/formatted"))) {
        #ifdef CFG_DEBUG
        Serial.println("That didn't work!");
        #endif
      } else {
        #ifdef CFG_DEBUG
        Serial.println("Cool, working!");
        #endif
      }
    }
  } else {
    #ifdef CFG_DEBUG
    Serial.println("Found '/formatted' >> FileSystem ready to use");
    #endif
  }
  #ifdef CFG_DEBUG
  Serial.println("Check if I remember who I am ... ");
  #endif

  #ifdef CFG_DEBUG
  #if CFG_BOARD_ESP8266
  Dir dir = FileSystem.openDir("/");
  while (dir.next()) {
    Serial.print("FileSystem file found: " + dir.fileName() + " - Size in byte: ");
    File f = dir.openFile("r");
    Serial.println(f.size());
  }
  #elif CFG_BOARD_ESP32
  listDir(FileSystem, "/", 0);
  #else
  #endif
  #endif  /* CFG_DEBUG */

  loadConfiguration(myConfig);  // load config

  #ifdef CFG_DEBUG
  Serial.println("My name is: " + String(myConfig.name));
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
  Serial.println("Initialize display");
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
  // keep display on for some time after initialization
  display_enabled_temporary = true;
  SetNextTimeInterval(&display_enabled_temporary_reference_time, seconds_to_milliseconds(20));
}

void WIFI_CONNECT(void) {
  wifi_connect_counter++;
  if (WiFi.status() != WL_CONNECTED) {
    #ifdef CFG_DEBUG
    Serial.println("Initialize WiFi ");
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
    /* non-blocking: connection status is checked every WIFI_RECONNECT_TIME seconds in
       HANDLE_SYSTEM_STATE(); WiFi autoReconnect (default: true) reconnects in background */
  }

  #ifdef CFG_DEBUG
  Serial.println("WiFi Status: "+ wifiStatusToString(WiFi.status()));
  WiFi.printDiag(Serial);
  Serial.println("Local IP: "+ WiFi.localIP().toString());
  Serial.println("Gateway IP: " + WiFi.gatewayIP().toString());
  Serial.println("DNS IP: " + WiFi.dnsIP().toString());
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
    Serial.println("Start OTA");
    #endif
  });

  ArduinoOTA.onEnd([]() {
    OTA_UPDATE = TH_OTA_FINISHED;
    DISPLAY_MAIN();
    #ifdef CFG_DEBUG
    Serial.println("\nEnd OTA");
    #endif
  });

  ArduinoOTA.onProgress([](uint16_t progress, uint16_t total) {
    #ifdef CFG_DEBUG
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    #endif
  });

  ArduinoOTA.onError([](ota_error_t error) {
    OTA_UPDATE = TH_OTA_ERROR;
    DISPLAY_MAIN();
    #ifdef CFG_DEBUG
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
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
      myMqttClient.setOptions(180, true, seconds_to_milliseconds(30)); /* short connect timeout; exponential backoff in MQTT_MAIN() */
      myMqttClient.begin(myConfig.mqtt_host, myConfig.mqtt_port, myWiFiClient);
      myMqttClient.setWill((myMqttHelper.getTopicLastWill()).c_str(),   "offline", true, MQTT_QOS);     /* broker shall publish 'offline' on ungraceful disconnect >> Last Will */
      myMqttClient.onMessage(messageReceived);                                                        /* register callback */
      (void)myMqttClient.connect(myConfig.name, myConfig.mqtt_user, myConfig.mqtt_password);

      homeAssistantRemoveDiscoveredObsolete();  /* for migration */
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
  if (TimeReached(loop_50_ms_in_millis)) {
    SetNextTimeInterval(&loop_50_ms_in_millis, loop_time_50_ms);
    /* nothing yet */
  }
  /* call every 100 ms */
  if (TimeReached(loop_100_ms_in_millis)) {
    SetNextTimeInterval(&loop_100_ms_in_millis, loop_time_100_ms);
    HANDLE_SYSTEM_STATE();   /* handle connectivity and trigger reconnects */
    myThermostat.loop();     /* control relay for heating */
    DISPLAY_MAIN();          /* draw display each loop */
    MQTT_MAIN();             /* handle MQTT each loop */
  }
  /* call every 500 ms */
  if (TimeReached(loop_500_ms_in_millis)) {
    SetNextTimeInterval(&loop_500_ms_in_millis, loop_time_500_ms);
    /* nothing yet */
  }
  /* call every second */
  if (TimeReached(loop_1000_ms_in_millis)) {
    SetNextTimeInterval(&loop_1000_ms_in_millis, loop_time_1000_ms);
    SENSOR_MAIN();          /* get sensor data */
    FS_MAIN();              /* check for new data and write FileSystem is necessary */
    HANDLE_HTTP_UPDATE();   /* pull update from server if it was requested via MQTT or a new version was detected */
  }
  /* call every 30 s */
  if (TimeReached(loop_30_seconds_in_millis)) {
    SetNextTimeInterval(&loop_30_seconds_in_millis, loop_time_30_seconds);
    NTP();                 /* calculate formatted time */
  }
  /* call every minute */
  if (TimeReached(loop_1_minute_in_millis)) {
    SetNextTimeInterval(&loop_1_minute_in_millis, loop_time_1_minute);
  }
  /* call every 5 minutes */
  if (TimeReached(loop_5_minutes_in_millis)) {
    SetNextTimeInterval(&loop_5_minutes_in_millis, loop_time_5_minutes);
    CHECK_FOR_UPDATE();    /* check for updates on update server */
  }
  /* call every 30 minutes */
  if (TimeReached(loop_30_minutes_in_millis)) {
    SetNextTimeInterval(&loop_30_minutes_in_millis, loop_time_30_minutes);
  }
  /* rotary encoder event handling: ISR sets flag, evaluation here with critical section */
  if (encoderEventPending) {
    noInterrupts();
    int16_t encoderDirection = rotaryEncoderDirectionInts;
    rotaryEncoderDirectionInts = rotInit;
    encoderEventPending = false;
    interrupts();
    SetNextTimeInterval(&display_enabled_temporary_reference_time, display_enabled_temporary_interval);
    if (myConfig.display_enabled || display_enabled_temporary) {
      if (encoderDirection > rotRight) { /* majority of interrupts to the right */
        if (myThermostat.getThermostatMode() == TH_HEAT) {
          myThermostat.increaseTargetTemperature(tempStep);
        }
      } else if (encoderDirection < rotLeft) { /* majority of interrupts to the left */
        if (myThermostat.getThermostatMode() == TH_HEAT) {
          myThermostat.decreaseTargetTemperature(tempStep);
        }
      } else {
        /* left/right counts equal -> should never happen, do nothing */
      }
    }
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
      if (myThermostat.getThermostatMode() == TH_HEAT) {
        myThermostat.increaseTargetTemperature(tempStep);
      }
    }
    SetNextTimeInterval(&display_enabled_temporary_reference_time, display_enabled_temporary_interval);
  }
  if (debounceDown.fell()) {
    if (myConfig.display_enabled || display_enabled_temporary) {
      if (myThermostat.getThermostatMode() == TH_HEAT) {
        myThermostat.decreaseTargetTemperature(tempStep);
      }
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
  /* check WiFi connection every 30 seconds */
  if (TimeReached(wifiReconnectTimer)) {
    SetNextTimeInterval(&wifiReconnectTimer, (seconds_to_milliseconds(WIFI_RECONNECT_TIME)));
    if (WiFi.status() != WL_CONNECTED) {
      #ifdef CFG_DEBUG
      Serial.println("Lost WiFi; Status: "+ String(WiFi.status()));
      #endif
      /* try to come online, debounced to avoid reconnect each loop*/
      WIFI_CONNECT();
    } else {
      /* WiFi is connected, reset systemRestartTimer */
      SetNextTimeInterval(&systemRestartTimer, (minutes_to_milliseconds(SYSTEM_RESTART_TIME)));
    }
  }
  if (TimeReached(systemRestartTimer)) {
  #ifdef CFG_DEBUG
  Serial.println("Restart requested by System State Handler");
  #endif /* CFG_DEBUG */
    systemRestartRequest = true;
    silentRestart = true;
  }
  /* Check if the sytem shall restart at a defined time */
  #ifdef CFG_DEBUG_PLANNED_RESTART
  Serial.println("== PLANNED RESTART ==");
  Serial.print("Planned Restart enabled: ");
  Serial.println(myConfig.planned_restart);
  Serial.print("Restarted Today: ");
  Serial.println(restartedToday);
  Serial.print("Time: ");
  Serial.println(time_buffer);
  Serial.print("Time Compare: ");
  Serial.println(strcmp(time_buffer, "04:00"));
  #endif /* CFG_DEBUG_PLANNED_RESTART */

  if (myConfig.planned_restart == true) {
    /* at midnight, reset restartedToday */
    if ((strcmp(time_buffer, "00:00") == 0) && (restartedToday == true)) {
      restartedToday = false;
    }
    /* at 04:00, request a silent restart */
    if ((strcmp(time_buffer, "04:00") == 0) && (restartedToday == false)) {
      #ifdef CFG_DEBUG
      Serial.println("Restart requested by planned time");
      #endif /* CFG_DEBUG */
      systemRestartRequest = true;
      silentRestart = true;
      restartedToday = true;
    }
  }
  /* check if button is pushed for 5 seconds to request a reset (overflow-safe timer) */
  if (LOW == digitalRead(PHYS_INPUT_3_PIN)) {
    #ifdef CFG_DEBUG_ENCODER
    Serial.println("Encoder Button pressed: "+ String(digitalRead(PHYS_INPUT_3_PIN)));
    Serial.println("Current time: "+ String(millis()));
    #endif
    if (TimeReached(onOffButtonSystemResetTime)) {
      Serial.println("Restart requested by push button");
      systemRestartRequest = true;
    } else {
      /* waiting */
    }
  } else {
    SetNextTimeInterval(&onOffButtonSystemResetTime, onOffButtonSystemResetInterval);
  }

  /* restart handling */
  if (systemRestartRequest == true) {
    if (silentRestart == true) {
      silentRestart = false;
    } else
    {
      DISPLAY_MAIN();
    }
    myMqttClient.publish(myMqttHelper.getTopicSystemRestartRequest(), "false", false, MQTT_QOS);   /* publish restart = false on connect */
    myMqttClient.loop();
    systemRestartRequest = false;
    #ifdef CFG_DEBUG
    Serial.println("Restarting in 3 seconds due to Restart Request");
    #endif
    myMqttClient.disconnect();
    delay(seconds_to_milliseconds(3));
    ESP.restart();
  }
}

void NTP(void) {
  time_t now;                              // this is the epoch
  time(&now);                              // read the current time
  localtime_r(&now, &time_info);           // update the structure time_info with the current time
  const char* time_format = "%H:%M";
  strftime(time_buffer, sizeof(time_buffer), time_format, &time_info);
  #ifdef CFG_DEBUG_SNTP
  Serial.printf("NTP - year:%d", time_info.tm_year + 1900);   // years since 1900
  Serial.printf("  month:%d", time_info.tm_mon + 1);      // January = 0 (!)
  Serial.printf("  day:%d", time_info.tm_mday);         // day of month
  Serial.printf("  hour:%d", time_info.tm_hour);         // hours since midnight  0-23
  Serial.printf("  min:%d", time_info.tm_min);          // minutes after the hour  0-59
  Serial.printf("  sec:%d", time_info.tm_sec);          // seconds after the minute  0-61*
  Serial.printf("  wday:%d", time_info.tm_wday);         // days since Sunday 0-6
  Serial.printf("  DST:%d\n", time_info.tm_isdst);  // Daylight Saving Time flag
  #endif  /* CFG_DEBUG_SNTP */
}

void SENSOR_INIT() {
  if (myConfig.sensor_type == cBME280) {
    if (!myBME280.begin()) { /* init BMP sensor */
      #ifdef CFG_DEBUG
      Serial.println("Could not find a valid BME sensor!");
      #endif
    } else {
      myBME280.setSettings(settings);
      #ifdef CFG_DEBUG
      // ChipModel_UNKNOWN = 0
      // ChipModel_BMP280 = 0x58
      // ChipModel_BME280 = 0x60
      switch (myBME280.chipModel()) {
      case 0x60:
        Serial.println("Sensor Chip Model: BME280");
        break;
      case 0x58:
        Serial.println("Sensor Chip Model: BMP280");
        break;
      default:
        Serial.println("Sensor Chip Model: unknown");
      }
      #endif
    }
  } else if (myConfig.sensor_type == cDHT22) {
      myDHT22.setup(DHT_PIN, DHTesp::DHT22); /* init DHT sensor */
      Serial.println("DHT22 init: " + String(myDHT22.getStatusString()));
  } else {
      #ifdef CFG_DEBUG
      Serial.println("Sensor misconfiguration!");
      #endif
  }
}

void SENSOR_MAIN() {
  /* schedule routine for sensor read */
  if (TimeReached(readSensorScheduled)) {
    SetNextTimeInterval(&readSensorScheduled, (seconds_to_milliseconds(myConfig.sensor_update_interval)));

    float sensTemp(NAN), sensHumid(NAN);

    if (myConfig.sensor_type == cDHT22) {
      TempAndHumidity dhtResult = myDHT22.getTempAndHumidity();  /* triggers readSensor(); fields are NAN on read failure */
      sensTemp  = dhtResult.temperature;
      sensHumid = dhtResult.humidity;
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
      Serial.println("Failed to read from sensor! Failure counter: " + String(myThermostat.getSensorFailureCounter()));
      #endif
    } else {
      myThermostat.setLastSensorReadFailed(false);   /* set no failure during read sensor */
      myThermostat.setCurrentHumidity((int16_t)(10* sensHumid));     /* read value and convert to one decimal precision integer */
      myThermostat.setCurrentTemperature((int16_t)(10* sensTemp));   /* read value and convert to one decimal precision integer */

      #ifdef CFG_DEBUG_SENSOR_VALUES
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
      #endif  /* CFG_DEBUG_SENSOR_VALUES */

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

uint32_t display_content_hash_last = 0;
bool display_flushed = false;

/* FNV-1a hash of all display-relevant content; used to skip redundant OLED flushes */
uint32_t displayContentHash(void) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d%d%d%d%d%s",
           static_cast<int>(systemRestartRequest),
           static_cast<int>(OTA_UPDATE),
           static_cast<int>(update_firmware),
           static_cast<int>(myConfig.display_enabled || display_enabled_temporary),
           static_cast<int>(myThermostat.getSensorError()),
           static_cast<int>(myThermostat.getSensorFailureCounter()),
           static_cast<int>(myThermostat.getFilteredTemperature()),
           static_cast<int>(myThermostat.getOutsideTemperatureReceived()),
           static_cast<int>(myThermostat.getOutsideTemperature()),
           static_cast<int>(myThermostat.getThermostatMode()),
           static_cast<int>(myThermostat.getTargetTemperature()),
           time_buffer);
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < strlen(buf); i++) {
    hash ^= static_cast<uint32_t>(buf[i]);
    hash *= 16777619u;
  }
  return hash;
}

void DISPLAY_MAIN(void) {
  const uint32_t contentHash = displayContentHash();
  if (display_flushed && (contentHash == display_content_hash_last)) {
    return;  /* display content unchanged: skip I2C flush */
  }
  display_content_hash_last = contentHash;
  display_flushed = true;
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
    } else if (update_firmware == true) { /* HTTP Update */
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      myDisplay.drawString(64, 22, "Update");
    } else {
      /* display current time */
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
      myDisplay.drawString(0, 48, String(time_buffer));

      #ifdef CFG_DEBUG_DISPLAY_VERSION
      // myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
      // myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.drawString(0, 16, String(VERSION));
      myDisplay.drawString(0, 32, String(BUILD_NUMBER));
      myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
      myDisplay.drawString(128, 48, String("IPv4: " + (WiFi.localIP().toString()).substring(((WiFi.localIP().toString()).lastIndexOf(".") + 1), (WiFi.localIP().toString()).length())));
      #endif

      myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
      myDisplay.setFont(Roboto_Condensed_32);

      if (myThermostat.getSensorError()) {
        myDisplay.drawString(128, drawTempYOffset, "err");
      } else if (myThermostat.getSensorFailureCounter() == -1) {
        myDisplay.drawString(128, drawTempYOffset, "init");
      } else {
        myDisplay.drawString(128, drawTempYOffset, String(intToFloat(myThermostat.getFilteredTemperature()), 1));
      }

      /* display outside temperature in top left corner */
      if (myThermostat.getOutsideTemperatureReceived() == true) {
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
          myDisplay.drawXbm(80, 0, myThermo_width, myThermo_height, myThermo);
        }
      }
    }
  }
  myDisplay.display();
}

void MQTT_MAIN(void) {
  if (myMqttClient.connected() == false) {
    if (TimeReached(mqttReconnectTime)) {  /* try reconnect to MQTT broker after mqttReconnectTime expired */
        MQTT_CONNECT();
        if (myMqttClient.connected()) {
          mqttReconnectInterval = seconds_to_milliseconds(5);  /* success: return to fast retry */
        } else {
          mqttReconnectInterval = min(mqttReconnectInterval * 2u, minutes_to_milliseconds(10));  /* exponential backoff, capped at 10 minutes */
        }
        SetNextTimeInterval(&mqttReconnectTime, mqttReconnectInterval); /* reset interval */
    } else {
      /* just wait */
    }
  } else {  /* check if there is new data to publish and shift PubCycle if data is published on event, else publish every PubCycleInterval */
    if (myMqttHelper.getTriggerRemoveDiscovered()) {
      homeAssistantRemoveDiscovered();  /* make HA remove entities */
      myMqttHelper.setTriggerRemoveDiscovered(false);
    }
    if (myMqttHelper.getTriggerDiscovery()) {
      homeAssistantDiscovery();  /* make HA discover/update necessary devices at runtime e.g. after name change */
      myMqttHelper.setTriggerDiscovery(false);
    }
    if ( TimeReached(mqttPubCycleTime) || myThermostat.getNewData() ) {
        SetNextTimeInterval(&mqttPubCycleTime, minutes_to_milliseconds(myConfig.mqtt_publish_cycle));
        mqttPubState();
        myThermostat.resetNewData();
    }
  }
}

void CHECK_FOR_UPDATE(void) {
  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin(wifiClient, myConfig.binary_version_address);
  int httpCode = httpClient.GET();
  if (httpCode == 200) {
    myConfig.available_firmware_version = httpClient.getString();
    #ifdef CFG_DEBUG
    Serial.println("Check For Update");
    Serial.print("  Current firmware version: ");
    Serial.println(VERSION);
    Serial.print("  Available firmware version: ");
    Serial.println(myConfig.available_firmware_version);
    #endif /* CFG_DEBUG */
  } else {
    myConfig.available_firmware_version = "none";
    #ifdef CFG_DEBUG
    Serial.println("Check For Update");
    Serial.printf("  URL: %s\n", myConfig.binary_version_address.c_str());
    Serial.printf("  HTTP Code: %d - %s\n", httpCode, httpClient.errorToString(httpCode).c_str());
    #endif  // CFG_DEBUG
  }

  if ( (myConfig.available_firmware_version != "none") \
    && (myConfig.available_firmware_version != "") \
    && (myConfig.available_firmware_version != VERSION) ) {
      if (myConfig.auto_update == true) {
        update_firmware = true;
      }
  }
}

void HANDLE_HTTP_UPDATE(void) {
  if (update_firmware == true) {
    DISPLAY_MAIN();
    CHECK_FOR_UPDATE();
    update_firmware = false;
    WiFiClient client;
    t_httpUpdate_return ret = myHttpUpdate.update(client, myConfig.binary_address, FW);

    #ifdef CFG_DEBUG
    Serial.println("Remote update started");
    Serial.println("Firmware Address: " + myConfig.binary_address);
    #endif

    switch (ret) {
    case HTTP_UPDATE_FAILED:
      #ifdef CFG_DEBUG
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s \n", myHttpUpdate.getLastError(), myHttpUpdate.getLastErrorString().c_str());
      #endif
      break;

    case HTTP_UPDATE_NO_UPDATES:
      #ifdef CFG_DEBUG
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      #endif
      break;

    case HTTP_UPDATE_OK:
      #ifdef CFG_DEBUG
      Serial.println("HTTP_UPDATE_OK");
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
      Serial.println("Restart requested by file system handler");
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
    SetNextTimeInterval(&FS_REFERENCE_TIME, FS_WRITE_DEBOUNCE);  /* overflow-safe timer at last change */
    myConfig.target_temperature  = myThermostat.getTargetTemperature();
    myConfig.calibration_factor = myThermostat.getSensorCalibFactor();
    myConfig.calibration_offset = myThermostat.getSensorCalibOffset();
    myConfig.temperature_hysteresis  = myThermostat.getThermostatHysteresis();
    myConfig.thermostat_mode   = myThermostat.getThermostatMode();
    myThermostat.resetNewCalib();
    FS_WRITTEN = false;
    #ifdef CFG_DEBUG
    char cstr[16];
    Serial.printf("FileSystem to be stored after debounce time: %s\n", itoa(FS_WRITE_DEBOUNCE, cstr, 10));
    #endif /* CFG_DEBUG */
  } else {
    /* no spiffs data changed this loop */
  }
  if (FS_WRITTEN == true) {
    /* do nothing, last change was already stored in FileSystem */
  } else { /* latest change not stored in FileSystem */
    if (TimeReached(FS_REFERENCE_TIME)) { /* check debounce, overflow-safe */
      /* debounce expired -> write */
      if (saveConfiguration(myConfig)) {
        FS_WRITTEN = true;
      } else {
        /* FileSystem not written, retry next loop */
        #ifdef CFG_DEBUG
        Serial.println("FileSystem write failed");
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
void IRAM_ATTR updateEncoder(void) {
  /* Minimal ISR: touch volatile state only. No Serial, no non-IRAM function calls.
     Int-to-flash access while the flash is busy (LittleFS write, OTA, WiFi storage)
     caused exception 28/29 reboots. Direction evaluation happens in loop(). */
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
  }

  if (encoded == 0b11) { /* encoder in end position: loop() evaluates the counted direction */
    encoderEventPending = true;
  }

  lastEncoded = encoded; /* store this value for next time */
}

/* Home Assistant discovery_enabled on connect; used to define entities in HA to communicate with*/
void homeAssistantDiscovery(void) {
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate(),                         myMqttHelper.buildHassDiscoveryClimate(String(myConfig.name), String(FW), String(BOARD)),    true, MQTT_QOS);    // make HA discover the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kTemp),                     myMqttHelper.buildHassDiscoverySensor(String(myConfig.name),  kTemp),                        true, MQTT_QOS);    // make HA discover the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kHum),                      myMqttHelper.buildHassDiscoverySensor(String(myConfig.name),  kHum),                         true, MQTT_QOS);    // make HA discover the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryButton(kRestart_button),           myMqttHelper.buildHassDiscoveryButton(String(myConfig.name),  kRestart_button),              true, MQTT_QOS);    // make HA discover the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryButton(kUpdate_button),            myMqttHelper.buildHassDiscoveryButton(String(myConfig.name),  kUpdate_button),               true, MQTT_QOS);    // make HA discover the update switch
}

/* Make Home Assistant forget the discovered entities on demand */
void homeAssistantRemoveDiscovered(void) {
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate(),                         String(""),         true, MQTT_QOS);    // make HA forget the climate component
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kTemp),                     String(""),         true, MQTT_QOS);    // make HA forget the temperature sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(kHum),                      String(""),         true, MQTT_QOS);    // make HA forget the humidity sensor
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryButton(kRestart_button),           String(""),         true, MQTT_QOS);    // make HA forget the reset switch
  myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryButton(kUpdate_button),            String(""),         true, MQTT_QOS);    // make HA forget the update switch
  homeAssistantRemoveDiscoveredObsolete();
}

/* DEPRECATED */
void homeAssistantRemoveDiscoveredObsolete(void) {
}

/* publish state topic in JSON format */
void mqttPubState(void) {
  char payload[384];
  char tempBuf[16];
  char humBuf[16];
  char hystBuf[16];
  char tarTempBuf[16];
  char calOffsetBuf[16];
  char ipBuf[16];

  /* send "None" in case of sensor error and if filtered values are still on init values */
  const char* temperature = "None";
  const char* humidity    = "None";
  const char* hysteresis  = "None";
  const char* tarTemp     = "None";
  const char* calOffset   = "None";

  if (myThermostat.getSensorError() == false) {
    if (myThermostat.getFilteredTemperature() != THERMOSTAT_SENSOR_INIT_VALUE) {
      formatScaled(tempBuf, sizeof(tempBuf), myThermostat.getFilteredTemperature(), 1);
      temperature = tempBuf;
    }
    if (myThermostat.getFilteredHumidity() != THERMOSTAT_SENSOR_INIT_VALUE) {
      formatScaled(humBuf, sizeof(humBuf), myThermostat.getFilteredHumidity(), 1);
      humidity = humBuf;
    }
  }

  formatScaled(hystBuf, sizeof(hystBuf), myThermostat.getThermostatHysteresis(), 1);
  hysteresis = hystBuf;

  formatScaled(tarTempBuf, sizeof(tarTempBuf), myThermostat.getTargetTemperature(), 1);
  tarTemp = tarTempBuf;

  formatScaled(calOffsetBuf, sizeof(calOffsetBuf), myThermostat.getSensorCalibOffset(), 1);
  calOffset = calOffsetBuf;

  const IPAddress ip = WiFi.localIP();
  (void)snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  const int payloadLen = myMqttHelper.buildStateJSON(payload, sizeof(payload), \
      myConfig.name, \
      temperature, \
      humidity, \
      hysteresis, \
      cstrHeatingOff(myThermostat.getActualState()), \
      tarTemp, \
      cstrSensStatus(myThermostat.getSensorError()), \
      cstrHeatOff(myThermostat.getThermostatMode()), \
      myThermostat.getSensorCalibFactor(), \
      calOffset, \
      ipBuf, \
      FW);

  if (payloadLen > 0) {
    myMqttClient.publish( \
      myMqttHelper.getTopicData().c_str(), \
      payload, \
      payloadLen, \
      true, /* retain */ \
      MQTT_QOS); /* QOS */
  }
}

/* ---- HTML page assembled in chunks ----
   The page is streamed with the core's chunked transfer encoding: every row is formatted
   into a small stack buffer with snprintf and written straight to the client, so there is
   no page String in the heap (no 'Reallocating large String' reallocs, no fragmentation of
   the DRAM heap), no operator+ / concat / changeBuffer frames on the cont stack that runs
   loop() ('Stack smashing detected' reported by cont_check), and each row checks its own
   length so a row that does not fit is reported by key instead of silently truncating. */

static uint16_t htmlPageBytes;  /* bytes of the page streamed in the current response */

#define HTML_ROW_BUF_SIZE 400  /* worst row: update_server_address with a 255 char configured address */

static void htmlRowTruncated(const char* key) {
  #ifdef CFG_DEBUG
  Serial.print("HTML row truncated: "); Serial.println(key);
  #endif /* CFG_DEBUG */
}

/* Send one chunk. The buffer may be a flash literal or a RAM stack buffer; the core picks
   the matching copy for each (StreamConstPtr tests the address at runtime). */
static void htmlSendChunk(const char* buf, size_t len) {
  webServer.sendContent(buf, len);
  htmlPageBytes += static_cast<uint16_t>(len);
}

static void htmlSendText(const char* text) {
  htmlSendChunk(text, strlen(text));
}

static void htmlRowAppend(int rowLen, const char* key, const char* row, size_t rowSize) {
  if (rowLen < 0) { return; }
  if (static_cast<size_t>(rowLen) >= rowSize) {
    htmlRowTruncated(key);
    rowLen = static_cast<int>(rowSize) - 1;  /* send what fits, the row is complete HTML */
  }
  htmlSendChunk(row, static_cast<size_t>(rowLen));
}

static void htmlRow2(const char* key, const char* value) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s:</td><td>%s</td></tr>", key, value), key, row, sizeof(row));
}

static void htmlRow2Str(const char* key, const String& value) {  /* core APIs that return String */
  htmlRow2(key, value.c_str());
}

static void htmlRow2U32(const char* key, uint32_t value, const char* suffix = "") {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s:</td><td>%lu%s</td></tr>", key, static_cast<unsigned long>(value), suffix), key, row, sizeof(row));
}

static void htmlRow2I32(const char* key, int32_t value, const char* suffix = "") {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s:</td><td>%ld%s</td></tr>", key, static_cast<long>(value), suffix), key, row, sizeof(row));
}

static void htmlRow4(const char* key, const char* value, const char* current_value, const char* description) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>", key, value, current_value, description), key, row, sizeof(row));
}

static void htmlRow4Int(const char* key, const char* value, int16_t current_value, const char* description) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>", key, value, current_value, description), key, row, sizeof(row));
}

void htmlStreamPage(void) {
  float rssiInPercent = WiFi.RSSI();
  rssiInPercent = isnan(rssiInPercent) ? -100.0 : min(max(2 * (rssiInPercent + 100.0), 0.0), 100.0);
  char numBuf[16];  /* one scratch buffer for values that need formatting */

  htmlPageBytes = 0;
  if (webServer.chunkedResponseModeStart(200, "text/html") == false) {
    #ifdef CFG_DEBUG
    Serial.println("Client speaks HTTP/1.0, chunked page not sent");
    #endif /* CFG_DEBUG */
    return;
  }

  htmlSendText("<!DOCTYPE html> <html>\n");
  /* HEAD */
  htmlSendText("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n");
  htmlSendText("<title> "); htmlSendText(myConfig.name); htmlSendText("</title>\n");
  htmlSendText("<style>html { font-family: Helvetica; font-size: 16px; display: inline-block; margin: 0px auto; text-align: left;}\n");
  htmlSendText("body {background-color: #202226; color: #A0A2A8}\n");
  htmlSendText("p {color: #2686c1; margin-bottom: 10px;}\n");
  htmlSendText("table {color: #A0A2A8;}\n");
  htmlSendText("</style>\n");
  htmlSendText("</head>\n");
  /* BODY */
  htmlSendText("<body>\n");
  htmlSendText("<p><b>System information</b></p>");
  htmlSendText("<table>");
  /*= KEY ======================================||= VALUE ================================*/
  htmlRow2("Name", myConfig.name);
  htmlRow2Str("Chip ID", getEspChipId());
  #if CFG_BOARD_ESP8266
  snprintf(numBuf, sizeof(numBuf), "%d.%02d", static_cast<int>(ESP.getVcc() / 1000), static_cast<int>(ESP.getVcc() % 1000) / 10);
  htmlRow2("Vcc", numBuf);
  htmlRow2Str("Arduino Core", ESP.getCoreVersion());
  htmlRow2Str("Reset Reason", ESP.getResetReason());
  htmlRow2U32("Flash Size", ESP.getFlashChipRealSize());
  #elif CFG_BOARD_ESP32
  htmlRow2Str("Reset Reason CPU 0", getEspResetReason(rtc_get_reset_reason(0)));
  htmlRow2Str("Reset Reason CPU 1", getEspResetReason(rtc_get_reset_reason(1)));
  htmlRow2U32("CPU Frequency", static_cast<uint32_t>(ESP.getCpuFreqMHz()), " MHz");
  htmlRow2U32("Flash Speed", static_cast<uint32_t>(ESP.getFlashChipSpeed() / 1000000), " MHz");
  htmlRow2U32("Flash Size", ESP.getFlashChipSize());
  htmlRow2U32("Flash Mode", ESP.getFlashChipMode());
  htmlRow2("SDK Verison", ESP.getSdkVersion());
  #else
  #endif
  htmlRow2U32("Sketch Size", ESP.getSketchSize() / 1024, " kB");
  htmlRow2U32("Free for Sketch", ESP.getFreeSketchSpace() / 1024, " kB");
  htmlRow2U32("Free Heap", ESP.getFreeHeap() / 1024, " kB");
  htmlRow2Str("Time since Reset", millisFormatted());
  htmlRow2("Installed Firmware", VERSION);
  htmlRow2("Available Firmware", myConfig.available_firmware_version.c_str());
  htmlRow2Str("IPv4", WiFi.localIP().toString());
  htmlRow2Str("WiFi Status", wifiStatusToString(WiFi.status()));
  htmlRow2I32("WiFi Strength", static_cast<int32_t>(rssiInPercent), " %");
  htmlRow2U32("WiFi Connects", wifi_connect_counter);
  htmlRow2("MQTT Status", myMqttClient.connected() ? "connected" : "disconnected");
  htmlRow2U32("MQTT Connects", mqtt_connect_counter);
  htmlRow2Str("MQTT Last Error", myMqttHelper.mqttLastErrorToString((int8_t)myMqttClient.lastError()));
  htmlRow2("Local Time", time_buffer);
  htmlRow2U32("Looptime mean [&micro;s]", MyLooptime.get_time_duration_mean());
  htmlRow2U32("Looptime min [&micro;s]", MyLooptime.get_time_duration_min());
  htmlRow2U32("Looptime max [&micro;s]", MyLooptime.get_time_duration_max());
  htmlSendText("</table>");
  /* Change Input Method */
  htmlSendText("<p><b>Change Input Method</b></p>");
  htmlSendText("<form method='POST' autocomplete='off'>");

  if (myConfig.input_method == cPUSH_BUTTONS) {
    htmlSendText("<select name='InputMethod'> <option value='0'>Rotary Encoder </option> <option value='1' selected> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>");
  } else {
    htmlSendText("<select name='InputMethod'> <option value='0' selected> Rotary Encoder </option> <option value='1'> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>");
  }
  htmlSendText("</form>");
  /* Change sensor */
  htmlSendText("<p><b>Change Sensor</b></p>");
  htmlSendText("<form method='POST' autocomplete='off'>");
  htmlSendText("<select name='Sensor'> ");
  if (myConfig.sensor_type == cDHT22) {  /* select currently configured input method */
    htmlSendText("<option value='0' selected >DHT22 </option> <option value='1'> BME280 </option>");
  } else {
    htmlSendText("<option value='0'>DHT22 </option> <option value='1' selected > BME280 </option>");
  }
  htmlSendText("</select>&nbsp;<input type='submit' value='Submit'>");
  htmlSendText("</form>");

  /* Command Line */
  htmlSendText("<p><b>Command Line</b></p>");
  htmlSendText("<form method='POST' autocomplete='off'>");
  htmlSendText("<input type='text' name='cmd' value='key:value'>&nbsp;<input type='submit' value='Submit'>");
  htmlSendText("</form>");
  htmlSendText("<p><b>Commands</b></p>");
  /* COMMANDS TABLE */
  htmlSendText("<table style='font-size: 12px'>");
  htmlRow4("<b>Key</b>", "<b>Value</b>", "<b>Current Value</b>", "<b>Description</b>");
  htmlRow4("discover", "0 | 1", "none", "MQTT on demand device discovery: 1 = discover; 0 = remove");
  htmlRow4Int("discovery_enabled", "0 | 1", myConfig.discovery_enabled, "MQTT Autodiscovery: 1 = enabled; 0 = disabled");
  htmlRow4("name", "string", myConfig.name, "Define a name for this device");
  htmlRow4("mqtt_server_address", "string", myConfig.mqtt_host, "Address of the MQTT server");
  htmlRow4Int("mqtt_server_port", "1 .. 65535", myConfig.mqtt_port, "Port of the MQTT server");
  htmlRow4("mqtt_user", "string", myConfig.mqtt_user, "MQTT user");
  htmlRow4("mqtt_pwd", "string", "***", "MQTT password");
  htmlRow4Int("mqtt_pub_cycle", "Range: 1 .. 255, LSB: 1 min", myConfig.mqtt_publish_cycle, "Publish cycle for MQTT messages in minutes");
  htmlRow4("update_server_address", "string", myConfig.update_server_address, "Address of the update server");
  htmlRow4Int("auto_update", "0 | 1", myConfig.auto_update, "Automatically apply available updates: 1 = Auto Updates enabled; 0 = Auto Updates disabled");
  htmlRow4("update_firmware", "0 | 1", "none", "On demand firmware update: 1 = update");
  htmlRow4Int("planned_restart", "0 | 1", myConfig.planned_restart, "Execute planned restart");
  htmlRow4Int("calibration_offset", "Range: -50 .. +50, LSB: 0.1 &deg;C", myConfig.calibration_offset, "Offset calibration for temperature sensor.");
  htmlRow4Int("calibration_factor", "Range: +50 .. +200, LSB: 1 %", myConfig.calibration_factor, "Linearity calibration for temperature sensor.");
  htmlRow4Int("display_enabled", "0 | 1", myConfig.display_enabled, "Display always on or only on user interaction");
  htmlRow4Int("display_brightness", "Range: 0 .. +255, LSB: 1 step", myConfig.display_brightness, "Brightness of OLED display");
  htmlRow4("timezone", "string", myConfig.timezone, "Timezone for NTP, pick from this list: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv");
  htmlSendText("</table>");
  /* Restart Device */
  htmlSendText("<p><b>Restart Device</b></p>");
  if (systemRestartRequest == false) {
    htmlSendText("<button onclick=\"window.location.href='/restart'\"> Restart </button>");
  } else {
    htmlSendText("<button onclick=\"window.location.href='/'\"> Restarting, reload site </button>");
  }
  htmlSendText("</body>\n");
  htmlSendText("</html>\n");

  htmlSendChunk("", 0);  /* zero length chunk closes the chunked response */

  #ifdef CFG_DEBUG
  /* Serial.print() keeps the deep frames small; Serial.printf() allocates a 128 byte
     local buffer inside Print::vprintf in the cont stack that runs loop() */
  Serial.print("HTML page bytes streamed: ");
  Serial.println(htmlPageBytes);
  Serial.print("  Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("  Free ContStack at page build: ");
  Serial.println(ESP.getFreeContStack());
  #endif /* CFG_DEBUG */
}

void handleWebServerClient(void) {
  htmlStreamPage();  /* page is streamed in chunks, no page String is built */

  #ifdef CFG_DEBUG
  /* high water mark of the continuation stack that executes loop(): the core's
     cont_check() reports 'Stack smashing detected' when this stack runs out */
  Serial.print("Free ContStack after page send: ");
  Serial.println(ESP.getFreeContStack());
  #endif /* CFG_DEBUG */

  /* Evaluate Received Arguments */
  if (webServer.args() > 0) {  /* Arguments were received */
    for ( uint8_t i = 0; i < webServer.args(); i++ ) {
      #ifdef CFG_DEBUG
      Serial.println("HTTP arg(" + String(i) + "): " + webServer.argName(i) + " = " + webServer.arg(i));  /* Display each argument */
      #endif /* CFG_DEBUG */
      if (webServer.argName(i) == "cmd") {  /* check for cmd */
        String key = "";
        String value = "";
        if (splitHtmlCommand(webServer.arg(i), &key, &value)) {
          #ifdef CFG_DEBUG
          Serial.println(key + " : " + value);
          #endif /* CFG_DEBUG */
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
                Serial.println("Request FileSystem write with restart.");
                #endif
              } else {
                requestSaveToSpiffs = true;
                #ifdef CFG_DEBUG
                Serial.println("Request FileSystem write.");
                #endif
              }
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "update_server_address") {
            if ( (value != "") && (value != myConfig.update_server_address) ) {
              #ifdef CFG_DEBUG
              Serial.println("Request FileSystem write.");
              #endif
              strlcpy(myConfig.update_server_address, value.c_str(), sizeof(myConfig.update_server_address));
              requestSaveToSpiffsWithRestart = true;
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "auto_update") {
            boolean auto_update = static_cast<boolean>(value.toInt() & 1);
            if (auto_update != myConfig.auto_update) {
              myConfig.auto_update = auto_update;
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "planned_restart") {
            boolean planned_restart = static_cast<boolean>(value.toInt() & 1);
            if (planned_restart != myConfig.planned_restart) {
              myConfig.planned_restart = planned_restart;
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "calibration_factor") {
            uint8_t u8_value = (uint8_t) value.toInt();
            if ((u8_value != myConfig.calibration_factor) && (u8_value >= 50) && (u8_value <= 200)) {
              #ifdef CFG_DEBUG
              Serial.println("Request FileSystem write.");
              #endif
              myConfig.calibration_factor = int16_t(u8_value);
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "calibration_offset") {
            int16_t i16_value = (int16_t) value.toInt();
            if ((i16_value != myConfig.calibration_offset) && (i16_value >= -50) && (i16_value <= 50)) {
              #ifdef CFG_DEBUG
              Serial.println("Request FileSystem write.");
              #endif
              myConfig.calibration_offset = int16_t(i16_value);
              requestSaveToSpiffs = true;
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "display_brightness") {
            uint8_t u8_value = (uint8_t) value.toInt();
            if ((u8_value != myConfig.display_brightness) && (u8_value <= 255)) {
              #ifdef CFG_DEBUG
              Serial.println("Request FileSystem write.");
              #endif
              myConfig.display_brightness = u8_value;
              requestSaveToSpiffsWithRestart = true;
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "update_firmware") {
            if ((value.toInt() & 1) == true) {
              update_firmware = true;
              #ifdef CFG_DEBUG
              Serial.println("HTTP Update triggered via webpage");
              #endif
            }
          } else if (key == "display_enabled") {
            if ((value.toInt() & 1) == true) {
              myConfig.display_enabled = true;
              #ifdef CFG_DEBUG
              Serial.println("Display enabled via webpage");
              #endif
            } else if ((value.toInt() & 1) == false) {
              myConfig.display_enabled = false;
              #ifdef CFG_DEBUG
              Serial.println("Display disabled via webpage");
              #endif
            }
            requestSaveToSpiffs = true;
          } else if (key == "mqtt_server_address") {
            if ( (value != "") && (value != myConfig.mqtt_host) ) {
              strlcpy(myConfig.mqtt_host, value.c_str(), sizeof(myConfig.mqtt_host));
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              Serial.printf("New MQTT server configured: %s\n", myConfig.mqtt_host);
              #endif
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_server_port") {
            if ( (value.toInt() != myConfig.mqtt_port) && (value.toInt() > 0) && (value.toInt() <= UINT16_MAX) ) {
              myConfig.mqtt_port = value.toInt();
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              Serial.println("New MQTT port configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_user") {
            if ( (value != "") && (value != myConfig.mqtt_user) ) {
              strlcpy(myConfig.mqtt_user, value.c_str(), sizeof(myConfig.mqtt_user));
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              Serial.println("New MQTT user configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_pwd") {
            if ( (value != "") && (value != myConfig.mqtt_password) ) {
              strlcpy(myConfig.mqtt_password, value.c_str(), sizeof(myConfig.mqtt_password));
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              Serial.println("New MQTT password configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "mqtt_pub_cycle") {
            if ( (value.toInt() != myConfig.mqtt_publish_cycle) && (value.toInt() > 0) && (value.toInt() <= UINT8_MAX) ) {
              myConfig.mqtt_publish_cycle = value.toInt();
              requestSaveToSpiffs = true;
              #ifdef CFG_DEBUG
              Serial.println("New MQTT pyblish cycle configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          } else if (key == "timezone") {
            if ( (value != myConfig.timezone) && (value != "") ) {
              strlcpy(myConfig.timezone, value.c_str(), sizeof(myConfig.timezone));
              requestSaveToSpiffsWithRestart = true;    // restarting could be prevented by reconfiguration via configTzTime()
              #ifdef CFG_DEBUG
              Serial.println("New UTC offset configured.");
              #endif
            } else {
              #ifdef CFG_DEBUG
              Serial.println("Configuration unchanged, do nothing");
              #endif
            }
          }
        }
      }
      if (webServer.argName(i) == "InputMethod") {  /* check for dedicated arguments */
        if (webServer.arg(i) != String(myConfig.input_method) && (webServer.arg(i).toInt() >= 0) && (webServer.arg(i).toInt() < 2)) { /* check range and if changed at all */
          #ifdef CFG_DEBUG
          Serial.println("Request FileSystem write with restart.");
          #endif
          myConfig.input_method = static_cast<bool>(webServer.arg(i).toInt());
          requestSaveToSpiffsWithRestart = true;
        } else {
          #ifdef CFG_DEBUG
          Serial.println("Configuration unchanged, do nothing");
          #endif
        }
      }
      if (webServer.argName(i) == "Sensor") {  /* check for dedicated arguments */
        if (webServer.arg(i) != String(myConfig.sensor_type) && (webServer.arg(i).toInt() >= 0) && (webServer.arg(i).toInt() < 2)) { /* check range and if changed at all */
          #ifdef CFG_DEBUG
          Serial.println("Request FileSystem write with restart.");
          #endif
          myConfig.sensor_type = webServer.arg(i).toInt();
          requestSaveToSpiffsWithRestart = true;
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
  requestSaveToSpiffsWithRestart = true;
  handleWebServerClient();
}

/* MQTT callback if a message was received */
// cppcheck-suppress constParameter ; messageReceived is a library function
void messageReceived(String &topic, String &payload) {  // NOLINT
  #ifdef CFG_DEBUG
  Serial.println("received: " + topic + " : " + payload);
  #endif

  if (topic == myMqttHelper.getTopicSystemRestartRequest()) {
    #ifdef CFG_DEBUG
    Serial.println("Restart request received with value: " + payload);
    #endif
    if (payload == "PRESS") {
      systemRestartRequest = true;
      Serial.println("Restart requested by MQTT");
    }
  } else if (topic == myMqttHelper.getTopicUpdateFirmware()) { /* HTTP Update */
    if (payload == "PRESS") {
      #ifdef CFG_DEBUG
      Serial.println("Firmware updated triggered via MQTT");
      #endif
      update_firmware = true;
      myMqttHelper.setTriggerRemoveDiscovered(false);
    }
  } else if (topic == myMqttHelper.getTopicTargetTempCmd()) { /* check incoming target temperature, don't set same target temperature as new*/
    if (!isNumericPayload(payload)) {
      #ifdef CFG_DEBUG
      Serial.println("Non-numeric target temperature payload >> ignored: " + payload);
      #endif
    } else if (myThermostat.getTargetTemperature() != floatToInt(payload.toFloat())) {
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

    if (!isNumericPayload(payload)) {
      #ifdef CFG_DEBUG
      Serial.println("Non-numeric hysteresis payload >> ignored: " + payload);
      #endif
    } else {
      myThermostat.setThermostatHysteresis(payload.toInt());
    }
  } else if (topic == myMqttHelper.getTopicOutsideTemperature()) {
    #ifdef CFG_DEBUG
    Serial.println("New outside temperature received: " + payload);
    #endif
    if (!isNumericPayload(payload)) {
      /* Home Assistant sends "unknown" / "unavailable" when its own outside temperature
         source has no value; toFloat() on such a payload is 0.0 and would drive the
         thermostat to a false outside temperature, so the payload is dropped */
      #ifdef CFG_DEBUG
      Serial.println("Non-numeric outside temperature payload >> ignored");
      #endif
    } else {
      myThermostat.setOutsideTemperature(floatToInt(payload.toFloat()));
    }
  }
}
