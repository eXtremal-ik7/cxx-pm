#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <stdint.h>
#include <stddef.h>

struct TarEntry {
  std::string Name;
  std::string Data;
};

bool tarParse(const void *data, size_t size, std::vector<TarEntry> &entries);
bool tarExtract(const void *data, size_t size, const std::filesystem::path &destDir);
