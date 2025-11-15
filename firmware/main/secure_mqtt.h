#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

// TO ADJUST: 32-byte CLIENT_MASTER_KEY provisioned (copied from your Python simulation)
extern const uint8_t CLIENT_MASTER_KEY[32];

// Full name of the secure topic (e.g., "iot/esp32/telemetry")
void secureMqttSetTopic(const char* topic_name);

// Starts the KMS handshake for this client/topic.
void secureMqttBeginHandshake(PubSubClient& client,
                              const char* baseTopic,
                              const char* clientId);

// Must be called from the MQTT callback to handle KMS messages.
// Returns true if the message was for the KMS and has been handled.
bool secureMqttHandleKmsMessage(const char* topic,
                                const uint8_t* payload,
                                unsigned int length,
                                const char* baseTopic,
                                const char* clientId,
                                PubSubClient& client);

// Returns true when TOPIC_key is ready
bool secureMqttIsReady();

// Encrypts a payload and publishes it to appTopic (e.g., "iot/esp32/telemetry")
bool secureMqttEncryptAndPublish(PubSubClient& client,
                                 const char* appTopic,
                                 const uint8_t* plaintext,
                                 size_t plaintextLen);
