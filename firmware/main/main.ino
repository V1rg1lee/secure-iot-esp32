#include "led.h"

#define LED_PIN 32
#define BTN_PIN 27

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 30;

int lastButtonReading = LOW;
int stableButtonState = LOW;

void setup() {
  Serial.begin(115200);
  
  pinMode(BTN_PIN, INPUT_PULLDOWN);
  setupLED(LED_PIN);
}

void loop() {
  int reading = digitalRead(BTN_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    stableButtonState = reading;
  }
  
  setLED(stableButtonState == HIGH);

  lastButtonReading = reading;
}
