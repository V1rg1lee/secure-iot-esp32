#include "secure_crypto.h"

#include <string.h>
#include <algorithm>

extern "C" {
#include "esp_random.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#include "esp_system.h"
}

void sc_random_bytes(uint8_t* buf, size_t len) {
  size_t i = 0;
  while (i < len) {
    uint32_t r = esp_random();
    size_t chunk = std::min(len - i, (size_t)4);
    memcpy(buf + i, &r, chunk);
    i += chunk;
  }
}

bool sc_hkdf_sha256(const uint8_t* ikm, size_t ikm_len,
                    const uint8_t* salt, size_t salt_len,
                    const uint8_t* info, size_t info_len,
                    uint8_t* okm, size_t okm_len) {
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md) return false;
  int ret = mbedtls_hkdf(md, salt, salt_len, ikm, ikm_len, info, info_len, okm, okm_len);
  return ret == 0;
}

bool sc_hmac_sha256(const uint8_t* key, size_t key_len,
                    const uint8_t* data, size_t data_len,
                    uint8_t* out, size_t out_len) {
  if (out_len == 0 || out_len > 32) return false;
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md) return false;
  unsigned char full[32];
  int ret = mbedtls_md_hmac(md, key, key_len, data, data_len, full);
  if (ret != 0) return false;
  memcpy(out, full, out_len);
  return true;
}

bool sc_aes_gcm_encrypt(const uint8_t* key, size_t key_len,
                        const uint8_t* iv, size_t iv_len,
                        const uint8_t* aad, size_t aad_len,
                        const uint8_t* input, size_t in_len,
                        uint8_t* output,
                        uint8_t* tag, size_t tag_len) {
  if (key_len != 32 || tag_len != 16) return false;

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (ret != 0) {
    mbedtls_gcm_free(&ctx);
    return false;
  }

  ret = mbedtls_gcm_crypt_and_tag(&ctx,
                                  MBEDTLS_GCM_ENCRYPT,
                                  in_len,
                                  iv, iv_len,
                                  aad, aad_len,
                                  input, output,
                                  tag_len, tag);
  mbedtls_gcm_free(&ctx);
  return ret == 0;
}

bool sc_aes_gcm_decrypt(const uint8_t* key, size_t key_len,
                        const uint8_t* iv, size_t iv_len,
                        const uint8_t* aad, size_t aad_len,
                        const uint8_t* input, size_t in_len,
                        const uint8_t* tag, size_t tag_len,
                        uint8_t* output) {
  if (key_len != 32 || tag_len != 16) return false;

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (ret != 0) {
    mbedtls_gcm_free(&ctx);
    return false;
  }

  ret = mbedtls_gcm_auth_decrypt(&ctx,
                                 in_len,
                                 iv, iv_len,
                                 aad, aad_len,
                                 tag, tag_len,
                                 input, output);
  mbedtls_gcm_free(&ctx);
  return ret == 0;
}
