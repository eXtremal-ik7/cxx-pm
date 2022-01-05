#pragma once

#include <stdint.h>

static inline uint8_t hexDigit2bin(char c)
{
  uint8_t digit = c - '0';
  if (digit >= 10)
    digit -= ('A' - '0' - 10);
  if (digit >= 16)
    digit -= ('a' - 'A');
  return digit;
}

static inline char bin2hexLowerCaseDigit(uint8_t b)
{
  return b < 10 ? '0'+b : 'a'+b-10;
}

static inline void hex2bin(const char *in, size_t inSize, void *out)
{
  uint8_t *pOut = static_cast<uint8_t*>(out);
  for (size_t i = 0; i < inSize/2; i++)
    pOut[i] = (hexDigit2bin(in[i*2]) << 4) | hexDigit2bin(in[i*2+1]);
}

static inline void bin2hexLowerCase(const void *in, char *out, size_t size)
{
  const uint8_t *pIn = static_cast<const uint8_t*>(in);
  for (size_t i = 0, ie = size; i != ie; ++i) {
    out[i*2] = bin2hexLowerCaseDigit(pIn[i] >> 4);
    out[i*2+1] = bin2hexLowerCaseDigit(pIn[i] & 0xF);
  }
} 
