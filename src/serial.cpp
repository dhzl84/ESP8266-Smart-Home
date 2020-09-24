//
// A simple server implementation showing how to:
//  * serve static messages
//  * read GET and POST parameters
//  * handle missing pages / 404s
//

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include "webserial.h"

AsyncWebServer server(81);

const char* PARAM_MESSAGE = "message";

void recvMsg(uint8_t *data, size_t len) {
  WebSerial.println("Received Data...");
  String d = "";
  for (uint32_t i = 0; i < len; i++) {
    d += static_cast<char>(data[i]);
  }
  WebSerial.println(d);
}

void WEBSERIAL_INIT(void) {
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);

  server.begin();
}

class webSerial {
 public:
  template <typename Type>
  void print(Type x) {
    WebSerial.print(x);
  }

  template <typename Type>
  void println(Type x) {
    WebSerial.println(x);
  }
};