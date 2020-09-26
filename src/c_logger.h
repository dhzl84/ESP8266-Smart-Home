#ifndef C_LOGGER_H_
#define C_LOGGER_H_

#include "Arduino.h"
#include "MQTTClient.h"
#include "c_mqtt.h"

typedef enum {
  serial = 0,
  mqtt = 1
} log_t;

class logger {
 public:
  logger(MQTTClient* client, mqttHelper* helper);
  ~logger(void);
  void print(String text);
  void println(String text);
  void set_logger(log_t logger, bool state);
  bool get_logger(log_t logger);

 private:
  bool log_to_serial_;
  bool log_to_mqtt_;
  MQTTClient* mqtt_client_;
  mqttHelper* mqtt_helper_;
};

#endif  /* C_LOGGER_H_ */
