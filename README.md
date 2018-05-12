# ESP8266 Smart Home Playground

This is my ESP8266 smart home playground for:  
* Self made floor heating sens and control units
* Sonoff S20 plugs

Basic building blocks and technologies:
* [ESP8266-07](https://www.espressif.com/)
* WiFi 
* MQTT
* [Home Assistant](https://home-assistant.io/)

SW Built with [Sloeber IDE 4.2](http://eclipse.baeyens.it/), lots of great [Arduino](https://www.arduino.cc) libraries and ESP8266 Arduino 2.4.0  
Circuit and PCB made with [Target 3001](https://ibfriedrich.com/de/index.html)

## 1. Build Status [![Build Status](https://travis-ci.org/dhzl84/ESP8266-Smart-Home.svg?branch=master)](https://travis-ci.org/dhzl84/ESP8266-Smart-Home)
* Master branch
* Arduino IDE 1.8.5
* ESP8266 Arduino 2.4.1
* latest libraries:
  1. Adafruit Unified Sensor
  2. DHT sensor library for ESPx
  3. ESP8266 and ESP32 Oled Driver for SSD1306 display
  4. MQTT
* Thermostat config
* S20 config

## 2. General hints
See [my Home Assistant configuration](https://github.com/dhzl84/Home-Assistant-Configuration) for the usage of this devices.

Since there is certain information in my software that I do not want everyone to know I located those in a single header file called *config.h* which is not part of the repository, and therefore the build will fail.

My *config.h* for building a thermostat SW contains the following information:
```c++
#define WIFI_SSID          "xxx"
#define WIFI_PWD           "xxx"
#define LOCAL_MQTT_HOST    "123.456.789.012"
#define LOCAL_MQTT_USER    "xxx"
#define LOCAL_MQTT_PWD     "xxx"
#define THERMOSTAT_BINARY  "http://<domain or ip>/<name>.bin"
#define S20_BINARY         "http://<domain or ip>/<name>.bin"
#define cThermostat        0
#define cS20               1
#define CFG_DEVICE         $1
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
* switch on/off

**remote:**
* allow control from remote devices (smartphones, computers, etc.)
* provide a nice front end
* stay locally operational if not connected to a network

### 3.2SW configuration
```c++
#define CFG_DEVICE   cThermostat
```

### 3.3 Assembly parts
* ESP8266-07
* DHT22 temeperture and humidity sensor
* 0,96" OLED display
* SRD-05VDC-SL-C 230V 10A relay
* IRM-03-5 230V AC to 5V DC print module
* 3,3 V DC voltage reulator
* rotary encoder with push button
* wires, resistors, capacitors, diodes, transistor, optocoupler, circuit board, etc

### 3.4 Wiring
![image](https://user-images.githubusercontent.com/5675570/35767892-47fde138-08f4-11e8-863e-870828831ac0.png)

## 4. Sonoff S20
While having already some 433 MHz outlets setup I decided to buy some of those nice devices to connect all the christmas lights to Home Assistant and switch them reliably.
I had a look at some available software solutions but then decided to just stick to my base software and extend it a bit for the S20.

### 4.1 Features

**local:**
* switch on/off via button

**remote:**
* switch on/off from remote devices (smartphones, computers, etc.)
* switch on/off from remote controls (433 MHz, EnOcean)
* configurable time based automations

### 4.2 SW configuration
```c++
#define CFG_DEVICE   cS20
```
