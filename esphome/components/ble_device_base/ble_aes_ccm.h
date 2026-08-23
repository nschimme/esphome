#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::ble_device_base {

void aes128_encrypt_block(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

bool aes_ccm_auth_decrypt(const uint8_t key[16], const uint8_t *nonce, size_t nonce_len, const uint8_t *aad,
                          size_t aad_len, const uint8_t *ciphertext, size_t ct_len, uint8_t *plaintext,
                          const uint8_t *tag, size_t tag_len);

bool aes_ccm_auth_encrypt(const uint8_t key[16], const uint8_t *nonce, size_t nonce_len, const uint8_t *aad,
                          size_t aad_len, const uint8_t *plaintext, size_t pt_len, uint8_t *ciphertext, uint8_t *tag,
                          size_t tag_len);

}  // namespace esphome::ble_device_base
