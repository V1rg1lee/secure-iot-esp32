#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

void secureMqttInit(const char* topic_name, const char* client_id);

void secureMqttSetClientId(const char* client_id);

// TO ADJUST: 32-byte CLIENT_MASTER_KEY provisioned (copied from your Python simulation)
extern uint8_t CLIENT_MASTER_KEY[32];

// TO ADJUST: KMS public key in PEM format for signature verification
extern char KMS_PUBKEY_PEM[];

void secureMqttSetKmsPubkey(const char* pem);

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

// Decrypts an incoming secure payload for the expected topic into outBuffer.
// Returns true on success.
bool secureMqttDecryptPayload(const uint8_t* payload,
                              unsigned int length,
                              const char* expectedTopic,
                              char* outBuffer,
                              size_t outBufferSize);
