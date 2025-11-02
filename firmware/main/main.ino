#include <Arduino.h>
#include "led.h"
#include "sensor.h"
#include "local_network.h"

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

// Wifi credentials
const char* ssid = "your_ssid";
const char* password = "your_password";

void setup() {
  Serial.begin(115200);

  setupWiFi(ssid, password);

  pinMode(BTN_PIN, INPUT_PULLDOWN);
  setupLED(LED_PIN);

  sensorInit(DHT_PIN);

  Serial.println("Init OK (anti-rebond + DHT11 sur GPIO 26)");
}

void loop() {
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
      Serial.print("DHT -> T: "); Serial.print(th.t, 1);
      Serial.print(" °C | H: ");   Serial.print(th.h, 1);
      Serial.println(" %");
    } else {
      Serial.println("DHT -> invalid reading");
    }
  }

  if (pressedEvent) {
    Serial.println("Button pressed!");
  }
}
