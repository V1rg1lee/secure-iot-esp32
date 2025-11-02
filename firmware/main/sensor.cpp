#include <Arduino.h>
#include <DHT.h>
#include "sensor.h"

static DHT* dht = nullptr;

void sensorInit(uint8_t dhtPin) {
  if (dht) delete dht;
  dht = new DHT(dhtPin, DHT11);
  dht->begin();
}

TH sensorRead() {
  TH r{};
  if (!dht) { r.ok = false; return r; }
  r.h = dht->readHumidity();
  r.t = dht->readTemperature();
  r.ok = !isnan(r.h) && !isnan(r.t);
  return r;
}
