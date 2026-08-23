#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::ble_device_base {

// Self-contained AES-128-CCM authenticated encryption and decryption (RFC 3610).
//
// Encrypted BLE advertisements (BTHome, Xiaomi/ATC) and Bluetooth SIG Mesh use
// AES-128-CCM. The platform crypto that provides it is inconsistent across BLE
// targets: ESP-IDF exposes PSA/mbedtls, but a LibreTiny SDK or Arduino framework
// may keep its mbedtls internal, so a component cannot rely on <mbedtls/ccm.h>
// being available on all platforms. This software implementation makes AES-CCM
// operations work on every BLE platform without a per-chip crypto dependency.
//
// Verifies the CCM authentication tag and, on success, writes `ct_len` decrypted
// bytes to `plaintext` and returns true. Returns false when authentication fails
// (the caller must then discard `plaintext`).
bool aes_ccm_auth_decrypt(const uint8_t key[16], const uint8_t *nonce, size_t nonce_len, const uint8_t *aad,
                          size_t aad_len, const uint8_t *ciphertext, size_t ct_len, uint8_t *plaintext,
                          const uint8_t *tag, size_t tag_len);

// Encrypts `plaintext` of length `pt_len` into `ciphertext` and generates an authentication tag
// of length `tag_len` using AES-128-CCM.
bool aes_ccm_auth_encrypt(const uint8_t key[16], const uint8_t *nonce, size_t nonce_len, const uint8_t *aad,
                          size_t aad_len, const uint8_t *plaintext, size_t pt_len, uint8_t *ciphertext, uint8_t *tag,
                          size_t tag_len);

/// AES-128 single-block encrypt (the same software cipher CCM uses). Used by
/// ESPBTDevice::resolve_irk() for the Bluetooth "ah" RPA hash, so IRK matching
/// works identically on every platform with no chip crypto dependency.
void aes128_encrypt_block(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

}  // namespace esphome::ble_device_base
