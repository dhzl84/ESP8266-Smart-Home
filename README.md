# ESP8266 Smart Home Playground

This is my ESP8266 smart home playground for:  
* Self made floor heating sens and control units

Basic building blocks and technologies:
* [ESP8266-07](https://www.espressif.com/)
* WiFi 
* MQTT
* [Home Assistant](https://home-assistant.io/)
* c++

SW Built with PlatformIO in Visual Studio Code, some great [Arduino](https://www.arduino.cc) libraries and ESP8266 Arduino

[Circuit and PCB](https://github.com/dhzl84/ESP8266_Thermostat_PCB.git) made with [Target 3001](https://ibfriedrich.com/de/index.html)

## 1. Build Status Master: [![Build Status](https://travis-ci.org/dhzl84/ESP8266-Smart-Home.svg?branch=master)](https://travis-ci.org/dhzl84/ESP8266-Smart-Home) Develop: [![Build Status](https://travis-ci.org/dhzl84/ESP8266-Smart-Home.svg?branch=develop)](https://travis-ci.org/dhzl84/ESP8266-Smart-Home)
* Master branch only provides (pre-)release SW
* Develop may contain untested changes

### Dependencies
Platform:
* ESP8266 Arduino 2.5.0 (Espressif8266 2.0.1)

Custom libraries:
* DHT sensor library for ESPx
* ESP8266 and ESP32 Oled Driver for SSD1306 display
* ArduinoMQTT or PubSubClient
* ArduinoJSON

## 2. General hints
See [my Home Assistant configuration](https://github.com/dhzl84/Home-Assistant-Configuration) for the usage of this devices.

Since there is certain information in my software that I do not want everyone to know I located those in a single header file called *config.h* which is not part of the repository, and therefore the build will fail.

My *config.h* for building a thermostat SW contains the following information:
```c++
#define WIFI_SSID              "xxx"
#define WIFI_PWD               "xxx"
#define LOCAL_MQTT_HOST        "123.456.789.012"
#define LOCAL_MQTT_PORT        1234
#define LOCAL_MQTT_USER        "xxx"
#define LOCAL_MQTT_PWD         "xxx"
#define DEVICE_BINARY      "http://<domain or ip>/<name>.bin"
#define SENSOR_UPDATE_INTERVAL 20      /* seconds */
#define THERMOSTAT_HYSTERESIS  2       /* 0.2 °C */
#define WIFI_RECONNECT_TIME    30      /* seconds */
#define CFG_PUSH_BUTTONS       false

#define cArduinoMQTT  0
#define cPubSubClient 1
#define CFG_MQTT_LIB                cPubSubClient
```

For Travis CI compatibility there is the *config.sh* script which generates the above mentioned dummy code.


## 3 Inwall thermostat
I started to think about new thermostats for my floor heating system while trying to find a good setting for each room with the analog thermostats only giving the range ice cold (0) to 6, whatever temperature that should be.
So I thought about buying them but they were either expensive as hell or just didn't have the functionalities I wanted, thus I decided to build them on my own.

### 3.1 Features

**local:**
* sensing room temperature
* controlling room temperature
* display current room temperature
* display target temperature

**remote:**
* allow control from remote devices (smartphones, computers, etc.)
* provide a nice front end
* stay locally operational if not connected to a network

### 3.3 Assembly parts
* ESP8266-07
* DHT22 temeperture and humidity sensor
* 0,96" OLED display
* SRD-05VDC-SL-C 230V 10A relay
* IRM-03-5 230V AC to 5V DC print module
* 3,3 V DC voltage reulator
* rotary encoder with push button or just push buttons
* wires, resistors, capacitors, diodes, transistor, optocoupler, circuit board, etc

### 3.4 Wiring
![image](https://user-images.githubusercontent.com/5675570/35767892-47fde138-08f4-11e8-863e-870828831ac0.png)

### 3.5 Picture
![image](https://user-images.githubusercontent.com/5675570/50345529-b7659380-052f-11e9-8c72-13e437296978.jpg)
