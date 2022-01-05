#pragma once

#include <filesystem>

std::string sha3FileHash(const std::filesystem::path &path);
std::string sha3StringHash(const std::string &s);
