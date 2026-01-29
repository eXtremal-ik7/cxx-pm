#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <stdint.h>

// Download URL to file. Returns true on success.
bool httpDownloadFile(const std::string &url, const std::filesystem::path &destPath);

// Download URL to memory. Returns true on success.
bool httpDownloadToMemory(const std::string &url, std::vector<uint8_t> &data);
