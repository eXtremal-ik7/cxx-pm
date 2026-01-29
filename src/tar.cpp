#include "tar.h"
#include <string.h>
#include <stdio.h>

static size_t tarParseOctal(const char *s, size_t len)
{
  size_t result = 0;
  for (size_t i = 0; i < len && s[i] >= '0' && s[i] <= '7'; i++)
    result = result * 8 + (s[i] - '0');
  return result;
}

bool tarParse(const void *data, size_t size, std::vector<TarEntry> &entries)
{
  const uint8_t *p = static_cast<const uint8_t*>(data);
  const uint8_t *end = p + size;

  while (p + 512 <= end) {
    // Check for end-of-archive (two zero blocks)
    bool allZero = true;
    for (int i = 0; i < 512; i++) {
      if (p[i] != 0) { allZero = false; break; }
    }
    if (allZero)
      break;

    // Parse tar header (POSIX ustar format)
    const char *name = reinterpret_cast<const char*>(p);           // offset 0, 100 bytes
    const char *sizeField = reinterpret_cast<const char*>(p + 124); // offset 124, 12 bytes
    char typeflag = static_cast<char>(p[156]);                      // offset 156
    const char *prefix = reinterpret_cast<const char*>(p + 345);    // offset 345, 155 bytes

    size_t fileSize = tarParseOctal(sizeField, 12);
    p += 512; // advance past header

    // Build full name
    std::string fullName;
    size_t prefixLen = strnlen(prefix, 155);
    if (prefixLen > 0) {
      fullName.append(prefix, prefixLen);
      fullName.push_back('/');
    }
    fullName.append(name, strnlen(name, 100));

    // Only process regular files
    if (typeflag == '0' || typeflag == '\0') {
      if (p + fileSize > end)
        return false;
      entries.emplace_back();
      entries.back().Name = std::move(fullName);
      entries.back().Data.assign(reinterpret_cast<const char*>(p), fileSize);
    }

    // Advance past data, rounded up to 512-byte boundary
    size_t blocks = (fileSize + 511) / 512;
    p += blocks * 512;
  }

  return true;
}

bool tarExtract(const void *data, size_t size, const std::filesystem::path &destDir)
{
  const uint8_t *p = static_cast<const uint8_t*>(data);
  const uint8_t *end = p + size;

  while (p + 512 <= end) {
    bool allZero = true;
    for (int i = 0; i < 512; i++) {
      if (p[i] != 0) { allZero = false; break; }
    }
    if (allZero)
      break;

    const char *name = reinterpret_cast<const char*>(p);
    const char *sizeField = reinterpret_cast<const char*>(p + 124);
    char typeflag = static_cast<char>(p[156]);
    const char *linkname = reinterpret_cast<const char*>(p + 157);
    const char *prefix = reinterpret_cast<const char*>(p + 345);

    size_t fileSize = tarParseOctal(sizeField, 12);
    p += 512;

    std::string fullName;
    size_t prefixLen = strnlen(prefix, 155);
    if (prefixLen > 0) {
      fullName.append(prefix, prefixLen);
      fullName.push_back('/');
    }
    fullName.append(name, strnlen(name, 100));

    // Skip empty names
    if (fullName.empty()) {
      size_t blocks = (fileSize + 511) / 512;
      p += blocks * 512;
      continue;
    }

    std::filesystem::path outPath = destDir / fullName;

    if (typeflag == '5') {
      // Directory
      std::error_code ec;
      std::filesystem::create_directories(outPath, ec);
    } else if (typeflag == '0' || typeflag == '\0') {
      // Regular file
      if (p + fileSize > end)
        return false;

      std::error_code ec;
      std::filesystem::create_directories(outPath.parent_path(), ec);

      FILE *f = fopen(outPath.string().c_str(), "wb");
      if (f) {
        fwrite(p, 1, fileSize, f);
        fclose(f);
      }
    } else if (typeflag == '2') {
      // Symlink
      std::string target(linkname, strnlen(linkname, 100));
      std::error_code ec;
      std::filesystem::create_directories(outPath.parent_path(), ec);
      std::filesystem::remove(outPath, ec);
      std::filesystem::create_symlink(target, outPath, ec);
    } else if (typeflag == '1') {
      // Hard link
      std::string target(linkname, strnlen(linkname, 100));
      std::filesystem::path targetPath = destDir / target;
      std::error_code ec;
      std::filesystem::create_directories(outPath.parent_path(), ec);
      std::filesystem::remove(outPath, ec);
      std::filesystem::create_hard_link(targetPath, outPath, ec);
    }

    size_t blocks = (fileSize + 511) / 512;
    p += blocks * 512;
  }

  return true;
}
