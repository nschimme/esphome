#pragma once

#ifdef USE_BLUETOOTH_SIG_MESH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome {
namespace bluetooth_sig_mesh {

struct MeshKey {
  std::array<uint8_t, 16> bytes{};
  bool is_set{false};
};

class BluetoothSIGMeshCrypto {
 public:
  static uint8_t mesh_k4(const uint8_t app_key[16]);
  static void mesh_aes_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]);
  static void mesh_s1(const uint8_t *m, size_t len, uint8_t out[16]);
  static void mesh_k1(const uint8_t n[16], const uint8_t *p, size_t p_len, uint8_t out[16]);
  static void mesh_k2(const uint8_t net_key[16], const uint8_t *p, size_t p_len, uint8_t *out_nid, uint8_t out_ek[16],
                      uint8_t out_pk[16]);
  static void obfuscate_header(const uint8_t privacy_key[16], uint32_t iv_index, const uint8_t privacy_random[7],
                               uint8_t header_data[6]);
  static bool decrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *ct, size_t ct_len,
                                   uint8_t *pt, size_t mic_len);
  static void encrypt_mesh_payload(const uint8_t key[16], const uint8_t nonce[13], const uint8_t *pt, size_t pt_len,
                                   uint8_t *ct, size_t mic_len);
  static bool parse_hex_key(const std::string &hex, MeshKey &out_key);
};

}  // namespace bluetooth_sig_mesh
}  // namespace esphome

#endif  // USE_BLUETOOTH_SIG_MESH
