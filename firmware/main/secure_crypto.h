#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Generates `len` random bytes into `buf`
void sc_random_bytes(uint8_t* buf, size_t len);

// HKDF-SHA256
bool sc_hkdf_sha256(const uint8_t* ikm, size_t ikm_len,
                    const uint8_t* salt, size_t salt_len,
                    const uint8_t* info, size_t info_len,
                    uint8_t* okm, size_t okm_len);

// HMAC-SHA256 (out_len <= 32)
bool sc_hmac_sha256(const uint8_t* key, size_t key_len,
                    const uint8_t* data, size_t data_len,
                    uint8_t* out, size_t out_len);

// AES-256-GCM
bool sc_aes_gcm_encrypt(const uint8_t* key, size_t key_len,
                        const uint8_t* iv, size_t iv_len,
                        const uint8_t* aad, size_t aad_len,
                        const uint8_t* input, size_t in_len,
                        uint8_t* output,
                        uint8_t* tag, size_t tag_len);

bool sc_aes_gcm_decrypt(const uint8_t* key, size_t key_len,
                        const uint8_t* iv, size_t iv_len,
                        const uint8_t* aad, size_t aad_len,
                        const uint8_t* input, size_t in_len,
                        const uint8_t* tag, size_t tag_len,
                        uint8_t* output);
