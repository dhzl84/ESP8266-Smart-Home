#!/bin/bash

cat > config.h << EOF
#ifndef CONFIG_H_
#define CONFIG_H_

/* the config.h file contains your personal configuration of the parameters below: */
#define WIFI_SSID                   "xxx"
#define WIFI_PWD                    "xxx"
#define WIFI_RECONNECT_TIME         10  // Reconnect WiFi after xx seconds
#define SYSTEM_RESTART_TIME         5  // Restart after having to WiFi connection for xx minutes
#define PLANNED_SYSTEM_RESTART      true  // Restart every night
#define LOCAL_MQTT_HOST             "123.456.789.012"
#define LOCAL_MQTT_PORT             1883
#define LOCAL_MQTT_USER             "xxx"
#define LOCAL_MQTT_PWD              "xxx"
#define DEVICE_BINARY               "http://<domain or ip>/<name>.bin"
#define TIMEZONE                    "CET-1CEST,M3.5.0,M10.5.0/3"

#endif /* CONFIG_H_ */
EOF