#ifndef CONFIG_H_
#define CONFIG_H_

#define WIFI_SSID          "Bazinga"
#define WIFI_PWD           "04618389046386203237"
#define LOCAL_MQTT_HOST    "192.168.178.12"
#define THERMOSTAT_BINARY  "http://192.168.178.12:88/thermostat/ESP8266_with_DHT22_SW.bin"
#define S20_BINARY         "http://192.168.178.12:88/s20/ESP8266_with_DHT22_SW.bin"

#define cThermostat  0
#define cS20         1
#define CFG_DEVICE   cThermostat


#endif /* CONFIG_H_ */
