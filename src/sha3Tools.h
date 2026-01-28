#pragma once

#include <filesystem>
#include <string>

std::string sha3FileHash(const std::filesystem::path &path);
std::string sha3StringHash(const std::string &s);
std::string sha3DirectoryHash(const std::filesystem::path &dir, const std::string &excludeFile = "");
