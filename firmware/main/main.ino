#include <Arduino.h>
#include "led.h"
#include "sensor.h"
#include "oled.h"
#include "local_network.h"
#include "mqtt_client.h"
#include "secure_mqtt.h" 

#define LED_PIN 32
#define BTN_PIN 27
#define DHT_PIN 26

// WiFi credentials
#define SSID "your_ssid"
#define PASSWORD "your_password"
#define MQQTSERVER "ip" //put your computer's inet address here

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
const char* ssid = SSID;
const char* password = PASSWORD;
WiFiClient espClient;
PubSubClient client(espClient);

// Role selection: set to 1 for temperature publisher, 0 for humidity publisher.
#define IS_TEMPERATURE_NODE 1

const char* mqttClientId = IS_TEMPERATURE_NODE ? "esp32_temp_client"
                                               : "esp32_hum_client";

// MQTT client config
const char* mqttServer = MQQTSERVER; // ip address printed by the broker
const int mqttPort = 1883;
const char* topic_pub      = "iot/esp32/data";
const char* topic_data_sub = "iot/esp32/data";
const char* topic_cmd_sub = "iot/esp32/commands";

void setup() {
  Serial.begin(115200);

  setupWiFi(ssid, password);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(messageReceived);
  client.setBufferSize(1024);

  // Secure MQTT: set the topic name
  secureMqttSetTopic(topic_pub);

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
    reconnectMQTT(client, mqttClientId, topic_cmd_sub, topic_data_sub);

    // Once reconnected, start the secure handshake
    secureMqttBeginHandshake(client, "iot/esp32", mqttClientId);
  }

  client.loop(); // IMPORTANT to process MQTT messages

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

    float temperatureToSend = th.t;
    float humidityToSend = th.h;

    if (th.ok) {
      char payload[64];
      if (IS_TEMPERATURE_NODE) {
        snprintf(payload, sizeof(payload), "{\"temperature\": %.1f}", temperatureToSend);
      } else {
        snprintf(payload, sizeof(payload), "{\"humidity\": %.1f}", humidityToSend);
      }

      if (secureMqttIsReady()) {
        Serial.print("Publishing SECURE MQTT message to ");
        Serial.print(topic_pub);
        Serial.print(": ");
        Serial.println(payload);
        secureMqttEncryptAndPublish(client,
                                    topic_pub,
                                    (const uint8_t*)payload,
                                    strlen(payload));
      } else {
        Serial.println("TOPIC_key not ready, skipping secure publish");
      }
    } else {
      Serial.println("DHT -> invalid reading");
    }

    // Display rules:
    // - Temperature node: show local temperature; show humidity only if received from server, else "?".
    // - Humidity node: show local humidity; show temperature only if received from server, else "?".
    char tempStr[8];
    char humStr[8];

    if (IS_TEMPERATURE_NODE) {
      if (th.ok) {
        snprintf(tempStr, sizeof(tempStr), "%.1f", temperatureToSend);
      } else {
        snprintf(tempStr, sizeof(tempStr), "?");
      }
      if (g_remoteHumidityValid) {
        snprintf(humStr, sizeof(humStr), "%.1f", g_remoteHumidity);
      } else {
        snprintf(humStr, sizeof(humStr), "?");
      }
    } else {
      if (g_remoteTemperatureValid) {
        snprintf(tempStr, sizeof(tempStr), "%.1f", g_remoteTemperature);
      } else {
        snprintf(tempStr, sizeof(tempStr), "?");
      }
      if (th.ok) {
        snprintf(humStr, sizeof(humStr), "%.1f", humidityToSend);
      } else {
        snprintf(humStr, sizeof(humStr), "?");
      }
    }

    bool displayOk = (IS_TEMPERATURE_NODE ? th.ok : g_remoteTemperatureValid) ||
                     (!IS_TEMPERATURE_NODE ? th.ok : g_remoteHumidityValid);

    oledShowTempHumText(tempStr, humStr, displayOk);
  }

  if (pressedEvent) {
    Serial.println("Button pressed!");
  }
}
