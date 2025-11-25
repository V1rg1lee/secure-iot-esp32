#include <Arduino.h>
#include "mqtt_client.h"
#include "secure_mqtt.h"

#include <PubSubClient.h>
#include <string.h>

float g_remoteHumidity = 0.0f;
bool g_remoteHumidityValid = false;
float g_remoteTemperature = 0.0f;
bool g_remoteTemperatureValid = false;
unsigned long g_remoteHumidityLastMs = 0;
unsigned long g_remoteTemperatureLastMs = 0;

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
      g_remoteHumidityLastMs = millis();
      parsed = true;
    }
    if (extractFloatField(parsePayload, parseLen, "temperature", &value)) {
      g_remoteTemperature = value;
      g_remoteTemperatureValid = true;
      g_remoteTemperatureLastMs = millis();
      parsed = true;
    }
  }


  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // If decryption failed due to tag/auth failure, request current TOPIC_key from KMS.
  // Throttle requests to avoid spamming the KMS (5s).
  if (secureMqttConsumeDecryptFailure()) {
    static unsigned long lastRekeyRequestMs = 0;
    unsigned long now = millis();
    if (now - lastRekeyRequestMs > 5000) { // if more than 5s since last request
      lastRekeyRequestMs = now;
      // publish a simple request: {"topic":"<expectedTopic>"}
      char reqTopic[128];
      snprintf(reqTopic, sizeof(reqTopic), "%s/%s/kms/request_key", "iot/esp32", mqttClientId);
      char body[128];
      // expected app topic is topic_data_sub (we used that as expectedTopic)
      snprintf(body, sizeof(body), "{\"topic\":\"%s\"}", topic);
      Serial.print("[MQTT] Decrypt tag failure -> requesting key from KMS on ");
      Serial.println(reqTopic);
      client.publish(reqTopic, body);
    }
  }
}

extern PubSubClient client;
extern void handleResetButton();

void reconnectMQTT(PubSubClient& client,
                   const char* clientId,
                   const char* commandTopicSub,
                   const char* dataTopicSub) {
  while (!client.connected()) {
    handleResetButton();

    if (client.connect(clientId)) {
      client.subscribe(commandTopicSub);
      client.subscribe(dataTopicSub);

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

      unsigned long start = millis();
      while (millis() - start < 5000) {
        handleResetButton();
        delay(100);
      }
    }
  }
}