#include "local_network.h"

extern void handleButtonInput();

void setupWiFi(const char* ssid, const char* password) {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  const unsigned long timeoutMs = 15000;

  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < timeoutMs) {

    Serial.print(".");
    handleButtonInput();
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connect timeout, continuing without network.");
  }
}
