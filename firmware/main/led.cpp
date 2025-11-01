#include <Arduino.h>
#include "led.h"

static int ledPin = -1;

void setupLED(int pin) {
  ledPin = pin;
  pinMode(ledPin, OUTPUT);
}

void setLED(bool on) {
  digitalWrite(ledPin, on ? HIGH : LOW);
}
