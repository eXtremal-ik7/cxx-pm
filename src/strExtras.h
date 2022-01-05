#pragma once

#include <string.h>
#include <string>

class RawData {
public:
  RawData(const char *s) : Data_(s), Size_(strlen(s)) {}
  RawData(const std::string &s) : Data_(s.data()), Size_(s.size()) {}
  const char *data() const { return Data_; }
  size_t size() const { return Size_; }
  bool operator==(RawData data) { return Size_ == data.Size_ && memcmp(Data_, data.Data_, Size_) == 0; }

  static void assign(std::string &s, RawData data) { s.assign(data.Data_, data.Size_); }

private:
  const char *Data_;
  size_t Size_;
};

class StringSplitter {
public:
  StringSplitter(std::string_view input, const char *delimiters) : Input_(input.data()), Delimiters_(delimiters) {
    P_ = Last_ = Input_;
    End_ = P_ + input.size();
  }

  std::string_view get();
  bool next();

private:
  const char *Input_;
  const char *Delimiters_;
  const char *P_;
  const char *Last_;
  const char *End_;
  bool EndReached_ = false;
};

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

static inline bool startsWith(RawData s, RawData substr)
{
  return substr.size() <= s.size() && memcmp(s.data(), substr.data(), substr.size()) == 0;
}

static inline bool endsWith(RawData s, RawData substr)
{
  return substr.size() <= s.size() && memcmp(s.data() + s.size() - substr.size(), substr.data(), substr.size()) == 0;
}
