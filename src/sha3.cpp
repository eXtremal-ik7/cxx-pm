#include "sha3.h"
extern "C" {
#include "tiny_sha3.h"
}
#include "strExtras.h"

std::string sha3FileHash(const std::filesystem::path &path)
{
  sha3_ctx_t ctx;
  sha3_init(&ctx, 32);

  static constexpr unsigned bufferSize = 1u << 22;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[bufferSize]);
  FILE *hFile = fopen(path.string().c_str(), "rb");
  bool success = false;
  if (hFile) {
    size_t bytesRead = 0;
    while ( (bytesRead = fread(buffer.get(), 1, bufferSize, hFile)) )
      sha3_update(&ctx, buffer.get(), bytesRead);
    fclose(hFile);
    success = true;
  }

  if (success) {
    uint8_t hash[32];
    char hex[72] = {0};
    sha3_final(hash, &ctx, 0);
    bin2hexLowerCase(hash, hex, 32);
    return hex;
  } else {
    return std::string();
  }
}

std::string sha3StringHash(const std::string &s)
{
  uint8_t hash[32];
  char hex[72] = {0};
  sha3_ctx_t ctx;
  sha3_init(&ctx, 32);
  sha3_update(&ctx, s.data(), s.size());
  sha3_final(hash, &ctx, 0);
  bin2hexLowerCase(hash, hex, 32);
  return hex;
}
