#ifndef _ESP8266_with_DHT22_H_
#define _ESP8266_with_DHT22_H_
#include <Arduino.h>

#include "config.h"
/* the config.h file contains your personal configuration of the parameters below:
#define WIFI_SSID          "xxxxxxxxx"
#define WIFI_PWD           "xxx"
#define LOCAL_MQTT_HOST    "123.456.789.012"
#define THERMOSTAT_BINARY  "http://<domain or ip>/<name>.bin"
#define S20_BINARY         "http://<domain or ip>/<name>.bin"
#define cThermostat  0
#define cS20         1
#define CFG_DEVICE   cThermostat
*/

#define CFG_DEBUG

/*===================================================================================================================*/
/* Configuration */
/*===================================================================================================================*/
#if (CFG_DEVICE == cThermostat)
#define SENSOR_UPDATE_INTERVAL      20000 /* 20s in milliseconds -> time between reading sensor */
#define MQTT_RECONNECT_TIME         10000
#define CFG_MQTT_CLIENT             true
#define CFG_HTTP_UPDATE             true
#define CFG_SPIFFS                  true
#define CFG_OTA                     true
#define CFG_DISPLAY                 true
#define CFG_SENSOR                  true
#define CFG_ROTARY_ENCODER          true
#define CFG_HEATING_CONTROL         true
#elif (CFG_DEVICE == cS20)
#define MQTT_RECONNECT_TIME         10000
#define CFG_MQTT_CLIENT             true
#define CFG_HTTP_UPDATE             true
#define CFG_SPIFFS                  true
#define CFG_OTA                     true
#define CFG_DISPLAY                 false
#define CFG_SENSOR                  false
#define CFG_ROTARY_ENCODER          false
#define CFG_HEATING_CONTROL         false
#else
#error "misconfiguration, check CFG_DEVICE"
#endif /* CFG_DEVICE */

#endif /* _ESP8266_with_DHT22_H_ */
