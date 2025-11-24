#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "led.h"
#include "sensor.h"
#include "oled.h"
#include "local_network.h"
#include "mqtt_client.h"
#include "secure_mqtt.h" 

Preferences prefs;

struct DeviceConfig {
  String wifi_ssid;
  String wifi_password;
  String mqtt_broker;
  int    mqtt_port;
  String client_id;
  bool   is_temp_node;
  uint8_t client_master_key[32];
  String kms_pubkey_pem;
};

DeviceConfig g_cfg;

#define LED_PIN 32
#define RESET_LED_PIN 33
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
WiFiClient espClient;
PubSubClient client(espClient);
bool IS_TEMPERATURE_NODE = true;
const char* mqttClientId = nullptr;
const char* mqttServer   = nullptr;
int mqttPort             = 1883;
const char* ssid         = nullptr;
const char* password     = nullptr;
const char* topic_pub      = "iot/esp32/data";
const char* topic_data_sub = "iot/esp32/data";
const char* topic_cmd_sub = "iot/esp32/commands";

static unsigned long resetPressStart = 0;

void handleResetButton() {
  int rawBtn = digitalRead(BTN_PIN);

  if (rawBtn == HIGH) {
    if (resetPressStart == 0) {
      resetPressStart = millis();
    }

    setLED(true);

    unsigned long held = millis() - resetPressStart;
    if (held > 5000) { // > 5s -> trigger reset
      setResetLED(true);
      oledShowMessage("Reset config", "Reboot...");
      delay(300);
      setResetLED(false);
      setLED(false);

      clearConfig();
    }
  } else {
    resetPressStart = 0;
    setLED(false);
    setResetLED(false);
  }
}

bool loadConfig(DeviceConfig& cfg) {
  if (!prefs.begin("config", true)) { // read-only
    return false;
  }
  if (!prefs.isKey("ssid")) {
    prefs.end();
    return false;
  }

  cfg.wifi_ssid      = prefs.getString("ssid", "");
  cfg.wifi_password  = prefs.getString("wpass", "");
  cfg.mqtt_broker    = prefs.getString("broker", "");
  cfg.mqtt_port      = prefs.getInt("port", 1883);
  cfg.client_id      = prefs.getString("cid", "esp32_client");
  cfg.is_temp_node   = prefs.getBool("is_temp", true);

  size_t len = prefs.getBytesLength("cmk");
  if (len != 32) {
    prefs.end();
    return false;
  }
  prefs.getBytes("cmk", cfg.client_master_key, 32);

  cfg.kms_pubkey_pem = prefs.getString("kms_pub", "");

  prefs.end();
  return true;
}

bool saveConfig(const DeviceConfig& cfg) {
  if (!prefs.begin("config", false)) { // read-write
    return false;
  }
  prefs.putString("ssid",   cfg.wifi_ssid);
  prefs.putString("wpass",  cfg.wifi_password);
  prefs.putString("broker", cfg.mqtt_broker);
  prefs.putInt("port",      cfg.mqtt_port);
  prefs.putString("cid",    cfg.client_id);
  prefs.putBool("is_temp",  cfg.is_temp_node);
  prefs.putBytes("cmk",     cfg.client_master_key, 32);
  prefs.putString("kms_pub", cfg.kms_pubkey_pem);
  prefs.end();
  return true;
}

void clearConfig() {
  if (!prefs.begin("config", false)) {
    Serial.println("Failed to open NVS for clear");
    return;
  }
  prefs.clear();
  prefs.end();
  Serial.println("Config cleared. Rebooting...");
  delay(1000);
  ESP.restart();
}

bool hexToBytes(const char* hex, uint8_t* out, size_t outLen) {
  size_t hexLen = strlen(hex);
  if (hexLen != outLen * 2) return false;

  auto val = [](char c)->int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };

  for (size_t i = 0; i < outLen; ++i) {
    int v1 = val(hex[2*i]);
    int v2 = val(hex[2*i+1]);
    if (v1 < 0 || v2 < 0) return false;
    out[i] = (uint8_t)((v1 << 4) | v2);
  }
  return true;
}

void waitForProvisioning() {
  Serial.println("== PROVISIONING MODE ==");
  Serial.println("Send a configuration JSON over serial (end with \\n+).\"");
  Serial.println("Example:");
  Serial.println("{\"wifi_ssid\":\"...\",\"wifi_password\":\"...\","
                 "\"mqtt_broker\":\"192.168.3.170\",\"mqtt_port\":1883,"
                 "\"client_id\":\"esp32_temp_client\",\"is_temp_node\":1,"
                 "\"client_master_key\":\"0011...ff\","
                 "\"kms_pubkey_pem\":\"-----BEGIN PUBLIC KEY-----\\n...\"}");
  // Flush any leftover bytes in the serial buffer
  while (Serial.available()) {
    Serial.read();
  }

  // Read a line from Serial
  String line;
  Serial.println("Waiting for a JSON line...");
  while (line.length() == 0) {
    if (Serial.available()) {
      line = Serial.readStringUntil('\n');
      line.trim(); // remove possible '\r'
      break;
    }
    delay(10);
  }

  Serial.print("Received: ");
  Serial.println(line);

  if (!line.startsWith("{")) {
    Serial.println("Line doesn't start with '{' -> not JSON, aborting.");
    return;
  }

  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  DeviceConfig cfg;
  cfg.wifi_ssid     = doc["wifi_ssid"]     | "";
  cfg.wifi_password = doc["wifi_password"] | "";
  cfg.mqtt_broker   = doc["mqtt_broker"]   | "";
  cfg.mqtt_port     = doc["mqtt_port"]     | 1883;
  cfg.client_id     = doc["client_id"]     | "esp32_client";
  cfg.is_temp_node  = (doc["is_temp_node"] | 1) != 0;

  const char* cmk_hex = doc["client_master_key"] | "";
  if (!hexToBytes(cmk_hex, cfg.client_master_key, 32)) {
    Serial.println("Invalid client_master_key hex");
    return;
  }

  const char* kms_pem = doc["kms_pubkey_pem"] | "";
  cfg.kms_pubkey_pem = String(kms_pem);

  if (!saveConfig(cfg)) {
    Serial.println("Failed to save config!");
    return;
  }

  Serial.println("Config saved. Rebooting...");
  delay(1000);
  ESP.restart();
}


void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!loadConfig(g_cfg)) { // no config yet
    waitForProvisioning();
    while (true) { delay(1000); }
  }

  ssid         = g_cfg.wifi_ssid.c_str();
  password     = g_cfg.wifi_password.c_str();
  mqttServer   = g_cfg.mqtt_broker.c_str();
  mqttPort     = g_cfg.mqtt_port;
  mqttClientId = g_cfg.client_id.c_str();
  IS_TEMPERATURE_NODE = g_cfg.is_temp_node;

  memcpy(CLIENT_MASTER_KEY, g_cfg.client_master_key, 32);

  secureMqttSetKmsPubkey(g_cfg.kms_pubkey_pem.c_str());

  Serial.println("Config chargee depuis NVS :");
  Serial.print("  SSID = "); Serial.println(ssid);
  Serial.print("  Broker = "); Serial.print(mqttServer);
  Serial.print(":"); Serial.println(mqttPort);
  Serial.print("  ClientID = "); Serial.println(mqttClientId);
  Serial.print("  Role = "); Serial.println(IS_TEMPERATURE_NODE ? "TEMP" : "HUM");

  pinMode(BTN_PIN, INPUT_PULLDOWN);
  setupLED(LED_PIN);
  setupResetLED(RESET_LED_PIN);

  sensorInit(DHT_PIN);

  bool ok = oledInit();
  if (!ok) {
    Serial.println("OLED not detected");
  } else {
    Serial.println("OLED initialized");
    oledShowMessage("Boot...", "DHT + Button OK");
  }

  setupWiFi(ssid, password);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(messageReceived);
  client.setBufferSize(1024);

  secureMqttSetTopic(topic_pub);
  secureMqttSetClientId(mqttClientId);
  secureMqttInit(topic_pub, mqttClientId);

  Serial.println("Init OK (debounce + DHT11 on GPIO 26)");
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT(client, mqttClientId, topic_cmd_sub, topic_data_sub);

    // Once reconnected, start the secure handshake
    secureMqttBeginHandshake(client, "iot/esp32", mqttClientId);
  }

  client.loop(); // IMPORTANT to process MQTT messages

  handleResetButton();

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
}
