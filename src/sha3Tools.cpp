#include "sha3Tools.h"
extern "C" {
#include "sha3.h"
#include "base64.h"
}
#include "strExtras.h"
#include <map>

std::string sha3FileHash(const std::filesystem::path &path)
{
  CCtxSha3 ctx;
  sha3Init(&ctx, 32);

  static constexpr unsigned bufferSize = 1u << 22;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[bufferSize]);
  FILE *hFile = fopen(path.string().c_str(), "rb");
  bool success = false;
  if (hFile) {
    size_t bytesRead = 0;
    while ( (bytesRead = fread(buffer.get(), 1, bufferSize, hFile)) )
      sha3Update(&ctx, buffer.get(), bytesRead);
    fclose(hFile);
    success = true;
  }

  if (success) {
    uint8_t hash[32];
    char hex[72] = {0};
    sha3Final(&ctx, hash, 0);
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
  CCtxSha3 ctx;
  sha3Init(&ctx, 32);
  sha3Update(&ctx, s.data(), s.size());
  sha3Final(&ctx, hash, 0);
  bin2hexLowerCase(hash, hex, 32);
  return hex;
}

std::string sha3StringHashBase64url(const std::string &s, size_t bytes)
{
  uint8_t hash[32];
  CCtxSha3 ctx;
  sha3Init(&ctx, 32);
  sha3Update(&ctx, s.data(), s.size());
  sha3Final(&ctx, hash, 0);

  if (bytes > 32)
    bytes = 32;
  char b64[48] = {0};
  size_t len = base64Encode(b64, hash, bytes);

  // Convert to base64url: replace + with - and / with _
  std::string result(b64, len);
  for (char &c : result) {
    if (c == '+') c = '-';
    else if (c == '/') c = '_';
  }

  // Strip padding
  while (!result.empty() && result.back() == '=')
    result.pop_back();

  return result;
}

static std::string sha3Hash(const std::filesystem::path &path, const std::string &excludeFile)
{
  if (std::filesystem::is_directory(path))
    return sha3DirectoryHash(path, excludeFile);
  else
    return sha3FileHash(path);
}

std::string sha3DirectoryHash(const std::filesystem::path &dir, const std::string &excludeFile)
{
  std::map<std::string, std::string> entries;

  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    if (!excludeFile.empty() && name == excludeFile)
      continue;
    std::string hash = sha3Hash(entry.path(), excludeFile);
    if (hash.empty())
      return std::string();
    entries[name] = hash;
  }

  std::string concatenated;
  for (const auto &[name, hash] : entries)
    concatenated += hash;

  return sha3StringHash(concatenated);
}
