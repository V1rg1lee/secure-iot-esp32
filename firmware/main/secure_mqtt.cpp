#include "secure_mqtt.h"

#include "secure_crypto.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// ========= PROTOCOL CONFIG =========

static char g_topicName[64] = {0};
static uint8_t g_topicKey[32];
static bool g_topicKeyReady = false;
static uint32_t g_counter = 0;

static uint8_t g_lastChallenge[32];
static bool g_haveChallenge = false;

// TO BE REPLACED: given by the KMS after launch 
const uint8_t CLIENT_MASTER_KEY[32] = {
  0x15,
  0x13,
  0x3a,
  0x90,
  0x5e,
  0x6d,
  0x8a,
  0x21,
  0xb8,
  0xb7,
  0x14,
  0xdd,
  0xae,
  0x94,
  0xb0,
  0x87,
  0x57,
  0x3b,
  0xf5,
  0xf6,
  0x4f,
  0x98,
  0x12,
  0x99,
  0x1e,
  0xf1,
  0xca,
  0x42,
  0x01,
  0x91,
  0x5c,
  0x44
};


// Hex helpers
static void bytesToHex(const uint8_t* in, size_t len, char* out, size_t outSize) {
  const char* hex = "0123456789abcdef";
  if (outSize < 2 * len + 1) return;
  for (size_t i = 0; i < len; ++i) {
    out[2*i]   = hex[in[i] >> 4];
    out[2*i+1] = hex[in[i] & 0x0F];
  }
  out[2*len] = '\0';
}

static size_t hexToBytes(const char* hex, uint8_t* out, size_t maxLen) {
  size_t hexLen = strlen(hex);
  size_t outLen = hexLen / 2;
  if (outLen > maxLen) outLen = maxLen;
  for (size_t i = 0; i < outLen; ++i) {
    char c1 = hex[2*i];
    char c2 = hex[2*i+1];
    auto val = [](char c)->uint8_t {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return 0;
    };
    out[i] = (val(c1) << 4) | val(c2);
  }
  return outLen;
}

// HKDF to derive TOPIC_auth_key and TOPIC_key_enc_key
static void deriveTopicKeys(const char* topic,
                            uint8_t* topicAuthKey,
                            uint8_t* topicEncKey) {
  uint8_t material[64];
  const uint8_t salt[1] = {0};
  sc_hkdf_sha256(CLIENT_MASTER_KEY, sizeof(CLIENT_MASTER_KEY),
                 salt, sizeof(salt),
                 (const uint8_t*)topic, strlen(topic),
                 material, sizeof(material));
  memcpy(topicAuthKey, material, 32);
  memcpy(topicEncKey, material + 32, 32);
}

// ========= API =========

void secureMqttSetTopic(const char* topic_name) {
  strncpy(g_topicName, topic_name, sizeof(g_topicName)-1);
  g_topicName[sizeof(g_topicName)-1] = '\0';
}

void secureMqttBeginHandshake(PubSubClient& client,
                              const char* baseTopic,
                              const char* clientId) {
  if (g_topicName[0] == '\0') {
    Serial.println("[SEC] Topic name not set!");
    return;
  }

  // Generate client challenge
  sc_random_bytes(g_lastChallenge, sizeof(g_lastChallenge));
  g_haveChallenge = true;

  char challHex[32*2+1];
  bytesToHex(g_lastChallenge, sizeof(g_lastChallenge), challHex, sizeof(challHex));

  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"challenge\":\"%s\"}", challHex);

  char authTopic[128];
  snprintf(authTopic, sizeof(authTopic),
           "%s/%s/kms/auth", baseTopic, clientId);

  Serial.print("[SEC] Sending auth to ");
  Serial.println(authTopic);
  client.publish(authTopic, payload);
}

bool secureMqttIsReady() {
  return g_topicKeyReady;
}

// Very simple parser for a string field in a tiny JSON: "field":"value"
static bool extractJsonStringField(const char* json, const char* key,
                                   char* out, size_t outSize) {
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char* p = strstr(json, pattern);
  if (!p) return false;
  p += strlen(pattern);
  const char* end = strchr(p, '"');
  if (!end) return false;
  size_t len = (size_t)(end - p);
  if (len >= outSize) len = outSize - 1;
  memcpy(out, p, len);
  out[len] = '\0';
  return true;
}

// Same for an integer "field":123
static bool extractJsonIntField(const char* json, const char* key, int* value) {
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* p = strstr(json, pattern);
  if (!p) return false;
  p += strlen(pattern);
  *value = atoi(p);
  return true;
}

static void handleClientAuth(const char* json,
                             const char* baseTopic,
                             const char* clientId,
                             PubSubClient& client) {
  if (!g_haveChallenge) {
    Serial.println("[SEC] No stored challenge, ignoring clientauth");
    return;
  }

  char challHex[32*2+1];
  char nonceHex[32*2+1];

  if (!extractJsonStringField(json, "challenge", challHex, sizeof(challHex))) {
    Serial.println("[SEC] challenge missing in clientauth");
    return;
  }
  if (!extractJsonStringField(json, "nonce_k", nonceHex, sizeof(nonceHex))) {
    Serial.println("[SEC] nonce_k missing in clientauth");
    return;
  }

  uint8_t challRecv[32];
  hexToBytes(challHex, challRecv, sizeof(challRecv));
  if (memcmp(challRecv, g_lastChallenge, sizeof(g_lastChallenge)) != 0) {
    Serial.println("[SEC] Challenge mismatch, aborting");
    return;
  }

  // TODO: extract "signature" and verify RSA with the KMS public key.
  Serial.println("[SEC] KMS authenticated (signature NOT YET VERIFIED!).");

  uint8_t nonceK[32];
  hexToBytes(nonceHex, nonceK, sizeof(nonceK));

  // HMAC(nonce_k) with TOPIC_auth_key
  uint8_t topicAuthKey[32];
  uint8_t topicEncKey[32];
  deriveTopicKeys(g_topicName, topicAuthKey, topicEncKey);

  uint8_t hmacVal[32];
  sc_hmac_sha256(topicAuthKey, sizeof(topicAuthKey),
                 nonceK, sizeof(nonceK),
                 hmacVal, sizeof(hmacVal));

  char hmacHex[32*2+1];
  bytesToHex(hmacVal, sizeof(hmacVal), hmacHex, sizeof(hmacHex));

  // send clientverify back
  char payload[512];
  snprintf(payload, sizeof(payload),
           "{\"topic\":\"%s\",\"nonce_k\":\"%s\",\"hmac\":\"%s\"}",
           g_topicName, nonceHex, hmacHex);

  char verifyTopic[128];
  snprintf(verifyTopic, sizeof(verifyTopic),
           "%s/%s/kms/clientverify", baseTopic, clientId);

  Serial.print("[SEC] Sending clientverify to ");
  Serial.println(verifyTopic);
  client.publish(verifyTopic, payload);
}

static void handleKeyMessage(const char* json) {
  char topicBuf[64];
  char ivHex[12*2+1];
  char ctHex[128*2+1];   // large buffer
  char tagHex[16*2+1];

  if (!extractJsonStringField(json, "topic", topicBuf, sizeof(topicBuf))) {
    Serial.println("[SEC] key.topic missing");
    return;
  }
  if (strcmp(topicBuf, g_topicName) != 0) {
    Serial.println("[SEC] key for different topic, ignoring");
    return;
  }

  if (!extractJsonStringField(json, "iv", ivHex, sizeof(ivHex))) {
    Serial.println("[SEC] key.iv missing");
    return;
  }
  if (!extractJsonStringField(json, "ciphertext", ctHex, sizeof(ctHex))) {
    Serial.println("[SEC] key.ciphertext missing");
    return;
  }
  if (!extractJsonStringField(json, "tag", tagHex, sizeof(tagHex))) {
    Serial.println("[SEC] key.tag missing");
    return;
  }

  uint8_t iv[12];
  uint8_t ciphertext[64]; // max size of the encrypted TOPIC_key (32 bytes)
  uint8_t tag[16];

  size_t ivLen = hexToBytes(ivHex, iv, sizeof(iv));
  size_t ctLen = hexToBytes(ctHex, ciphertext, sizeof(ciphertext));
  hexToBytes(tagHex, tag, sizeof(tag));

  uint8_t topicAuthKey[32];
  uint8_t topicEncKey[32];
  deriveTopicKeys(g_topicName, topicAuthKey, topicEncKey);

  uint8_t plain[32];
  bool ok = sc_aes_gcm_decrypt(topicEncKey, sizeof(topicEncKey),
                               iv, ivLen,
                               (const uint8_t*)"KMS_TOPIC_KEY", strlen("KMS_TOPIC_KEY"),
                               ciphertext, ctLen,
                               tag, sizeof(tag),
                               plain);
  if (!ok) {
    Serial.println("[SEC] Failed to decrypt TOPIC_key");
    return;
  }

  memcpy(g_topicKey, plain, 32);
  g_topicKeyReady = true;
  g_counter = 0;
  Serial.println("[SEC] TOPIC_key received and stored.");
}

bool secureMqttHandleKmsMessage(const char* topic,
                                const uint8_t* payload,
                                unsigned int length,
                                const char* baseTopic,
                                const char* clientId,
                                PubSubClient& client) {
  // Expecting: baseTopic/clientId/kms/xxx
  char prefix[128];
  snprintf(prefix, sizeof(prefix), "%s/%s/kms/", baseTopic, clientId);
  size_t prefixLen = strlen(prefix);

  if (strncmp(topic, prefix, prefixLen) != 0) {
    return false; // not for us
  }

  const char* action = topic + prefixLen; // "clientauth" or "key"
  // payload -> JSON string
  static char jsonBuf[1024];
  size_t copyLen = (length < sizeof(jsonBuf)-1) ? length : (sizeof(jsonBuf)-1);
  memcpy(jsonBuf, payload, copyLen);
  jsonBuf[copyLen] = '\0';

  if (strcmp(action, "clientauth") == 0) {
    handleClientAuth(jsonBuf, baseTopic, clientId, client);
  } else if (strcmp(action, "key") == 0) {
    handleKeyMessage(jsonBuf);
  } else {
    // other KMS message
  }

  return true;
}

bool secureMqttEncryptAndPublish(PubSubClient& client,
                                 const char* appTopic,
                                 const uint8_t* plaintext,
                                 size_t plaintextLen) {
  if (!g_topicKeyReady) {
    Serial.println("[SEC] Cannot publish, TOPIC_key not ready");
    return false;
  }

  uint8_t iv[12];
  sc_random_bytes(iv, sizeof(iv));

  uint8_t counterBytes[4];
  counterBytes[0] = (g_counter >> 24) & 0xFF;
  counterBytes[1] = (g_counter >> 16) & 0xFF;
  counterBytes[2] = (g_counter >> 8)  & 0xFF;
  counterBytes[3] = (g_counter)       & 0xFF;

  // AAD = counter || topic_name
  uint8_t aad[4 + 64];
  size_t aadLen = 4 + strlen(g_topicName);
  memcpy(aad, counterBytes, 4);
  memcpy(aad+4, g_topicName, strlen(g_topicName));

  // AES_key = HKDF(TOPIC_key, salt = iv||counter)
  uint8_t salt[12+4];
  memcpy(salt, iv, 12);
  memcpy(salt+12, counterBytes, 4);
  uint8_t aesKey[32];
  sc_hkdf_sha256(g_topicKey, sizeof(g_topicKey),
                 salt, sizeof(salt),
                 (const uint8_t*)g_topicName, strlen(g_topicName),
                 aesKey, sizeof(aesKey));

  uint8_t ciphertext[256];
  uint8_t tag[16];
  if (plaintextLen > sizeof(ciphertext)) {
    Serial.println("[SEC] Plaintext too large");
    return false;
  }

  bool ok = sc_aes_gcm_encrypt(aesKey, sizeof(aesKey),
                               iv, sizeof(iv),
                               aad, aadLen,
                               plaintext, plaintextLen,
                               ciphertext,
                               tag, sizeof(tag));
  if (!ok) {
    Serial.println("[SEC] AES-GCM encrypt failed");
    return false;
  }

  // Build JSON
  char ivHex[12*2+1];
  char ctHex[256*2+1];
  char tagHex[16*2+1];

  bytesToHex(iv, sizeof(iv), ivHex, sizeof(ivHex));
  bytesToHex(ciphertext, plaintextLen, ctHex, sizeof(ctHex));
  bytesToHex(tag, sizeof(tag), tagHex, sizeof(tagHex));

  char payload[1024];
  snprintf(payload, sizeof(payload),
           "{\"iv\":\"%s\",\"counter\":%lu,\"ciphertext\":\"%s\",\"tag\":\"%s\",\"topic_name\":\"%s\"}",
           ivHex, (unsigned long)g_counter, ctHex, tagHex, g_topicName);

  g_counter++;

  return client.publish(appTopic, payload);
}
