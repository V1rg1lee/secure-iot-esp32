#include <Arduino.h>
#include "led.h"
#include "sensor.h"
#include "oled.h"
#include "local_network.h"
#include "mqtt_client.h"

#define LED_PIN 32
#define BTN_PIN 27
#define DHT_PIN 26

// Anti-rebond
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 30; // ms
int lastButtonReading = LOW;
int stableButtonState = LOW;
int lastStableButton   = LOW;

// Reading DHT11 periodically
unsigned long lastDht = 0;
const unsigned long dhtEveryMs = 2000; // ms

// Wifi config
const char* ssid = "your_ssid";
const char* password = "your_password";
WiFiClient espClient;

// MQTT client config
const char* mqttServer = "ip_address_of_your_broker"; // ip address printed by the broker
const int mqttPort = 8080;
const char* mqttClientId = "esp32_client";
const char* topic_pub = "iot/esp32/telemetry";
const char* topic_sub = "iot/esp32/commands";
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);

  setupWiFi(ssid, password);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(messageReceived);

  pinMode(BTN_PIN, INPUT_PULLDOWN);
  setupLED(LED_PIN);

  sensorInit(DHT_PIN);

   bool ok = oledInit();
  if (!ok) {
    Serial.println("OLED non detecte");
  } else {
    Serial.println("OLED initialise");
    oledShowMessage("Boot...", "DHT + Button OK");
  }

  Serial.println("Init OK (anti-rebond + DHT11 sur GPIO 26)");
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT(client, mqttClientId, topic_sub);
  }

  int reading = digitalRead(BTN_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    stableButtonState = reading;
  }

  bool pressedEvent = (stableButtonState == HIGH && lastStableButton == LOW);
  lastStableButton  = stableButtonState;
  lastButtonReading = reading;

  setLED(stableButtonState == HIGH);

  unsigned long now = millis();
  if (now - lastDht >= dhtEveryMs) {
    lastDht = now;
    TH th = sensorRead();
    if (th.ok) {
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"temperature\": %.1f, \"humidity\": %.1f}", th.t, th.h);
      Serial.print("Publishing MQTT message: ");
      Serial.println(payload);
      client.publish(topic_pub, payload);
    } else {
      Serial.println("DHT -> invalid reading");
    }

    oledShowTempHum(th.t, th.h, th.ok);
  }

  if (pressedEvent) {
    Serial.println("Button pressed!");
  }
}
