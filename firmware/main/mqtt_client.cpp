#include "mqtt_client.h"
#include "secure_mqtt.h"

#include <PubSubClient.h>

extern PubSubClient client;
extern const char* mqttClientId;
extern const char* topic_sub;

void messageReceived(char* topic, byte* payload, unsigned int length) {
  // Check whether this is a KMS message for the secure client
  if (secureMqttHandleKmsMessage(topic,
                                 (const uint8_t*)payload,
                                 length,
                                 "iot/esp32",
                                 mqttClientId,
                                 client)) {
    // It was a KMS message, already handled
    return;
  }

  // Otherwise, normal behavior (commands)
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

extern PubSubClient client;

void reconnectMQTT(PubSubClient& client, const char* clientId, const char* topicSub) {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientId)) {
      Serial.println("connected");

      // 1) Subscribe to the commands topic as before
      client.subscribe(topicSub);

      // 2) Subscribe to the KMS topics for this client
      char kmsTopic[128];
      snprintf(kmsTopic, sizeof(kmsTopic),
               "iot/esp32/%s/kms/#", clientId);
      Serial.print("Subscribing to KMS topic: ");
      Serial.println(kmsTopic);
      client.subscribe(kmsTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
