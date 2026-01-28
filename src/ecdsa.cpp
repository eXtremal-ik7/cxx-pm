#include "ecdsa.h"
#include "strExtras.h"

#define SECP256K1_CPLUSPLUS_TEST_OVERRIDE
#include "secp256k1/include/secp256k1.h"

#include <cstring>

// Embedded public key (uncompressed format: 04 + 64 bytes)
static const char EMBEDDED_PUBLIC_KEY_HEX[] =
    "049cbe86c917103530a3dcb2572f976aaaee909060579012b78899e228f8539c1b"
    "434e5f622f925a60c45226bb9cbd1dfaff46eef452aa1245b3d8d9b6e2b25dae";

const char* getEmbeddedPublicKey()
{
    return EMBEDDED_PUBLIC_KEY_HEX;
}

std::string ecdsaSign(const uint8_t hash[32], const uint8_t privateKey[32])
{
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx)
        return std::string();

    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_sign(ctx, &sig, hash, privateKey, nullptr, nullptr)) {
        secp256k1_context_destroy(ctx);
        return std::string();
    }

    uint8_t sigBytes[64];
    secp256k1_ecdsa_signature_serialize_compact(ctx, sigBytes, &sig);
    secp256k1_context_destroy(ctx);

    char hex[129] = {0};
    bin2hexLowerCase(sigBytes, hex, 64);
    return hex;
}

bool ecdsaVerify(const uint8_t hash[32], const std::string &signatureHex)
{
    if (signatureHex.size() != 128)
        return false;

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    if (!ctx)
        return false;

    // Parse signature
    uint8_t sigBytes[64];
    hex2bin(signatureHex.c_str(), 128, sigBytes);

    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_signature_parse_compact(ctx, &sig, sigBytes)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    // Parse embedded public key
    uint8_t pubKeyBytes[65];
    hex2bin(EMBEDDED_PUBLIC_KEY_HEX, 130, pubKeyBytes);

    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_parse(ctx, &pubkey, pubKeyBytes, 65)) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    int result = secp256k1_ecdsa_verify(ctx, &sig, hash, &pubkey);
    secp256k1_context_destroy(ctx);

    return result == 1;
}
