# ESP8266 / ESP32 Smart Home Playground

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
![Build Status](https://github.com/dhzl84/ESP8266-Smart-Home/actions/workflows/main.yml/badge.svg)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/62f9be8a6ab441ec82306f1c18f8c0b3)](https://www.codacy.com/manual/dhzl84/ESP8266-Smart-Home/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=dhzl84/ESP8266-Smart-Home&amp;utm_campaign=Badge_Grade)

**This is my ESP8266 / ESP32 smart home playground for self made floor heating sens and control units.**

:warning: This project partly uses 230 V so be careful and hands off if you don't know what you are doing.

Basic building blocks and technologies:

* [ESP8266 or ESP32](https://www.espressif.com/)
* WiFi
* MQTT
* [Home Assistant](https://home-assistant.io/)
* c++

SW Built with PlatformIO in Visual Studio Code, some great [Arduino](https://www.arduino.cc) libraries and ESP8266 Arduino

[Circuit and PCB](https://github.com/dhzl84/ESP8266_Thermostat_PCB.git) made with [Target 3001](https://ibfriedrich.com/de/index.html)

## Features

**local:**

* sensing room temperature
* controlling room temperature
* display current room temperature
* display target temperature

**remote:**

* allow control from remote devices (smartphones, computers, etc.)
* provide a nice front end
* stay locally operational if not connected to a network

## Software

* Master branch only provides (pre-)release SW
* Develop may contain untested changes

### Dependencies

Platforms:

* ESP8266 Core for Arduino
* ESP32 Core for Arduino

Arduino Libraries:

* DHT sensor library for ESPx
* ESP8266_SSD1306
* ArduinoMQTT
* ArduinoJSON
* Bounce2

### General Hints

See [my Home Assistant configuration](https://github.com/dhzl84/Home-Assistant-Configuration) for the usage of these devices.

Since there is certain information in my software that I do not want everyone to know I located those in a single header file called *config.h* which is not part of the repository, and therefore the build will fail.

My *config.h* for building a thermostat SW contains the following information:

```c++
#define WIFI_SSID              "xxx"
#define WIFI_PWD               "xxx"
#define LOCAL_MQTT_HOST        "123.456.789.012"
#define LOCAL_MQTT_PORT        1234
#define LOCAL_MQTT_USER        "xxx"
#define LOCAL_MQTT_PWD         "xxx"
#define DEVICE_BINARY          "http://<domain or ip>/<name>.bin"
```

For Travis CI compatibility there is the *config.sh* script which generates the above mentioned dummy code.

Once connect to your WiFi, a website is served on port 80 allowing further configuration.

## Hardware

I started to think about new thermostats for my floor heating system while trying to find a good setting for each room with the analog thermostats only giving the range ice cold (0) to 6, whatever temperature that should be.
So I thought about buying them but they were either expensive as hell or just didn't have the functionalities I wanted, thus I decided to build them on my own.

### Assembly parts

* Microcontrollers: (alternatives)
  * ESP8266
  * ESP32
* Sensors: (alternatives)
  * DHT22 Temperature / Humidity
  * BME 280 Temperature / Humidity / Pressure
* 0,96" OLED display
* SRD-05VDC-SL-C 230V 10A relay
* IRM-03-5 230V AC to 5V DC print module
* 3,3 V DC voltage regulator
* Local Control via: (alternatives)
  * rotary encoder with push button
  * three push buttons
* wires, resistors, capacitors, diodes, transistor, optocoupler, circuit board, etc

### Schematic

The schemnatic shows the wiring for all variants. The sensors DHT22 and BME280 are meant to be alternatives. Same applies to the Rotary Encoder and the Push Buttons for local control.

![image](https://user-images.githubusercontent.com/5675570/77818501-36eaf680-70d3-11ea-9c11-1c7bbd2b8cc5.png)

### 3D Printed Parts

will be added soon

### Picture

![image](https://user-images.githubusercontent.com/5675570/50345529-b7659380-052f-11e9-8c72-13e437296978.jpg)
