cat > config.h << EOF
#ifndef CONFIG_H_
#define CONFIG_H_

#define WIFI_SSID          "xxx"
#define WIFI_PWD           "xxx"
#define LOCAL_MQTT_HOST    "123.456.789.012"
#define THERMOSTAT_BINARY  "xxx"
#define S20_BINARY         "xxx"
#define cThermostat  0
#define cS20         1
#define CFG_DEVICE   $1

#endif /* CONFIG_H_ */
EOF