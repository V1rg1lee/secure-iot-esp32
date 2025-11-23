#include <Arduino.h>
#include "led.h"

static int ledPin = -1;
static int resetLedPin = -1;

void setupLED(int pin) {
  ledPin = pin;
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
}

void setLED(bool on) {
  if (ledPin >= 0) {
    digitalWrite(ledPin, on ? HIGH : LOW);
  }
}

void setupResetLED(int pin) {
  resetLedPin = pin;
  pinMode(resetLedPin, OUTPUT);
  digitalWrite(resetLedPin, LOW);
}

void setResetLED(bool on) {
  if (resetLedPin >= 0) {
    digitalWrite(resetLedPin, on ? HIGH : LOW);
  }
}
