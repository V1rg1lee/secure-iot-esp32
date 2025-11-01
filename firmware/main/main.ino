#include "led.h"

#define LED_PIN 32
#define BTN_PIN 27

void setup() {
  pinMode(BTN_PIN, INPUT_PULLDOWN);
  setupLED(LED_PIN);
}

void loop() {
  int btnState = digitalRead(BTN_PIN);
  setLED(btnState == HIGH);
}
