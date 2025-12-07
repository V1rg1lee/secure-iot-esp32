#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "oled.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static bool ready = false;

bool oledInit() {
  Wire.begin();  // On ESP32: SDA=21, SCL=22 by default

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 init FAILED"));
    ready = false;
    return false;
  }

  ready = true;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Secure IoT ESP32"));
  display.println(F("OLED OK"));
  display.display();
  delay(1000);

  return true;
}

void oledShowMessage(const char* line1, const char* line2) {
  if (!ready) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2 && line2[0] != '\0') {
    display.println(line2);
  }
  display.display();
}

void oledShowTempHum(float temp, float hum, bool ok) {
  if (!ready) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(F("Capteur DHT11"));

  display.setCursor(0, 16);
  if (ok) {
    display.print(F("Temp: "));
    display.print(temp, 1);
    display.println(F(" C"));

    display.print(F("Hum : "));
    display.print(hum, 1);
    display.println(F(" %"));
  } else {
    display.println(F("Invalid reading"));
  }

  display.display();
}

void oledShowTempHumText(const char* tempStr, const char* humStr, bool ok) {
  if (!ready) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(F("DHT11 Sensor"));

  display.setCursor(0, 16);
  if (ok) {
    display.print(F("Temp: "));
    display.print(tempStr);
    display.println(F(" C"));

    display.print(F("Hum : "));
    display.print(humStr);
    display.println(F(" %"));
  } else {
    display.println(F("Invalid reading"));
  }

  display.display();
}

void oledShowTempHumWithSOS(const char* tempStr, const char* humStr, bool ok, bool localSOS, bool remoteSOS) {
  if (!ready) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(F("DHT11 Sensor"));

  display.setCursor(0, 16);
  if (ok) {
    display.print(F("Temp: "));
    display.print(tempStr);
    display.println(F(" C"));

    display.print(F("Hum : "));
    display.print(humStr);
    display.println(F(" %"));
  } else {
    display.println(F("Invalid reading"));
  }

  // Display SOS status at bottom
  display.setCursor(0, 48);
  if (localSOS) {
    display.setTextColor(SSD1306_WHITE);
    display.print(F(">>> SOS SENT <<<"));
  }
  if (remoteSOS) {
    if (localSOS) {
      display.setCursor(0, 56);
    }
    display.setTextColor(SSD1306_WHITE);
    display.print(F("!!! SOS ALERT !!!"));
  }

  display.display();
}

