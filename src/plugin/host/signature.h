// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_SIGNATURE_H_
#define PLUGIN_SIGNATURE_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace plugin {

struct SignatureVerifier {
  static bool sign(std::string_view zip_bytes, const uint8_t seed[32],
                   uint8_t sig64[64]);
  static bool verify(std::string_view zip_bytes, const uint8_t sig64[64],
                     const uint8_t pubkey32[32]);
  static std::string sha256_hex(std::string_view bytes);
  static void sha256(std::string_view bytes, uint8_t out[32]);
};

}  // namespace plugin

#endif  // PLUGIN_SIGNATURE_H_
