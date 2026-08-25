// crypto.cpp
//
// Cryptographic functions for Bluetooth SIG Mesh network security.

#include "crypto.h"

#ifdef USE_BLUETOOTH_SIG_MESH

#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace bluetooth_sig_mesh {

static const char *const TAG = "bluetooth_sig_mesh.crypto";

// ---------------------------------------------------------------------------
// Subkey Generation & CMAC Helpers
// ---------------------------------------------------------------------------

// Generates AES-CMAC subkeys K1 and K2 as specified in RFC 4493 Section 2.3.
// Subkeys K1 and K2 are derived by bit-shifting an AES-128 block encryption of zero
// and conditionally XORing with the constant R_b (0x00...87). These subkeys are XORed
// into the final message block to differentiate complete and padded incomplete blocks,
// preventing length extension and forgery attacks in AES-CMAC calculation.
static void generate_cmac_subkeys(const uint8_t key[16], uint8_t k1[16], uint8_t k2[16]) {
  static const uint8_t const_rb[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};
  uint8_t l[16] = {0};
  uint8_t zero[16] = {0};
  ble_device_base::aes128_encrypt_block(key, zero, l);

  bool carry = (l[0] & 0x80) != 0;
  for (int i = 0; i < 15; i++) {
    k1[i] = (l[i] << 1) | (l[i + 1] >> 7);
  }
  k1[15] = l[15] << 1;
  if (carry) {
    for (int i = 0; i < 16; i++) {
      k1[i] ^= const_rb[i];
    }
  }

  carry = (k1[0] & 0x80) != 0;
  for (int i = 0; i < 15; i++) {
    k2[i] = (k1[i] << 1) | (k1[i + 1] >> 7);
  }
  k2[15] = k1[15] << 1;
  if (carry) {
    for (int i = 0; i < 16; i++) {
      k2[i] ^= const_rb[i];
    }
  }
}

// ---------------------------------------------------------------------------
// Key Derivation Functions
// ---------------------------------------------------------------------------

// Derives 6-bit Application ID (AID) from AppKey per SIG Mesh Spec v1.0.1 Section 3.8.2.5.
// k4(N) = AES-CMAC_T( "id6" || 0x01 ) where T = AES-CMAC_smk4(N).
// The 6-bit AID allows lower transport frames to quickly identify matching AppKeys.
uint8_t BluetoothSIGMeshCrypto::mesh_k4(const uint8_t app_key[16]) {
  static const uint8_t salt_smk4[16] = {0x47, 0x14, 0xD4, 0xAA, 0xEB, 0x1F, 0xB6, 0xDF,
                                        0x10, 0xE9, 0xB4, 0x10, 0x14, 0x98, 0xBF, 0xA2};

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt_smk4, app_key, 16, t);

  static const uint8_t msg[4] = {'i', 'd', '6', 0x01};
  uint8_t out[16] = {0};
  mesh_aes_cmac(t, msg, sizeof(msg), out);
  return out[15] & 0x3F;
}

void BluetoothSIGMeshCrypto::mesh_aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]) {
  uint8_t k1[16] = {0};
  uint8_t k2[16] = {0};
  generate_cmac_subkeys(key, k1, k2);

  size_t n = (len + 15) / 16;
  bool flag = true;
  if (n == 0) {
    n = 1;
    flag = false;
  } else {
    flag = (len % 16 == 0);
  }

  uint8_t m_last[16] = {0};
  if (flag) {
    for (size_t i = 0; i < 16; i++) {
      m_last[i] = msg[(n - 1) * 16 + i] ^ k1[i];
    }
  } else {
    size_t last_block_len = len % 16;
    for (size_t i = 0; i < last_block_len; i++) {
      m_last[i] = msg[(n - 1) * 16 + i];
    }
    m_last[last_block_len] = 0x80;
    for (size_t i = last_block_len + 1; i < 16; i++) {
      m_last[i] = 0x00;
    }
    for (size_t i = 0; i < 16; i++) {
      m_last[i] ^= k2[i];
    }
  }

  uint8_t x[16] = {0};
  uint8_t y[16] = {0};

  for (size_t i = 0; i < n - 1; i++) {
    for (size_t j = 0; j < 16; j++) {
      y[j] = x[j] ^ msg[i * 16 + j];
    }
    ble_device_base::aes128_encrypt_block(key, y, x);
  }

  for (size_t j = 0; j < 16; j++) {
    y[j] = x[j] ^ m_last[j];
  }
  ble_device_base::aes128_encrypt_block(key, y, x);

  std::memcpy(out, x, 16);
}

void BluetoothSIGMeshCrypto::mesh_s1(const uint8_t *m, size_t len, uint8_t out[16]) {
  static const uint8_t zero_key[16] = {0};
  mesh_aes_cmac(zero_key, m, len, out);
}

void BluetoothSIGMeshCrypto::mesh_k1(const uint8_t n[16], const uint8_t *p, size_t p_len, uint8_t out[16]) {
  static const uint8_t salt_smk1[16] = {0x2E, 0xB1, 0x11, 0xAC, 0x2A, 0x48, 0x06, 0xA2,
                                        0xC7, 0xA3, 0xD7, 0xDF, 0xF9, 0x1A, 0xEB, 0x31};

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt_smk1, n, 16, t);
  mesh_aes_cmac(t, p, p_len, out);
}

// Derives NID (7 bits), Encryption Key (16 bytes), and Privacy Key (16 bytes) from NetKey
// per SIG Mesh Spec v1.0.1 Section 3.8.2.3.
// Uses salt smk2 and counter bytes 0x01, 0x02, 0x03 in HKDF-style AES-CMAC expansion to ensure
// distinct cryptographic separation between network message encryption and header obfuscation.
void BluetoothSIGMeshCrypto::mesh_k2(const uint8_t net_key[16], const uint8_t *p, size_t p_len, uint8_t *out_nid,
                                    uint8_t out_ek[16], uint8_t out_pk[16]) {
  static const uint8_t salt_smk2[16] = {0x33, 0x82, 0x56, 0x1B, 0xAD, 0x82, 0x10, 0x7E,
                                        0xAD, 0x3B, 0xF9, 0x7E, 0x8D, 0xA9, 0xC4, 0x6E};

  uint8_t t[16] = {0};
  mesh_aes_cmac(salt_smk2, net_key, 16, t);

  uint8_t buf[33] = {0};
  if (p_len > 0 && p != nullptr && p_len <= 16) {
    std::memcpy(buf, p, p_len);
  }
  buf[p_len] = 0x01;
  uint8_t t1[16] = {0};
  mesh_aes_cmac(t, buf, p_len + 1, t1);
  if (out_nid != nullptr) {
    *out_nid = t1[15] & 0x7F;
  }

  std::memcpy(buf, t1, 16);
  if (p_len > 0 && p != nullptr && p_len <= 16) {
    std::memcpy(buf + 16, p, p_len);
  }
  buf[16 + p_len] = 0x02;
  mesh_aes_cmac(t, buf, 16 + p_len + 1, out_ek);

  std::memcpy(buf, out_ek, 16);
  if (p_len > 0 && p != nullptr && p_len <= 16) {
    std::memcpy(buf + 16, p, p_len);
  }
  buf[16 + p_len] = 0x03;
  mesh_aes_cmac(t, buf, 16 + p_len + 1, out_pk);
}

// ---------------------------------------------------------------------------
// Privacy Header Obfuscation & AES-CCM Encryption/Decryption
// ---------------------------------------------------------------------------

// Applies/removes network header privacy obfuscation per SIG Mesh Spec v1.0.1 Section 3.8.7.
// Obfuscates 6 header bytes (CTL, TTL, SEQ [3 bytes], SRC [2 bytes]) using an AES-128 privacy
// mask derived from IV Index and Privacy Random (first 7 bytes of payload ciphertext).
// This hides node network topology and sequence numbers from passive over-the-air sniffers.
void BluetoothSIGMeshCrypto::obfuscate_header(const uint8_t privacy_key[16], uint32_t iv_index,
                                             const uint8_t privacy_random[7], uint8_t header_data[6]) {
  uint8_t privacy_block_in[16] = {0};
  privacy_block_in[0] = 0x00;
  privacy_block_in[1] = 0x00;
  privacy_block_in[2] = 0x00;
  privacy_block_in[3] = 0x00;
  privacy_block_in[4] = 0x00;
  privacy_block_in[5] = (iv_index >> 24) & 0xFF;
  privacy_block_in[6] = (iv_index >> 16) & 0xFF;
  privacy_block_in[7] = (iv_index >> 8) & 0xFF;
  privacy_block_in[8] = iv_index & 0xFF;
  std::memcpy(privacy_block_in + 9, privacy_random, 7);

  uint8_t privacy_block[16] = {0};
  ble_device_base::aes128_encrypt_block(privacy_key, privacy_block_in, privacy_block);

  for (size_t i = 0; i < 6; i++) {
    header_data[i] ^= privacy_block[i];
  }
}

bool BluetoothSIGMeshCrypto::decrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *ct,
                                                 size_t ct_len, uint8_t *pt, size_t mic_len) {
  if (ct_len < mic_len) {
    return false;
  }
  size_t payload_len = ct_len - mic_len;
  const uint8_t *tag = ct + payload_len;
  return ble_device_base::aes_ccm_auth_decrypt(key, nonce, 13, nullptr, 0, ct, payload_len, pt, tag, mic_len);
}

void BluetoothSIGMeshCrypto::encrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *pt,
                                                 size_t pt_len, uint8_t *ct, size_t mic_len) {
  uint8_t tag[8] = {0};
  ble_device_base::aes_ccm_auth_encrypt(key, nonce, 13, nullptr, 0, pt, pt_len, ct, tag, mic_len);
  std::memcpy(ct + pt_len, tag, mic_len);
}

bool BluetoothSIGMeshCrypto::parse_hex_key(const std::string &hex, MeshKey &out_key) {
  if (hex.length() != 32) {
    ESP_LOGE(TAG, "Invalid key hex length: %zu (expected 32)", hex.length());
    return false;
  }
  for (size_t i = 0; i < 16; i++) {
    std::string byte_str = hex.substr(i * 2, 2);
    out_key.bytes[i] = static_cast<uint8_t>(std::strtoul(byte_str.c_str(), nullptr, 16));
  }
  out_key.is_set = true;
  return true;
}

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
