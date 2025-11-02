#pragma once

#include <PubSubClient.h>

void messageReceived(char* topic, byte* payload, unsigned int length);
void reconnectMQTT(PubSubClient& client, const char* clientId, const char* topicSub);
