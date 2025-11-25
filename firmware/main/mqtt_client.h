#pragma once

#include <PubSubClient.h>

// Updated by MQTT message parsing when a metric is received from the other node.
extern float g_remoteHumidity;
extern bool g_remoteHumidityValid;
extern float g_remoteTemperature;
extern bool g_remoteTemperatureValid;
// Timestamps (millis) when the last remote value was received.
extern unsigned long g_remoteHumidityLastMs;
extern unsigned long g_remoteTemperatureLastMs;

void messageReceived(char* topic, byte* payload, unsigned int length);
void reconnectMQTT(PubSubClient& client,
                   const char* clientId,
                   const char* commandTopicSub,
                   const char* dataTopicSub);
