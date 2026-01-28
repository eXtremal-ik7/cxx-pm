#pragma once

#include <string>
#include <cstdint>

// Sign a 32-byte hash with private key, returns 64-byte signature in hex (128 chars)
std::string ecdsaSign(const uint8_t hash[32], const uint8_t privateKey[32]);

// Verify signature against 32-byte hash using embedded public key
bool ecdsaVerify(const uint8_t hash[32], const std::string &signatureHex);

// Get embedded public key in hex
const char* getEmbeddedPublicKey();
