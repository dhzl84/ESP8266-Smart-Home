#include "c_logger.h"
#include "c_mqtt.h"

logger::logger(MQTTClient* client, mqttHelper* helper)
  : log_to_serial_(true), \
    log_to_mqtt_(true) {
  mqtt_client_ = client;  /* copy pointer to object */
  mqtt_helper_ = helper;  /* copy pointer to object */
}

logger::~logger() {}

void logger::print(String text) {
  if (log_to_serial_ == true) {
    Serial.println(text);
  }
  if (log_to_mqtt_ == true) {
    mqtt_client_->publish(mqtt_helper_->getTopicLog(), text, false, MQTT_QOS);
  }
}
