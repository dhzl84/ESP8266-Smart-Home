
#ifndef BOARD_H_
#define BOARD_H_

#include "Arduino.h"

#if CFG_BOARD_ESP8266
#define BOARD "ESP8266"
#define DHT_PIN              2 /* sensor */
#define SDA_PIN              4 /* I²C */
#define SCL_PIN              5 /* I²C */
#define PHYS_INPUT_1_PIN    12 /* rotary left/right OR down/up */
#define PHYS_INPUT_2_PIN    13 /* rotary left/right OR down/up */
#define PHYS_INPUT_3_PIN    14 /* on/off/ok/reset */
#define RELAY_PIN           16 /* relay control */
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#elif CFG_BOARD_ESP32
#define BOARD "ESP32"
#define DHT_PIN             GPIO_NUM_26 /* sensor */
#define SDA_PIN             GPIO_NUM_21 /* I²C */
#define SCL_PIN             GPIO_NUM_22 /* I²C */
/* tbd */
#define PHYS_INPUT_1_PIN    GPIO_NUM_5  /* rotary left/right OR up */
#define PHYS_INPUT_2_PIN    GPIO_NUM_18 /* rotary left/right OR down */
#define PHYS_INPUT_3_PIN    GPIO_NUM_19 /* on/off/ok/reset */
#define RELAY_PIN           GPIO_NUM_23 /* relay control */
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdate.h>
#include <SPIFFS.h>
#include <rom/rtc.h>  /* for reset reasons */
#else
#error "Wrong device configured!"
#endif

#endif  /* BOARD_H_ */
