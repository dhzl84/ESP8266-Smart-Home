/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include <Arduino.h>

#include "config.h"
/* the config.h file contains your personal configuration of the parameters below: 
  #define WIFI_SSID                   "xxx"
  #define WIFI_PWD                    "xxx"
  #define LOCAL_MQTT_USER             "xxx"
  #define LOCAL_MQTT_PWD              "xxx"
  #define LOCAL_MQTT_PORT             1234
  #define LOCAL_MQTT_HOST             "123.456.789.012"
  #define THERMOSTAT_BINARY           "http://<domain or ip>/<name>.bin"
  #define SENSOR_UPDATE_INTERVAL      20000
*/

#include "version.h"
#include "main.h"
#include <ESP8266WiFi.h>
#include <DHTesp.h>
#include "cThermostat.h"
#include <osapi.h>   /* for sensor timer */
#include <os_type.h> /* for sensor timer */
#include "cSystemState.h"
#include "UserFonts.h"
#include <SSD1306.h>
#include "MQTTClient.h"
#include "cMQTT.h"
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

/*===================================================================================================================*/
/* GPIO config */
/*===================================================================================================================*/
#define DHT_PIN              2 /* sensor */
#define SDA_PIN              4 /* I²C */
#define SCL_PIN              5 /* I²C */
#define ENCODER_PIN1        12 /* rotary left/right */
#define ENCODER_PIN2        13 /* rotary left/right */
#define ENCODER_BUTTON_PIN  14 /* push button switch */
#define RELAY_PIN           16 /* relay control */

/*===================================================================================================================*/
/* variables, constants, types, classes, etc. definitions */
/*===================================================================================================================*/
/* config */
struct configuration myConfig;
/* mqtt */
unsigned long  mqttReconnectTime       = 0;
unsigned long  mqttReconnectInterval   = MQTT_RECONNECT_TIME;
unsigned long  mqttPubCycleTime        = 0;
#define secondsToMillisecondsFactor 60000
/* loop handler */
#define loop50ms        50
#define loop100ms      100
#define loop500ms      500
#define loop1000ms    1000
unsigned long loop50msMillis   =  0;
unsigned long loop100msMillis  = 22; /* start loops with some offset to avoid calling all loops every second */
unsigned long loop500msMillis  = 44; /* start loops with some offset to avoid calling all loops every second */
unsigned long loop1000msMillis = 66; /* start loops with some offset to avoid calling all loops every second */
/* HTTP Update */
bool     FETCH_UPDATE          = false;       /* global variable used to decide whether an update shall be fetched from server or not */
/* OTA */
typedef enum otaUpdate { TH_OTA_IDLE, TH_OTA_ACTIVE, TH_OTA_FINISHED, TH_OTA_ERROR } OtaUpdate_t;       /* global variable used to change display in case OTA update is initiated */
OtaUpdate_t OTA_UPDATE = TH_OTA_IDLE;
/* rotary encoder */
unsigned long  switchDebounceTime          = 0;
unsigned long  switchDebounceInterval      = 250;
unsigned long  switchSystemResetTime       = 0;
unsigned long  switchSystemResetInterval   = 10000;
#define rotLeft              -1
#define rotRight              1
#define rotInit               0
#define tempStep              5
#define displayTemp           0
#define displayHumid          1
volatile int LAST_ENCODED                    = 0b11;        /* initial state of the rotary encoders gray code */
volatile int ROTARY_ENCODER_DIRECTION_INTS   = rotInit;     /* initialize rotary encoder with no direction */
/* sensor */
unsigned long readSensorScheduled = 0;
/* Display */
#define drawTempYOffset       16
#define drawTargetTempYOffset 0
#define drawHeating drawXbm(0,drawTempYOffset,myThermo_width,myThermo_height,myThermo)
/* classes */
DHTesp         myDHT;
SSD1306        myDisplay(0x3c,SDA_PIN,SCL_PIN);
WiFiClient     myWiFiClient;
Thermostat     myThermostat;
SystemState    mySystemState;
MQTTClient     myMqttClient(1024);
mqttHelper     myMqttHelper;

#define        SPIFFS_MQTT_ID_FILE        String("/itsme")
#define        SPIFFS_SENSOR_CALIB_FILE   String("/sensor")
#define        SPIFFS_TARGET_TEMP_FILE    String("/targetTemp")
#define        SPIFFS_WRITE_DEBOUNCE      20000 /* write target temperature to spiffs if it wasn't changed for 20 s (time in ms) */
boolean        SPIFFS_WRITTEN =           true;
unsigned long  SPIFFS_REFERENCE_TIME;

/*===================================================================================================================*/
/* The setup function is called once at startup of the sketch */
/*===================================================================================================================*/
void setup()
{
   Serial.begin(115200);
   delay(1000);
   #ifdef CFG_DEBUG
   Serial.println("Serial connection established");
   #endif

   SPIFFS_INIT();   /* read stuff from SPIFFS */
   GPIO_CONFIG();    /* configure GPIOs */
   myThermostat.setup(RELAY_PIN, myConfig.tTemp, myConfig.calibF, myConfig.calibO); /*GPIO to switch connected relay and initial target temperature */
   myDHT.setup(DHT_PIN, DHTesp::DHT22);    /* init DHT sensor */
   DISPLAY_INIT();   /* init Display */
   WIFI_CONNECT();   /* connect to WiFi */
   OTA_INIT();
   myMqttHelper.setup(String(myConfig.name));
   MQTT_CONNECT();   /* connect to MQTT host and build subscriptions, must be called after SPIFFS_INIT()*/

   /* enable interrupts on encoder pins to decode gray code and recognize switch event*/
   attachInterrupt(ENCODER_PIN1, updateEncoder, CHANGE);
   attachInterrupt(ENCODER_PIN2, updateEncoder, CHANGE);
   attachInterrupt(ENCODER_BUTTON_PIN, encoderSwitch, FALLING);
   
   SENSOR_MAIN();    /* acquire first sensor data before staring loop() */
}

/*===================================================================================================================*/
/* functions called by setup() */
/*===================================================================================================================*/
void SPIFFS_INIT(void)
{
   SPIFFS.begin();

   if (!SPIFFS.exists("/formatted"))
   {
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
      }
      else
      {
         f.close();
         delay(5000);
         if (!SPIFFS.exists("/formatted"))
         {
            Serial.println("That didn't work!");
         }
         else
         {
            #ifdef CFG_DEBUG
            Serial.println("Cool, working!");
            #endif
         }
      }
   }
   else
   {
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

   loadConfiguration(myConfig); // load config

   /* code for SPIFFS migraions from several files to one JSON file */
   if (SPIFFS.exists(SPIFFS_MQTT_ID_FILE))  // first time with new config
   {
      String myName = readSpiffs(SPIFFS_MQTT_ID_FILE);

      if (myName != "")
      {
         strlcpy(myConfig.name, myName.c_str(), sizeof(myConfig.name));
      }

      String targetTemp = readSpiffs(SPIFFS_TARGET_TEMP_FILE);
      if (targetTemp != "")
      {
         myConfig.tTemp = targetTemp.toInt(); /* set temperature from spiffs here*/
         SPIFFS.remove(SPIFFS_TARGET_TEMP_FILE);  // delete file for next startup
         Serial.println("File " + SPIFFS_TARGET_TEMP_FILE + " is available, proceed with value: " + String(myConfig.tTemp));
      }

      SPIFFS.remove(SPIFFS_MQTT_ID_FILE);       // delete file for next startup
      SPIFFS.remove(SPIFFS_SENSOR_CALIB_FILE);  // delete file for next startup
   }

   Serial.println("My name is: " + String(myConfig.name));
}

void GPIO_CONFIG(void)
{
   /* initialize encoder pins */
   pinMode(ENCODER_PIN1, INPUT_PULLUP);
   pinMode(ENCODER_PIN2, INPUT_PULLUP);
   pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
}

/* Display */
void DISPLAY_INIT(void)
{
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
   myDisplay.drawString(64, 44, FIRMWARE_VERSION);
   myDisplay.display();
}

void WIFI_CONNECT(void)
{
   /* init WiFi */
   if (WiFi.status() != WL_CONNECTED)
   {
      #ifdef CFG_DEBUG
      Serial.println("Initialize WiFi ");
      #endif

      WiFi.mode(WIFI_STA);
      WiFi.begin(myConfig.ssid, myConfig.wifiPwd);

      #ifdef CFG_DEBUG
      Serial.println("WiFi Status: "+ String(WiFi.begin(myConfig.ssid, myConfig.wifiPwd)));
      #endif
   }

   /* try to connect to WiFi, proceed offline if not connecting here*/
   if (WiFi.waitForConnectResult() != WL_CONNECTED)
   {
     Serial.println("Failed to connect to WiFi, continue offline");
     mySystemState.setSystemState(systemState_offline);
     return;
   }

   mySystemState.setSystemState(systemState_online);
   Serial.print("IP address: ");
   Serial.println(WiFi.localIP());

}

/* OTA */
void OTA_INIT(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      ArduinoOTA.setHostname((myMqttHelper.getLoweredName()).c_str());

      ArduinoOTA.onStart([]()
      {
         OTA_UPDATE = TH_OTA_ACTIVE;
         DRAW_DISPLAY_MAIN();
         Serial.println("Start");
      });

      ArduinoOTA.onEnd([]()
      {
         OTA_UPDATE = TH_OTA_FINISHED;
         DRAW_DISPLAY_MAIN();
         Serial.println("\nEnd");
      });

      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
      {
         Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      });

      ArduinoOTA.onError([](ota_error_t error)
      {
         OTA_UPDATE = TH_OTA_ERROR;
         DRAW_DISPLAY_MAIN();

         Serial.printf("Error[%u]: ", error);
         if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
         else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
         else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
         else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
         else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
      ArduinoOTA.begin();
   }
}

void MQTT_CONNECT(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      /* broker shall publish 'offline' on ungraceful disconnect >> Last Will */
      myMqttClient.setWill((myMqttHelper.getTopicLastWill()).c_str() ,"offline", false, 1);
      myMqttClient.begin(myConfig.mqttHost, myConfig.mqttPort, myWiFiClient);
      myMqttClient.onMessage(messageReceived);       /* register callback */
      (void)myMqttClient.connect((myMqttHelper.getLoweredName()).c_str(), myConfig.mqttUser, myConfig.mqttPwd);

      /* subscribe some topics */
      (void)myMqttClient.subscribe(myMqttHelper.getTopicTargetTempCmd());
      (void)myMqttClient.subscribe(myMqttHelper.getTopicThermostatModeCmd());
      (void)myMqttClient.subscribe(myMqttHelper.getTopicUpdateFirmware());
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeName());
      (void)myMqttClient.subscribe(myMqttHelper.getTopicChangeSensorCalib());
      (void)myMqttClient.subscribe(myMqttHelper.getTopicSystemRestartRequest());
      
      homeAssistantDiscovery();

      /* publish restart = false on connect */
      myMqttClient.publish(myMqttHelper.getTopicSystemRestartRequest(),       "0",                                            false, 1);
      /* publish online in will topic */
      myMqttClient.publish(myMqttHelper.getTopicLastWill(),                   "online",                                       true, 1);
   }
}

/*===================================================================================================================*/
/* The loop function is called in an endless loop */
/*===================================================================================================================*/
void loop()
{
  ArduinoOTA.handle();
  myMqttClient.loop();
  delay(20);               /* <- fixes some issues with WiFi stability */

  /* call every 50 ms */
  if (TimeReached(loop50msMillis))
  {
    SetNextTimeInterval(loop50msMillis, loop50ms);
    /* nothing yet */
  }
  /* call every 100 ms */
  if (TimeReached(loop100msMillis))
  {
    SetNextTimeInterval(loop100msMillis, loop100ms);
    HANDLE_SYSTEM_STATE();   /* handle system state class and trigger reconnects */
    myThermostat.loop();     /* control relay for heating */
    MQTT_MAIN();             /* handle MQTT each loop */
    DRAW_DISPLAY_MAIN();     /* draw display each loop */
  }
  /* call every 500 ms */
  if (TimeReached(loop500msMillis))
  {
    SetNextTimeInterval(loop500msMillis, loop500ms);
    /* nothing yet */
  }
  /* call every second */
  if (TimeReached(loop1000msMillis))
  {
    SetNextTimeInterval(loop1000msMillis, loop1000ms);
    SENSOR_MAIN();          /* get sensor data */
    SPIFFS_MAIN();          /* check for new data and write SPIFFS is necessary */
    HANDLE_HTTP_UPDATE();   /* pull update from server if it was requested via MQTT*/
  }
  yield();
}

/*===================================================================================================================*/
/* functions called by loop() */
/*===================================================================================================================*/
void HANDLE_SYSTEM_STATE(void)
{
   if (mySystemState.getSystemRestartRequest() == true)
   {
      DRAW_DISPLAY_MAIN();

      Serial.println("Restarting in 3 seconds");
      delay(3000);
      ESP.restart();
   }

   if (WiFi.status() == WL_CONNECTED)
   {
      if (mySystemState.getSystemState() == systemState_offline)
      {
         mySystemState.setSystemState(systemState_online);
      }
   }
   else
   {
      if (mySystemState.getSystemState() == systemState_online)
      {
         mySystemState.setSystemState(systemState_offline);
      }
   }

   if (systemState_offline == mySystemState.getSystemState())
   {
      if (mySystemState.getConnectDebounceTimer() == 0) /* getConnectDebounceTimer decrements the timer */
      {
         /* try to come online, debounced to avoid reconnect each loop*/
         WIFI_CONNECT();
         MQTT_CONNECT();
         OTA_INIT();
      }
   }
   /* check if rotary encoder button is pushed for 10 seconds to request a reset */
   if (LOW == digitalRead(ENCODER_BUTTON_PIN))
   {
      #ifdef CFG_DEBUG
      Serial.println("Encoder Button pressed: "+ String(digitalRead(ENCODER_BUTTON_PIN)));
      Serial.println("Target Time: "+ String(switchSystemResetTime + switchSystemResetInterval));
      Serial.println("Current time: "+ String(millis()));
      #endif

      if (switchSystemResetTime + switchSystemResetInterval < millis())
      {
         mySystemState.setSystemRestartRequest(true);
      }
      else
      {
         /* waiting */
      }
   }
   else
   {
      switchSystemResetTime = millis();
   }
}

void SENSOR_MAIN()
{
   float dhtTemp;
   float dhtHumid;

   /* schedule routine for sensor read */
   if (TimeReached(readSensorScheduled))
   {
      SetNextTimeInterval(readSensorScheduled, myConfig.sensUpdInterval);

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
      }
      else
      {
         myThermostat.setLastSensorReadFailed(false);   /* set no failure during read sensor */

         myThermostat.setCurrentHumidity((int)(10* dhtHumid));     /* read value and convert to one decimal precision integer */
         myThermostat.setCurrentTemperature((int)(10* dhtTemp));   /* read value and convert to one decimal precision integer */

         #ifdef CFG_DEBUG
         Serial.print("Temperature: ");
         Serial.print(intToFloat(myThermostat.getCurrentTemperature()),1);
         Serial.println(" *C ");
         Serial.print("Filtered temperature: ");
         Serial.print(intToFloat(myThermostat.getFilteredTemperature()),1);
         Serial.println(" *C ");

         Serial.print("Humidity: ");
         Serial.print(intToFloat(myThermostat.getCurrentHumidity()),1);
         Serial.println(" %");
         Serial.print("Filtered humidity: ");
         Serial.print(intToFloat(myThermostat.getFilteredHumidity()),1);
         Serial.println(" %");
         #endif

         #ifdef CFG_PRINT_TEMPERATURE_QUEUE
         for (int i = 0; i < CFG_MEDIAN_QUEUE_SIZE; i++)
         {
            int temperature;
            int humidity;
            if ( (myTemperatureFilter.getRawSampleValue(i,&temperature)) && (myHumidityFilter.getRawSampleValue(i,&humidity)))
            {
               Serial.print("Sample ");
               Serial.print(i);
               Serial.print(": ");
               Serial.print(intToFloat(temperature),1);
               Serial.print(", ");
               Serial.println(intToFloat(humidity),1);
            }
         }
         #endif
      }
   }
}

void DRAW_DISPLAY_MAIN(void)
{
   myDisplay.clear();

   myDisplay.setContrast(50);

   if (mySystemState.getSystemRestartRequest() == true)
   {
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      myDisplay.drawString(64,22,"Restart");
   }
   /* OTA */
   else if (OTA_UPDATE != TH_OTA_IDLE)
   {
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      switch (OTA_UPDATE) {
         case TH_OTA_ACTIVE:
            myDisplay.drawString(64,22,"Update");
            break;
         case TH_OTA_FINISHED:
            myDisplay.drawString(64,22,"Finished");
            break;
         case TH_OTA_ERROR:
            myDisplay.drawString(64,22,"Error");
            break;
      }
   }
   /* HTTP Update */
   else if (FETCH_UPDATE == true)
   {
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      myDisplay.drawString(64,22,"Update");
   }
   else
   {
      myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
      myDisplay.setFont(Roboto_Condensed_32);

      if (myThermostat.getSensorError())
      {
         myDisplay.drawString(128, drawTempYOffset, "err");
      }
      else
      {
         myDisplay.drawString(128, drawTempYOffset, String(intToFloat(myThermostat.getFilteredTemperature()),1));
      }

      /* do not display target temperature if heating is not allowed */
      if (myThermostat.getThermostatMode() == TH_HEAT)
      {
         myDisplay.setFont(Roboto_Condensed_16);
         myDisplay.drawString(128, drawTargetTempYOffset, String(intToFloat(myThermostat.getTargetTemperature()),1));

         if (myThermostat.getActualState()) /* heating */
         {
            myDisplay.drawHeating;
         }
      }
   }
   #ifdef CFG_DEBUG
   myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
   myDisplay.setFont(Roboto_Condensed_16);
   myDisplay.drawString(0, drawTargetTempYOffset, String(VERSION));
   #endif

   myDisplay.display();
}

void MQTT_MAIN(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      if(!myMqttClient.connected())
      {
         if (TimeReached(mqttReconnectTime))
         {
            MQTT_CONNECT();
            SetNextTimeInterval(mqttReconnectTime, mqttReconnectInterval); /* reset interval if MQTT_CONNECT was called*/
         }
         else  /* try reconnect to MQTT broker after mqttReconnectTime expired */
         {
           /* just wait */
         }
         return; /* no MQTT connection */
      }
      else
      {
         /* do nothing */
      }

      /* HTTP Update */
      if (FETCH_UPDATE == true)
      {
         myMqttClient.publish(myMqttHelper.getTopicUpdateFirmwareAccepted(), String(false), false, 1); /* publish accepted update with value false to reset the switch in Home Assistant */
      }

      /* check if there is new data to publish and shift PubCycle if data is published on event*/
      if (myThermostat.getNewData())
      {
         myThermostat.resetNewData();
         SetNextTimeInterval(mqttPubCycleTime, myConfig.mqttPubCycleInterval * secondsToMillisecondsFactor);
         mqttPubState();
      }
      else if (TimeReached(mqttPubCycleTime) )   /* check if data was not published for the PubCycleInterval */
      {
         SetNextTimeInterval(mqttPubCycleTime, myConfig.mqttPubCycleInterval * secondsToMillisecondsFactor);
         mqttPubState();
      }
   }
}

void HANDLE_HTTP_UPDATE(void)
{
   if (FETCH_UPDATE == true)
   {
      DRAW_DISPLAY_MAIN();
      FETCH_UPDATE = false;
      Serial.printf("Remote update started");

      t_httpUpdate_return ret = ESPhttpUpdate.update(myConfig.updServer);

      switch(ret) {
          case HTTP_UPDATE_FAILED:
              Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
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

void SPIFFS_MAIN(void)
{
   if (myMqttHelper.getNameChanged())
   {
      strlcpy(myConfig.name, myMqttHelper.getName().c_str(), sizeof(myConfig.name));
      if (saveConfiguration(myConfig))
      {
         /* write successful, restart to rebuild MQTT topics etc. */
         mySystemState.setSystemRestartRequest(true);
      }
      else
      {
         /* write failed, retry next loop */
      }
   }

   if(myThermostat.getNewCalib())
   {
      myConfig.calibF = myThermostat.getSensorCalibFactor();
      myConfig.calibO = myThermostat.getSensorCalibOffset();
      if (saveConfiguration(myConfig))
      {
         myThermostat.resetNewCalib();
      }
      else
      {
         /* write failed, retry next loop */
      }
   }

   /* avoid extensive writing to SPIFFS, therefore check if the target temperature didn't change for a certain time before writing. */
   if (myThermostat.getTargetTemperature() != myConfig.tTemp)
   {
      SPIFFS_REFERENCE_TIME = millis();  // ToDo: handle wrap around
      myConfig.tTemp = myThermostat.getTargetTemperature();
      SPIFFS_WRITTEN = false;
   }
   else /* target temperature not changed this loop */
   {
      if (SPIFFS_WRITTEN == true)
      {
         /* do nothing, last change was already stored in SPIFFS */
      }
      else /* latest change not stored in SPIFFS */
      {
         if (SPIFFS_REFERENCE_TIME + SPIFFS_WRITE_DEBOUNCE < millis()) /* check debounce */
         {
            /* debounce expired -> write */
            if (saveConfiguration(myConfig))
            {
               SPIFFS_WRITTEN = true;
            }
            else
            {
               /* SPIFFS not written, retry next loop */
            }
         }
         else
         {
            /* debounce SPIFFS write */
         }
      }
   }
}
/*===================================================================================================================*/
/* callback, interrupt, timer, other functions */
/*===================================================================================================================*/

void ICACHE_RAM_ATTR encoderSwitch(void)
{
   /* debouncing routine for encoder switch */
   if (TimeReached(switchDebounceTime))
   {
      SetNextTimeInterval(switchDebounceTime, switchDebounceInterval);
      myThermostat.toggleThermostatMode();
   }
}

void ICACHE_RAM_ATTR updateEncoder(void)
{
   int MSB = digitalRead(ENCODER_PIN1);
   int LSB = digitalRead(ENCODER_PIN2);

   int encoded = (MSB << 1) |LSB;  /* converting the 2 pin value to single number */

   int sum  = (LAST_ENCODED << 2) | encoded; /* adding it to the previous encoded value */

   if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) /* gray patterns for rotation to right */
   {
      ROTARY_ENCODER_DIRECTION_INTS += rotRight; /* count rotation interrupts to left and right, this shall make the encoder routine more robust against bouncing */
   }
   if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)  /* gray patterns for rotation to left */
   {
      ROTARY_ENCODER_DIRECTION_INTS += rotLeft;
   }

   if (encoded == 0b11) /* if the encoder is in an end position, evaluate the number of interrupts to left/right. simple majority wins */
   {
      #ifdef CFG_DEBUG
      Serial.println("Rotary encoder interrupts: " + String(ROTARY_ENCODER_DIRECTION_INTS));
      #endif

      if(ROTARY_ENCODER_DIRECTION_INTS > rotRight) /* if there was a higher amount of interrupts to the right, consider the encoder was turned to the right */
      {
          myThermostat.increaseTargetTemperature(tempStep);
       }
      else if (ROTARY_ENCODER_DIRECTION_INTS < rotLeft) /* if there was a higher amount of interrupts to the left, consider the encoder was turned to the left */
      {
          myThermostat.decreaseTargetTemperature(tempStep);
       }
      else
      {
         /* do nothing here, left/right interrupts have occurred with same amount -> should never happen */
      }
      ROTARY_ENCODER_DIRECTION_INTS = rotInit; /* reset interrupt count for next sequence */
   }

   LAST_ENCODED = encoded; /* store this value for next time */
}

   /* Home Assistant discovery on connect; used to define entities in HA to communicate with*/
void homeAssistantDiscovery(void)
{
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryClimate(),       myMqttHelper.buildHassDiscoveryClimate(),       true, 1);    // make HA discover the climate component
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoveryBinarySensor(),  myMqttHelper.buildHassDiscoveryBinarySensor(),  true, 1);    // make HA discover the binary_sensor for sensor failure
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sTemp),   myMqttHelper.buildHassDiscoverySensor(sTemp),   true, 1);    // make HA discover the temperature sensor
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sHum),    myMqttHelper.buildHassDiscoverySensor(sHum),    true, 1);    // make HA discover the humidity sensor
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sIP),     myMqttHelper.buildHassDiscoverySensor(sIP),     true, 1);    // make HA discover the IP sensor
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibF), myMqttHelper.buildHassDiscoverySensor(sCalibF), true, 1);    // make HA discover the sensor scaling sensor
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sCalibO), myMqttHelper.buildHassDiscoverySensor(sCalibO), true, 1);    // make HA discover the sensor offset sensor
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySensor(sFW),     myMqttHelper.buildHassDiscoverySensor(sFW),     true, 1);    // make HA discover the firmware version sensor
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swReset), myMqttHelper.buildHassDiscoverySwitch(swReset), true, 1);    // make HA discover the reset switch
   myMqttClient.publish(myMqttHelper.getTopicHassDiscoverySwitch(swUpdate),myMqttHelper.buildHassDiscoverySwitch(swUpdate),true, 1);    // make HA discover the update switch
}

/* publish state topic in JSON format */
void mqttPubState(void)
{
   myMqttClient.publish( \
      myMqttHelper.getTopicData(), /* topic */ \
      myMqttHelper.buildStateJSON( /* JSON payload */\
         String(intToFloat(myThermostat.getFilteredTemperature()),1), \
         String(intToFloat(myThermostat.getFilteredHumidity()),1), \
         String(boolToStringOnOff(myThermostat.getActualState())), \
         String(intToFloat(myThermostat.getTargetTemperature()),1), \
         String(myThermostat.getSensorError()), \
         String(boolToStringHeatOff(myThermostat.getThermostatMode())), \
         String(myThermostat.getSensorCalibFactor()), \
         String(intToFloat(myThermostat.getSensorCalibOffset()),0), \
         WiFi.localIP().toString(), \
         String(FIRMWARE_VERSION) ), \
      true, /* retain */ \
      1 /* QoS */\
   );
}

/* MQTT callback if a message was received */
void messageReceived(String &topic, String &payload)
{
   #ifdef CFG_DEBUG
   Serial.println("received: " + topic + " : " + payload);
   #endif

   if (topic == myMqttHelper.getTopicSystemRestartRequest())
   {
      #ifdef CFG_DEBUG
      Serial.println("Restart request received with value: " + payload);
      #endif
      if ( (payload == "true") || (payload == "1") )
      {
         mySystemState.setSystemRestartRequest(true);
      }
   }

   /* HTTP Update */
   else if (topic == myMqttHelper.getTopicUpdateFirmware())
   {
      if ( (payload == "true") || (payload == "1") )
      {
         #ifdef CFG_DEBUG
         Serial.println("Firmware updated triggered via MQTT");
         #endif
         FETCH_UPDATE = true;
      }
      else
      {
         FETCH_UPDATE = false;
      }
   }

   /* check incoming target temperature, don't set same target temperature as new*/
   else if (topic == myMqttHelper.getTopicTargetTempCmd())
   {
      if (myThermostat.getTargetTemperature() != payload.toFloat())
      {
         /* set new target temperature for heating control, min/max limitation inside (string to float to int) */
         myThermostat.setTargetTemperature(floatToInt(payload.toFloat()));
      }
   }


   else if (topic == myMqttHelper.getTopicThermostatModeCmd())
   {
      if (String("heat") == payload)
      {
         myThermostat.setThermostatMode(TH_HEAT);
      }
      else if (String("off") == payload)
      {
         myThermostat.setThermostatMode(TH_OFF);
      }
      else
      {
          /* do nothing */
      }
   }


   else if (topic == myMqttHelper.getTopicChangeName())
   {
      #ifdef CFG_DEBUG
      Serial.println("New name received: " + payload);
      #endif
      if (payload != myMqttHelper.getName())
      {
         #ifdef CFG_DEBUG
         Serial.println("Old name was: " + myMqttHelper.getName());
         #endif
         myMqttHelper.changeName(payload);
      }
   }


   else if(topic == myMqttHelper.getTopicChangeSensorCalib())
   {
      #ifdef CFG_DEBUG
      Serial.println("New sensor calibration parameters received: " + payload);
      #endif

      int offset;
      int factor;

      if (splitSensorDataString(payload, &offset, &factor))
      {
         myThermostat.setSensorCalibData(factor, offset, true);
      }
   }
}

