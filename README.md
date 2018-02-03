# ESP8266

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

## Inwall thermostat

### SW configuration
```c++
#define CFG_DEVICE   cThermostat
```

### Assembly parts
* ESP8266-07
* DHT22 temeperture and humidity sensor
* 0,96" OLED display
* SRD-05VDC-SL-C 230V 10A relay
* IRM-03-5 230V AC to 5V DC print module
* 3,3 V DC voltage reulator
* rotary encoder with push button
* wires, resistors, capacitors, diodes, transistor, optocoupler, circuit board, etc

### Wiring
![image](https://user-images.githubusercontent.com/5675570/35767892-47fde138-08f4-11e8-863e-870828831ac0.png)
