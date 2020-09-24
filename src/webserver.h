#ifndef WEBSERVER_H_
#define WEBSERVER_H_

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer webServer(80);
void notFound(AsyncWebwebServerRequest *request);

#endif  /* WEBSERVER_H_ */
