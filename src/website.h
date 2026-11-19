#ifndef WEBSITE_H_
#define WEBSITE_H_

/*===================================================================================================================*/
/* the web page                                                                                            */
/*===================================================================================================================*/
/* Everything the page needs lives in website.cpp; this header is the entry point main.cpp calls and the
   declarations of the objects and counters the page renders, which main.cpp owns.

   The page is streamed with the core's chunked transfer encoding: every row is formatted into a small stack
   buffer with snprintf and written straight to the client, so there is no page String in the heap, no heap
   realloc and no concatenation frames on the cont stack that runs loop(). */

#include "main.h"          /* the board, config and version macros and the library helpers the page renders */
#include "c_mqtt.h"        /* mqttHelper: the class of myMqttHelper, whose last error the page shows */
#include "MQTTClient.h"    /* MQTTClient: the class of myMqttClient, whose connection state the page shows */

/*===================================================================================================================*/
/* objects and counters main.cpp owns; the page renders them                                                */
/*===================================================================================================================*/
extern uint32_t wifi_connect_counter;
extern uint32_t mqtt_connect_counter;
extern bool     update_firmware;
extern struct Configuration myConfig;
extern mqttHelper myMqttHelper;
extern MQTTClient myMqttClient;
extern DiffTime MyLooptime;
extern char time_buffer[];
extern bool systemRestartRequest;
#if CFG_BOARD_ESP8266
extern ESP8266WebServer webServer;
#elif CFG_BOARD_ESP32
extern WebServer webServer;
#endif

/*===================================================================================================================*/
/* the page                                                                                                 */
/*===================================================================================================================*/
void htmlStreamPage(void);  /* stream the whole status and configuration page to the waiting web server client */

#endif  // WEBSITE_H_
