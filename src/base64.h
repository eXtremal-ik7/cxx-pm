#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t base64GetDecodeLength(const char *in);
size_t base64getEncodeLength(size_t len);
size_t base64Decode(uint8_t *out, const char *in);
size_t base64Encode(char *out, const uint8_t *in, size_t size);

#ifdef __cplusplus
}
#endif
