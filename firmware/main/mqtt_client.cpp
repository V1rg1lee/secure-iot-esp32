#include <Arduino.h>
#include "mqtt_client.h"
#include "secure_mqtt.h"

#include <PubSubClient.h>
#include <string.h>

float g_remoteHumidity = 0.0f;
bool g_remoteHumidityValid = false;
float g_remoteTemperature = 0.0f;
bool g_remoteTemperatureValid = false;

static bool extractFloatField(const byte* payload,
                              unsigned int length,
                              const char* fieldName,
                              float* valueOut) {
  if (!payload || length == 0 || !fieldName || !valueOut) return false;

  // Copy into a small buffer to safely null-terminate before parsing.
  char buffer[256];
  unsigned int copyLen = (length < sizeof(buffer) - 1) ? length
                                                       : (sizeof(buffer) - 1);
  for (unsigned int i = 0; i < copyLen; ++i) {
    buffer[i] = static_cast<char>(payload[i]);
  }
  buffer[copyLen] = '\0';

  char key[32];
  snprintf(key, sizeof(key), "\"%s\":", fieldName);
  char* pos = strstr(buffer, key);
  if (!pos) return false;

  pos += strlen(key);
  while (*pos == ' ' || *pos == '\t') ++pos;
  *valueOut = atof(pos);
  return true;
}

extern PubSubClient client;
extern const char* mqttClientId;
extern const char* topic_cmd_sub;
extern const char* topic_data_sub;

void messageReceived(char* topic, byte* payload, unsigned int length) {

  // Check whether this is a KMS message for the secure client
  if (secureMqttHandleKmsMessage(topic,
                                 (const uint8_t*)payload,
                                 length,
                                 "iot/esp32",
                                 mqttClientId,
                                 client)) {
    Serial.println("[MQTT] Routed to KMS handler");
    return;
  }

  if (strcmp(topic, topic_data_sub) == 0) {
    // Attempt secure decrypt first
    char plain[256];
    bool decrypted = secureMqttDecryptPayload(payload, length, topic, plain, sizeof(plain));
    const byte* parsePayload = decrypted ? (const byte*)plain : payload;
    unsigned int parseLen = decrypted ? strlen(plain) : length;

    float value;
    bool parsed = false;
    if (extractFloatField(parsePayload, parseLen, "humidity", &value)) {
      g_remoteHumidity = value;
      g_remoteHumidityValid = true;
      parsed = true;
    }
    if (extractFloatField(parsePayload, parseLen, "temperature", &value)) {
      g_remoteTemperature = value;
      g_remoteTemperatureValid = true;
      parsed = true;
    }
  }


  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

extern PubSubClient client;

void reconnectMQTT(PubSubClient& client,
                   const char* clientId,
                   const char* commandTopicSub,
                   const char* dataTopicSub) {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (client.connect(clientId)) {

      // 1) Subscribe to the commands topic as before
      client.subscribe(commandTopicSub);
      // 2) Subscribe to the peer data topic
      client.subscribe(dataTopicSub);

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
