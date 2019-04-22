cat > config.h << EOF
#ifndef CONFIG_H_
#define CONFIG_H_

/* the config.h file contains your personal configuration of the parameters below: */
#define WIFI_SSID               "xxx"
#define WIFI_PWD                "xxx"
#define LOCAL_MQTT_HOST         "123.456.789.012"
#define LOCAL_MQTT_PORT         1883
#define LOCAL_MQTT_USER         "xxx"
#define LOCAL_MQTT_PWD          "xxx"
#define THERMOSTAT_BINARY       "http://<domain or ip>/<name>.bin"
#define WIFI_RECONNECT_TIME     30

#endif /* CONFIG_H_ */
EOF