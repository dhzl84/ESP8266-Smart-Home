/*===================================================================================================================*/
/* includes */
/*===================================================================================================================*/
#include "main.h"
#include "version.h"

#include <ESP8266WiFi.h>
#if CFG_SENSOR
#include <DHTesp.h>
#include "cSensorData.h"
#include "cMedianFilter.h"
#include <osapi.h>   /* for sensor timer */
#include <os_type.h> /* for sensor timer */
#endif
#if CFG_HEATING_CONTROL
#include "cHeatingControl.h"
#endif
#include "cSystemState.h"
#if CFG_DISPLAY
#include "UserFonts.h"
#include <SSD1306.h>
#endif
#if CFG_MQTT_CLIENT
#include "MQTTClient.h"
#include "cMQTT.h"
#endif /* CFG_MQTT_CLIENT */
#if CFG_HTTP_UPDATE
#include <ESP8266httpUpdate.h>
#endif
#if CFG_OTA
#include <ArduinoOTA.h>
#endif
#if CFG_SPIFFS
#include <FS.h>
#endif

/*===================================================================================================================*/
/* GPIO config */
/*===================================================================================================================*/
#define DHT_PIN              2 /* sensor */
#define SDA_PIN              4 /* I²C */
#define SCL_PIN              5 /* I²C */
#define ENCODER_PIN1        12 /* rotary left/right */
#define ENCODER_PIN2        13 /* rotary left/right */
#define ENCODER_SWITCH_PIN  14 /* push button switch */
#define RELAY_PIN           16 /* relay control */

/*===================================================================================================================*/
/* variables, constants, types, classes, etc, definitions */
/*===================================================================================================================*/
/* WiFi */
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PWD;

#if CFG_MQTT_CLIENT
const char*    mqttHost                = LOCAL_MQTT_HOST;
const int      mqttPort                = 1883;
unsigned long  MQTT_RECONNECT_REFERENCE_TIME;
boolean        MQTT_RECONNECT          = false;
#endif

#if CFG_HTTP_UPDATE
const String myUpdateServer = THERMOSTAT_BINARY;
boolean                 FETCH_UPDATE                    = false;       /* global variable used to decide whether an update shall be fetched from server or not */
#endif /* CFG_HTTP_UPDATE */

#if CFG_OTA
boolean                 OTA_UPDATE                      = false;       /* global variable used to change display in case OTA update is initiated */
#endif /* CFG_OTA */

#if CFG_ROTARY_ENCODER
#define switchDebounceTime  250
#define rotLeft              -1
#define rotRight              1
#define rotInit               0
#define tempStep              5
#define displayTemp           0
#define displayHumid          1
volatile unsigned long  SWITCH_DEBOUNCE_REF             = 0;           /* reference for debouncing the rotary encoder button switch, latches return of millis() to be checked in next switch interrupt */
volatile int            LAST_ENCODED                    = 0b11;        /* initial state of the rotary encoders gray code */
volatile int            ROTARY_ENCODER_DIRECTION_INTS   = rotInit;     /* initialize rotary encoder with no direction */
#endif

#if CFG_SENSOR
LOCAL os_timer_t SENSOR_TIMER;
DHTesp myDHT;
SensorData     mySensorData;
MedianFilter   myTemperatureFilter;
MedianFilter   myHumidityFilter;
#endif

#if CFG_DISPLAY
#define drawTempYOffset       16
#define drawTargetTempYOffset 0
#define drawHeating drawXbm(0,drawTempYOffset,myThermo_width,myThermo_height,myThermo)
SSD1306        myDisplay(0x3c,SDA_PIN,SCL_PIN);
#endif

#if CFG_HEATING_CONTROL
HeatingControl myHeatingControl;
#endif

#if CFG_MQTT_CLIENT
#define MQTT_MAIN_TOPIC "/heating/"
#endif /* CFG_MQTT_CLIENT */

WiFiClient     myWiFiClient;
SystemState    mySystemState;

#if CFG_MQTT_CLIENT
MQTTClient     myMqttClient;
mqttConfig     myMqttConfig;
#endif

#if CFG_SPIFFS
#if CFG_MQTT_CLIENT
#define        SPIFFS_MQTT_ID_FILE        String("/itsme")
#endif
#if CFG_SENSOR
#define        SPIFFS_SENSOR_CALIB_FILE   String("/sensor")
#endif
#if CFG_HEATING_CONTROL
#define        SPIFFS_TARGET_TEMP_FILE    String("/targetTemp")
#define        SPIFFS_WRITE_DEBOUNCE      20000 /* write target temperature to spiffs if it wasn't changed for 20 s (time in ms) */
boolean        SPIFFS_WRITTEN =           true;
unsigned long  SPIFFS_REFERENCE_TIME;
int            SPIFFS_LAST_TARGET_TEMPERATURE;
#endif /* CFG_HEATING_CONTROL */
#endif /* CFG_SPIFFS */

/*===================================================================================================================*/
/* function declarations */
/*===================================================================================================================*/
void GPIO_CONFIG(void);

#if CFG_SPIFFS
void SPIFFS_INIT(void);
void SPIFFS_MAIN(void);
#endif

#if CFG_DISPLAY
void DISPLAY_INIT(void);
#endif

void WIFI_CONNECT(void);

void HANDLE_SYSTEM_STATE(void);

#if CFG_SENSOR
void SENSOR_MAIN(void);
LOCAL void sensor_cb(void *arg); /* sensor timer callback */
#endif

#if CFG_HEATING_CONTROL
void HEATING_CONTROL_MAIN(void);
#endif

#if CFG_DISPLAY
void DRAW_DISPLAY_MAIN(void);
#endif

#if CFG_MQTT_CLIENT
void MQTT_CONNECT(void);
void MQTT_MAIN(void);
void messageReceived(String &topic, String &payload); /* MQTT callback */
#endif

#if CFG_OTA
void OTA_INIT(void);
void HANDLE_OTA_UPDATE(void);
#endif

#if CFG_HTTP_UPDATE
void HANDLE_HTTP_UPDATE(void);
#endif
#if CFG_ROTARY_ENCODER
void encoderSwitch (void);
void updateEncoder(void);
#endif

/*===================================================================================================================*/
/* library functions */
/*===================================================================================================================*/
boolean debounceCheck()
{
   boolean ret = false;

   unsigned long time = millis();
   if ((time - SWITCH_DEBOUNCE_REF) > switchDebounceTime)
   {
      SWITCH_DEBOUNCE_REF = time;
      ret = true;
   }
   else
   {
      ret = false;
   }

   return ret;
}

float intToFloat(int intValue)
{
   return ((float)(intValue/10.0));
}

int floatToInt(float floatValue)
{
   return ((int)(floatValue * 10));
}

String boolToStringOnOff(bool boolean)
{
   if (boolean == true)
   {
      return "on";
   }
   else
   {
      return "off";
   }
}

String boolToStringHeatOff(bool boolean)
{
   if (boolean == true)
   {
      return "heat";
   }
   else
   {
      return "off";
   }
}

#if CFG_SPIFFS
String readSpiffs(String file)
{
   String fileContent = "";

   if (SPIFFS.exists(file))
   {
      File f = SPIFFS.open(file, "r");
      if (!f) {
         Serial.println("oh no, failed to open file: "+ file);
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
   else
   {
      Serial.println("File " + file + " does not exist");
   }
   return fileContent;
}

boolean writeSpiffs(String file, String newFileContent)
{
   boolean success = false; /* return success = true if no error occured and the newFileContent equals the file content */
   String  currentFileContent = "";

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

#endif /* CFG_SPIFFS */

#if CFG_SENSOR
bool splitSensorDataString(String sensorCalib, int *offset, int *factor)
{
   bool ret = false;
   String delimiter = ";";
   size_t pos = 0;

   pos = (sensorCalib.indexOf(delimiter));

   /* don't accept empty substring or strings without delimiter */
   if ((pos > 0) && (pos < UINT32_MAX))
   {
      ret = true;
      *offset = (sensorCalib.substring(0, pos)).toInt();
      *factor = (sensorCalib.substring(pos+1)).toInt();
   }
   #ifdef CFG_DEBUG
   else
   {
      Serial.println("Malformed sensor calibration string >> calibration update denied");
   }

   Serial.println("Delimiter: " + delimiter);
   Serial.println("Position of delimiter: " + String(pos));
   Serial.println("Offset: " + String(*offset));
   Serial.println("Factor: " + String(*factor));
   #endif
   return ret;
}
#endif
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

   #if CFG_SPIFFS
   SPIFFS_INIT();   /* read stuff from SPIFFS */
   #endif

   GPIO_CONFIG();    /* configure GPIOs */

   #if CFG_SENSOR
   myDHT.setup(DHT_PIN, DHTesp::DHT22);    /* init DHT sensor */
   #endif

   #if CFG_DISPLAY
   DISPLAY_INIT();   /* init Display */
   #endif

   WIFI_CONNECT();   /* connect to WiFi */

   #if CFG_OTA
   OTA_INIT();
   #endif

   #if CFG_MQTT_CLIENT
   myMqttConfig.setup(MQTT_MAIN_TOPIC);
   MQTT_CONNECT();   /* connect to MQTT host and build subscriptions, must be called after SPIFFS_INIT()*/
   #endif

   #if CFG_SENSOR
   /* set timer to read sensor data every xx seconds */
   os_timer_disarm(&SENSOR_TIMER);
   os_timer_setfn(&SENSOR_TIMER, (os_timer_func_t *)sensor_cb, (void *)0);
   os_timer_arm(&SENSOR_TIMER, SENSOR_UPDATE_INTERVAL, 1);
   #endif

   #if CFG_ROTARY_ENCODER
   /* enable interrupts on encoder pins to decode gray code and recognize switch event*/
   attachInterrupt(ENCODER_PIN1, updateEncoder, CHANGE);
   attachInterrupt(ENCODER_PIN2, updateEncoder, CHANGE);
   attachInterrupt(ENCODER_SWITCH_PIN, encoderSwitch, FALLING);
   #endif
}

/*===================================================================================================================*/
/* functions called by setup() */
/*===================================================================================================================*/
#if CFG_SPIFFS
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

   #if CFG_MQTT_CLIENT
   /* check name for MQTT */

   String myName = readSpiffs(SPIFFS_MQTT_ID_FILE);

   if (myName != "")
   {
      myMqttConfig.setName(myName); /* set name from spiffs here, thus setup only needs to set the main topic */
   }
   else
   {
      Serial.println("File " + SPIFFS_MQTT_ID_FILE + " is empty or does not exist, proceed as 'unknown'");
   }

   Serial.println("My name is: " + myMqttConfig.getName());


   #endif /* CFG_MQTT_CLIENT */

   #if CFG_SENSOR
   String sensorCalib = readSpiffs(SPIFFS_SENSOR_CALIB_FILE);

   /* check parameters for Sensor calibration */
   if (sensorCalib != "")
   {
      int offset;
      int factor;

      /* split parameter string */
      if (splitSensorDataString(sensorCalib, &offset, &factor))
      {
         #ifdef CFG_DEBUG
         Serial.println("Offset read from /sensor is: " + String(offset) + " *C");
         Serial.println("Factor read from /sensor is: " + String(factor) + " %");
         #endif

         mySensorData.setSensorCalibData(offset, factor, false);
      }
      else
      {
         Serial.println("File " + SPIFFS_SENSOR_CALIB_FILE + " returned malformed string, proceed with uncalibrated sensor");
      }
   }
   else
   {
      Serial.println("File " + SPIFFS_SENSOR_CALIB_FILE + " is empty or does not exist, proceed with uncalibrated sensor");
   }

   #endif /* CFG_SENSOR */

   #if CFG_HEATING_CONTROL
   String targetTemp = readSpiffs(SPIFFS_TARGET_TEMP_FILE);
   if (targetTemp != "")
   {
      myHeatingControl.setTargetTemperature(targetTemp.toInt()); /* set temperature from spiffs here*/
      Serial.println("File " + SPIFFS_TARGET_TEMP_FILE + " is available, proceed with value: " + String(myHeatingControl.getTargetTemperature()));
   }
   else
   {
      Serial.println("File " + SPIFFS_TARGET_TEMP_FILE + " is empty or does not exist, proceed with init value: " + String(myHeatingControl.getTargetTemperature()));
   }

   SPIFFS_LAST_TARGET_TEMPERATURE = myHeatingControl.getTargetTemperature(); /* store this value also here to avoid unnecessary usage of SPIFFS */

   #endif /* CFG_HEATING_CONTROL */
}
#endif

void GPIO_CONFIG(void)
{
   #if CFG_HEATING_CONTROL
   myHeatingControl.setup(RELAY_PIN, myHeatingControl.getTargetTemperature()); /*GPIO to switch connected relay and initial target temperature */
   #endif
   /* initialize encoder pins */
   pinMode(ENCODER_PIN1, INPUT_PULLUP);
   pinMode(ENCODER_PIN2, INPUT_PULLUP);
   pinMode(ENCODER_SWITCH_PIN, INPUT_PULLUP);
}

#if CFG_DISPLAY
void DISPLAY_INIT(void)
{
   #ifdef CFG_DEBUG
   Serial.println("Initialize display");
   #endif
   Wire.begin();   /* needed for I�C communication with display */
   myDisplay.init();
   myDisplay.flipScreenVertically();
   myDisplay.clear();
   myDisplay.setFont(Roboto_Condensed_16);
   myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
   myDisplay.drawString(64,22,"Initialization");
   myDisplay.display();
}
#endif

void WIFI_CONNECT(void)
{
   /* init WiFi */
   if (WiFi.status() != WL_CONNECTED)
   {
      #ifdef CFG_DEBUG
      Serial.println("Initialize WiFi ");
      #endif

      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);

      #ifdef CFG_DEBUG
      Serial.println("WiFi Status: "+ String(WiFi.begin(ssid, password)));
      #endif
   }

   /* try to connect to WiFi, proceed offline if not connecting here*/
   while (WiFi.waitForConnectResult() != WL_CONNECTED) {
     Serial.println("Failed to connect to WiFi, continue offline");
     mySystemState.setSystemState(systemState_offline);
     return;
   }

   mySystemState.setSystemState(systemState_online);
   Serial.print("IP address: ");
   Serial.println(WiFi.localIP());

}

#if CFG_OTA
void OTA_INIT(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      /* convert string to char* */
      char *mqttName = new char[myMqttConfig.getName().length() + 1];
      strcpy(mqttName,myMqttConfig.getName().c_str());

      ArduinoOTA.setHostname(mqttName);

      delete [] mqttName;

      ArduinoOTA.onStart([]()
      {
         OTA_UPDATE = true;
         #if CFG_DISPLAY
         DRAW_DISPLAY_MAIN();
         #endif
         Serial.println("Start");
      });

      ArduinoOTA.onEnd([]()
      {
         OTA_UPDATE = false;
         Serial.println("\nEnd");
      });

      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
      {
         Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      });

      ArduinoOTA.onError([](ota_error_t error)
      {
         OTA_UPDATE = false;
         #if CFG_DISPLAY
         DRAW_DISPLAY_MAIN();
         #endif

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
#endif

#if CFG_MQTT_CLIENT
void MQTT_CONNECT(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      boolean ret;
      /* convert name string to char for connect API */
      char *mqttName = new char[myMqttConfig.getName().length() + 1];
      strcpy(mqttName,myMqttConfig.getName().c_str());

      /* convert name string to char for setWill API */
      char *mqttWillTopic = new char[myMqttConfig.getTopicState().length() + 1];
      strcpy(mqttWillTopic, myMqttConfig.getTopicState().c_str());

      /* broker shall publish 'offline' on ungraceful disconnect >> Last Will */
      myMqttClient.setWill(mqttWillTopic ,"offline", true, 1);
      #ifdef CFG_DEBUG
      Serial.println("MQTT set will " + String(mqttWillTopic) + ": offline");
      #endif

      myMqttClient.begin(mqttHost, mqttPort, myWiFiClient);
      /* register callback */
      myMqttClient.onMessage(messageReceived);

      /* connect to MQTT */
      ret = myMqttClient.connect(mqttName, LOCAL_MQTT_USER, LOCAL_MQTT_PWD);
      #ifdef CFG_DEBUG
      Serial.print("MQTT connect with name " + String(mqttName) + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif

      #if CFG_HEATING_CONTROL
      /* subscribe some topics */
      ret = myMqttClient.subscribe(myMqttConfig.getTopicTargetTempCmd());
      #ifdef CFG_DEBUG
      Serial.print("MQTT subscribe " + myMqttConfig.getTopicTargetTempCmd() + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif

      ret = myMqttClient.subscribe(myMqttConfig.getTopicHeatingAllowedCmd());
      #ifdef CFG_DEBUG
      Serial.print("MQTT subscribe " + myMqttConfig.getTopicHeatingAllowedCmd() + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif
      #endif /* CFG_HEATING_CONTROL */

      ret = myMqttClient.subscribe(myMqttConfig.getTopicUpdateFirmware());
      #ifdef CFG_DEBUG
      Serial.print("MQTT subscribe " + myMqttConfig.getTopicUpdateFirmware() + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif

      ret = myMqttClient.subscribe(myMqttConfig.getTopicChangeName());
      #ifdef CFG_DEBUG
      Serial.print("MQTT subscribe " + myMqttConfig.getTopicChangeName() + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif

      #if CFG_SENSOR
      ret = myMqttClient.subscribe(myMqttConfig.getTopicChangeSensorCalib());
      #ifdef CFG_DEBUG
      Serial.print("MQTT subscribe " + myMqttConfig.getTopicChangeSensorCalib() + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif
      #endif /* CFG_SENSOR */

      ret = myMqttClient.subscribe(myMqttConfig.getTopicSystemRestartRequest());
      #ifdef CFG_DEBUG
      Serial.print("MQTT subscribe " + myMqttConfig.getTopicSystemRestartRequest() + ": ");
      (ret == true) ? (Serial.println("success")) : (Serial.println("failed"));
      #endif

      /* publish online on connect */
      myMqttClient.publish(myMqttConfig.getTopicState(),"online", true, 1);
      #ifdef CFG_DEBUG
      Serial.println("MQTT publish " + myMqttConfig.getTopicState() + ": online");
      #endif

      /* publish restart = false on connect */
      myMqttClient.publish(myMqttConfig.getTopicSystemRestartRequest(),"0", true, 1);
      #ifdef CFG_DEBUG
      Serial.println("MQTT publish " + myMqttConfig.getTopicSystemRestartRequest() + ": 0");
      #endif

      /* publish firmware version on connect */
      myMqttClient.publish(myMqttConfig.getTopicFirmwareVersion(), FIRMWARE_VERSION, true, 1);
      #ifdef CFG_DEBUG
      Serial.println("MQTT publish " + myMqttConfig.getTopicFirmwareVersion() + ": " + FIRMWARE_VERSION);
      #endif

      delete [] mqttWillTopic;
      delete [] mqttName;
   }
}
#endif
/*===================================================================================================================*/
/* The loop function is called in an endless loop */
/*===================================================================================================================*/
void loop()
{
   HANDLE_SYSTEM_STATE(); /*handle system state class and trigger reconnects */

   #if CFG_SENSOR
   SENSOR_MAIN();   /* get sensor data */
   #endif

   #if CFG_HEATING_CONTROL
   HEATING_CONTROL_MAIN(); /* control relay for heating */
   #endif

   #if CFG_MQTT_CLIENT
   MQTT_MAIN(); /* handle MQTT each loop */
   #endif

   #if CFG_DISPLAY
   DRAW_DISPLAY_MAIN(); /* draw display each loop */
   #endif

   #if CFG_HTTP_UPDATE
   HANDLE_HTTP_UPDATE(); /* pull update from server if it was requested via MQTT*/
   #endif

   #if CFG_OTA
   HANDLE_OTA_UPDATE();
   #endif

   #if CFG_SPIFFS
   SPIFFS_MAIN();
   #endif

   yield();
}

/*===================================================================================================================*/
/* functions called by loop() */
/*===================================================================================================================*/
void HANDLE_SYSTEM_STATE(void)
{
   if (mySystemState.getSystemRestartRequest() == true)
   {
      #if CFG_DISPLAY
      DRAW_DISPLAY_MAIN();
      #endif /* CFG_DISPLAY */

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

         #if CFG_MQTT_CLIENT
         MQTT_CONNECT();
         #endif

         #if CFG_OTA
         OTA_INIT();
         #endif
      }
   }
}

#if CFG_SENSOR
void SENSOR_MAIN()
{
   float dhtTemp;
   float dhtHumid;

   if (mySensorData.getCheckSensor() == true) /* check sensor values this loop, set to true by sensor_cb(), else do nothing here */
   {
      mySensorData.setCheckSensor(false);    /* reset flag */

      /* Reading temperature or humidity takes about 250 milliseconds! */
      /* Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor) */
      dhtHumid = myDHT.getHumidity();
      dhtTemp  = myDHT.getTemperature();

      /* Check if any reads failed */
      if (isnan(dhtHumid) || isnan(dhtTemp)) {
         mySensorData.setLastSensorReadFailed(true);   /* set failure flag and exit SENSOR_MAIN() */
         #ifdef CFG_DEBUG
         Serial.println("Failed to read from DHT sensor! Failure counter: " + String(mySensorData.getSensorFailureCounter()));
         #endif
      }
      else
      {
         mySensorData.setLastSensorReadFailed(false);   /* set no failure during read sensor */

         mySensorData.setCurrentHumidity((int)(10* dhtHumid));     /* read value and convert to one decimal precision integer */
         mySensorData.setCurrentTemperature((int)(10* dhtTemp));   /* read value and convert to one decimal precision integer */

         myHumidityFilter.pushValue(mySensorData.getCurrentHumidity());                /* push current sensor value to filter */
         myTemperatureFilter.pushValue(mySensorData.getCurrentTemperature());          /* push current sensor value to filter */

         /* humidity: check if new filtered value is different to old one */
         if (mySensorData.getFilteredHumidity() != myHumidityFilter.getFilteredValue())
         {
            mySensorData.setFilteredHumidity(myHumidityFilter.getFilteredValue());     /* read back latest filtered value if different */
         }

         /* temperature: check if new filtered value is different to old one */
         if (mySensorData.getFilteredTemperature() != myTemperatureFilter.getFilteredValue())
         {
            mySensorData.setFilteredTemperature(myTemperatureFilter.getFilteredValue());     /* read back latest filtered value if different */
         }

         #ifdef CFG_DEBUG
         Serial.print("Temperature: ");
         Serial.print(intToFloat(mySensorData.getCurrentTemperature()),1);
         Serial.println(" *C ");
         Serial.print("Filtered temperature: ");
         Serial.print(intToFloat(mySensorData.getFilteredTemperature()),1);
         Serial.println(" *C ");

         Serial.print("Humidity: ");
         Serial.print(intToFloat(mySensorData.getCurrentHumidity()),1);
         Serial.println(" %");
         Serial.print("Filtered humidity: ");
         Serial.print(intToFloat(mySensorData.getFilteredHumidity()),1);
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
#endif

#if CFG_HEATING_CONTROL
void HEATING_CONTROL_MAIN(void)
{
   #if CFG_SENSOR
   if (mySensorData.getSensorError())
   {
      /* switch off heating if sensor does not provide values */
      #ifdef CFG_DEBUG
      Serial.println("not heating, sensor data invalid");
      #endif
      if (myHeatingControl.getHeatingEnabled() == true)
      {
         myHeatingControl.setHeatingEnabled(false);
      }
   }
   else /* sensor is healthy */
   {
      if (myHeatingControl.getHeatingAllowed() == true) /* check if heating is allowed by user */
      {
         if (mySensorData.getFilteredTemperature() < myHeatingControl.getTargetTemperature()) /* check if measured temperature is lower than heating target */
         {
            if (myHeatingControl.getHeatingEnabled() == false) /* switch on heating if target temperature is higher than measured temperature */
            {
               #ifdef CFG_DEBUG
               Serial.println("heating");
               #endif
               myHeatingControl.setHeatingEnabled(true);
            }
         }
         else if (mySensorData.getFilteredTemperature() > myHeatingControl.getTargetTemperature()) /* check if measured temperature is higher than heating target */
         {
            if (myHeatingControl.getHeatingEnabled() == true) /* switch off heating if target temperature is lower than measured temperature */
            {
               #ifdef CFG_DEBUG
               Serial.println("not heating");
               #endif
               myHeatingControl.setHeatingEnabled(false);
            }
         }
         else
         {
            /* remain in current heating state if temperatures are equal */
         }
      }
      else
      {
         /* disable heating if heating is set to not allowed by user */
         myHeatingControl.setHeatingEnabled(false);
      }
   }
   #endif /* CFG_SENSOR */
}
#endif

#if CFG_DISPLAY
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
   #if CFG_OTA
   else if (OTA_UPDATE == true)
   {
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      myDisplay.drawString(64,22,"Update");
   }
   #endif /* CFG_OTA */
   #if CFG_HTTP_UPDATE
   else if (FETCH_UPDATE == true)
   {
      myDisplay.setFont(Roboto_Condensed_16);
      myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
      myDisplay.drawString(64,22,"Update");
   }
   #endif /* CFG_HTTP_UPDATE */
   else
   {
      myDisplay.setTextAlignment(TEXT_ALIGN_RIGHT);
      myDisplay.setFont(Roboto_Condensed_32);

      #if CFG_SENSOR
      if (mySensorData.getSensorError())
      {
         myDisplay.drawString(128, drawTempYOffset, "err");
      }
      else
      {
         myDisplay.drawString(128, drawTempYOffset, String(intToFloat(mySensorData.getFilteredTemperature()),1));
      }
      #endif /* CFG_SENSOR */

      #if CFG_HEATING_CONTROL
      /* do not display target temperature if heating is not allowed */
      if (myHeatingControl.getHeatingAllowed() == true)
      {
         myDisplay.setFont(Roboto_Condensed_16);
         myDisplay.drawString(128, drawTargetTempYOffset, String(intToFloat(myHeatingControl.getTargetTemperature()),1));

         if (myHeatingControl.getHeatingEnabled()) /* heating */
         {
            myDisplay.drawHeating;
         }
      }
      #endif /* CFG_HEATING_CONTROL */
   }
   #ifdef CFG_DEBUG
   myDisplay.setTextAlignment(TEXT_ALIGN_LEFT);
   myDisplay.setFont(Roboto_Condensed_16);
   myDisplay.drawString(0, drawTargetTempYOffset, String(VERSION));
   #endif

   myDisplay.display();
}
#endif

#if CFG_MQTT_CLIENT
void MQTT_MAIN(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      if(!myMqttClient.connected())
      {
         if (MQTT_RECONNECT == false)
         {
            MQTT_RECONNECT = true;
            MQTT_RECONNECT_REFERENCE_TIME = millis();
         }
         else  /* try reconnect to MQTT broker after MQTT_RECONNECT_TIME expired */
         {
            if (MQTT_RECONNECT_REFERENCE_TIME + MQTT_RECONNECT_TIME < millis())
            {
               MQTT_RECONNECT = false;
               MQTT_CONNECT();
            }
            else
            {
               /* debounce reconnect */
               #ifdef CFG_DEBUG
               if ( (MQTT_RECONNECT_REFERENCE_TIME + MQTT_RECONNECT_TIME - millis()) % 100 == 0 )
               {
                  Serial.println("MQTT reconnect in: " + String((MQTT_RECONNECT_REFERENCE_TIME + MQTT_RECONNECT_TIME - millis())));
               }
               #endif
            }
         }

         return; /* no MQTT connection */
      }
      else
      {
          MQTT_RECONNECT = false; /* reset */
      }

      #if CFG_HTTP_UPDATE
      if (FETCH_UPDATE == true)
      {
         myMqttClient.publish(myMqttConfig.getTopicUpdateFirmwareAccepted(), String(false)); /* publish accepted update with value false to reset the switch in Home Assistant */
      }
      #endif /* CFG_HTTP_UPDATE */

      /* check if there is new data to transmit */
      if ( (mySensorData.getNewData()) || (myHeatingControl.getNewData()))
      {
         mySensorData.resetNewData();
         myHeatingControl.resetNewData();
         myMqttClient.publish(myMqttConfig.getTopicTemp()               ,String(intToFloat(mySensorData.getFilteredTemperature()))        ,true, 1); /* publish filtered temperature */
         myMqttClient.publish(myMqttConfig.getTopicHum()                ,String(mySensorData.getFilteredHumidity())                       ,true, 1); /* publish filtered humidity */
         myMqttClient.publish(myMqttConfig.getTopicHeatingState()       ,String(boolToStringOnOff(myHeatingControl.getHeatingEnabled()))  ,true, 1); /* publish heating state: 0 -> heating off, 1 -> heating on */
         myMqttClient.publish(myMqttConfig.getTopicTargetTempState()    ,String(intToFloat(myHeatingControl.getTargetTemperature()))      ,true, 1); /* publish target temperature as float */
         myMqttClient.publish(myMqttConfig.getTopicSensorStatus()       ,String(mySensorData.getSensorError())                            ,true, 1); /* publish sensor status: 0 -> good, 1 -> error */
         myMqttClient.publish(myMqttConfig.getTopicHeatingAllowedState(),String(boolToStringHeatOff(myHeatingControl.getHeatingAllowed()))  ,true, 1); /* publish if heating is allowed */
         myMqttClient.publish(myMqttConfig.getTopicSensorCalibFactor()  ,String(mySensorData.getSensorCalibFactor())                      ,true, 1); /* publish calibration factor */
         myMqttClient.publish(myMqttConfig.getTopicSensorCalibOffset()  ,String(mySensorData.getSensorCalibOffset())                      ,true, 1); /* publish calibration offset */
         myMqttClient.publish(myMqttConfig.getTopicDeviceIP()           ,WiFi.localIP().toString()                                        ,true, 1); /* publish device IP address */
      }

      myMqttClient.loop();
      delay(20); // <- fixes some issues with WiFi stability
   }
}
#endif

#if CFG_HTTP_UPDATE
void HANDLE_HTTP_UPDATE(void)
{
   if (FETCH_UPDATE == true)
   {
      #if CFG_DISPLAY
      DRAW_DISPLAY_MAIN();
      #endif

      FETCH_UPDATE = false;
      Serial.printf("Remote update started");

      t_httpUpdate_return ret = ESPhttpUpdate.update(myUpdateServer);

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
#endif

#if CFG_OTA
void HANDLE_OTA_UPDATE(void)
{
   if (systemState_online == mySystemState.getSystemState())
   {
      ArduinoOTA.handle();
   }
}
#endif
#if CFG_SPIFFS
void SPIFFS_MAIN(void)
{
   #if CFG_MQTT_CLIENT
   if (myMqttConfig.getNameChanged())
   {
      mySystemState.setSystemRestartRequest(true);

      if (!writeSpiffs(SPIFFS_MQTT_ID_FILE, myMqttConfig.getName()))
      {
         #ifdef CFG_DEBUG
         Serial.println("Writing to file: " + SPIFFS_MQTT_ID_FILE + " returned no success");
         #endif
         /* ToDo: implement retry */
      }
   }

   #if CFG_SENSOR
   if(mySensorData.getNewCalib())
   {
      mySystemState.setSystemRestartRequest(true);

      if (!writeSpiffs(SPIFFS_SENSOR_CALIB_FILE, String(mySensorData.getSensorCalibOffset()) + ";" + String(mySensorData.getSensorCalibFactor())))
      {
         #ifdef CFG_DEBUG
         Serial.println("Writing to file: " + SPIFFS_SENSOR_CALIB_FILE + " returned no success");
         #endif
         /* ToDo: implement retry */
      }

   }
   #endif /* CFG_SENSOR */
   #endif /* CFG_MQTT_CLIENT */

   #if CFG_HEATING_CONTROL
   /* avoid extensive writing to SPIFFS, therefore check if the target temperature didn't change for a certain time before writing. */
   if (myHeatingControl.getTargetTemperature() != SPIFFS_LAST_TARGET_TEMPERATURE)
   {
      #ifdef CFG_DEBUG
      Serial.println("Target temperature changed from. " + String(SPIFFS_LAST_TARGET_TEMPERATURE) + " to: " + String(myHeatingControl.getTargetTemperature()));
      #endif

      SPIFFS_REFERENCE_TIME = millis();
      SPIFFS_LAST_TARGET_TEMPERATURE = myHeatingControl.getTargetTemperature();
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
            if (writeSpiffs(SPIFFS_TARGET_TEMP_FILE, String(myHeatingControl.getTargetTemperature())))
            {
               SPIFFS_WRITTEN = true;
            }
            else
            {
               #ifdef CFG_DEBUG
               Serial.println("Writing to file: " + SPIFFS_TARGET_TEMP_FILE + " returned no success, retry next loop");
               #endif
            }

            #ifdef CFG_DEBUG
            Serial.println("New target temperature: " + String(myHeatingControl.getTargetTemperature()) + " stored in SPIFFS");
            #endif
         }
         else
         {
            /* debounce SPIFFS write */
            #ifdef CFG_DEBUG
            if ( (SPIFFS_REFERENCE_TIME + SPIFFS_WRITE_DEBOUNCE - millis()) % 100 == 0 ) /* modulo 100 to avoid excessive prints while debouncing */
            {
               Serial.println("Debouncing at: " + String((SPIFFS_REFERENCE_TIME + SPIFFS_WRITE_DEBOUNCE - millis())));
            }
            #endif
         }
      }
   }
   #endif /* CFG_HEATING_CONTROL */
}
#endif /* CFG_SPIFFS */
/*===================================================================================================================*/
/* callback, interrupt, timer functions */
/*===================================================================================================================*/



#if CFG_ROTARY_ENCODER
void ICACHE_RAM_ATTR encoderSwitch(void)
{
   /* debouncing routine for encoder switch */
   if (debounceCheck())
   {
      #if CFG_HEATING_CONTROL
      /* toggle heating allowed */
      myHeatingControl.toggleHeatingAllowed();
      #endif
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
         #if CFG_HEATING_CONTROL
         myHeatingControl.increaseTargetTemperature(tempStep);
         #endif /* CFG_HEATING_CONTROL */
      }
      else if (ROTARY_ENCODER_DIRECTION_INTS < rotLeft) /* if there was a higher amount of interrupts to the left, consider the encoder was turned to the left */
      {
         #if CFG_HEATING_CONTROL
         myHeatingControl.decreaseTargetTemperature(tempStep);
         #endif /* CFG_HEATING_CONTROL */
      }
      else
      {
         /* do nothing here, left/right interrupts have occurred with same amount -> should never happen */
      }
      ROTARY_ENCODER_DIRECTION_INTS = rotInit; /* reset interrupt count for next sequence */
   }

   LAST_ENCODED = encoded; /* store this value for next time */
}
#endif

#if CFG_SENSOR
LOCAL void ICACHE_FLASH_ATTR sensor_cb(void *arg) {
   mySensorData.setCheckSensor(true);     /* check sensor values next loop */
}
#endif

#if CFG_MQTT_CLIENT
void messageReceived(String &topic, String &payload)
{
   #ifdef CFG_DEBUG
   Serial.println("received: " + topic + " : " + payload);
   #endif

   if (topic == myMqttConfig.getTopicSystemRestartRequest())
   {
      #ifdef CFG_DEBUG
      Serial.println("Restart request received with value: " + payload);
      Serial.println("Parameter passed to Restart class: " + String((bool)(payload.toInt() & 1)));
      #endif

      mySystemState.setSystemRestartRequest((bool)(payload.toInt() & 1)); /* extract boolean from payload string */
   }

   #if CFG_HTTP_UPDATE
   if (topic == myMqttConfig.getTopicUpdateFirmware())
   {
      if (payload == "true")
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
   #endif /* CFG_HTTP_UPDATE */

   #if CFG_HEATING_CONTROL
   /* check incoming target temperature, don't set same target temperature as new*/
   if (topic == myMqttConfig.getTopicTargetTempCmd())
   {
      if (myHeatingControl.getTargetTemperature() != payload.toFloat())
      {
         /* set new target temperature for heating control, min/max limitation inside (string to float to int) */
         myHeatingControl.setTargetTemperature(floatToInt(payload.toFloat()));
      }
   }
   if (topic == myMqttConfig.getTopicHeatingAllowedCmd())
   {
      if (String("heat") == payload)
      {
         myHeatingControl.setHeatingAllowed(true);
      }
      else if (String("off") == payload)
      {
         myHeatingControl.setHeatingAllowed(false);
      }
      else
      {
          /* do nothing */
      }
   }
   #endif

   #if CFG_SPIFFS
   if (topic == myMqttConfig.getTopicChangeName())
   {
      #ifdef CFG_DEBUG
      Serial.println("New name received: " + payload);
      #endif
      if (payload != myMqttConfig.getName())
      {
         #ifdef CFG_DEBUG
         Serial.println("Old name was: " + myMqttConfig.getName());
         #endif
         myMqttConfig.changeName(payload);
      }
   }

   #if CFG_SENSOR
   if(topic == myMqttConfig.getTopicChangeSensorCalib())
   {
      #ifdef CFG_DEBUG
      Serial.println("New sensor calibration parameters received: " + payload);
      #endif

      int offset;
      int factor;

      if (splitSensorDataString(payload, &offset, &factor))
      {
         mySensorData.setSensorCalibData(offset, factor, true);
      }
   }
   #endif /* CFG_SENSOR */
   #endif /* CFG_SPIFFS */
}
#endif
