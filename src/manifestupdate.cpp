#include "sha3Tools.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

static const char *MANIFEST_FILENAME = "MANIFEST";

static std::string computeHash(const std::filesystem::path &path)
{
  if (std::filesystem::is_directory(path))
    return sha3DirectoryHash(path, MANIFEST_FILENAME);
  else
    return sha3FileHash(path);
}

bool generateManifest(const std::filesystem::path &dir, std::ostream &out)
{
  std::map<std::string, std::string> entries;

  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    if (name == MANIFEST_FILENAME || (!name.empty() && name[0] == '.'))
      continue;
    std::string hash = computeHash(entry.path());
    if (hash.empty()) {
      std::cerr << "ERROR: failed to compute hash for " << entry.path() << std::endl;
      return false;
    }
    entries[name] = hash;
  }

  for (const auto &[name, hash] : entries)
    out << name << " " << hash << "\n";

  return true;
}

int main(int argc, char **argv)
{
  std::filesystem::path dir = std::filesystem::current_path();
  if (argc > 1)
    dir = argv[1];

  if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
    std::cerr << "ERROR: " << dir << " is not a directory" << std::endl;
    return 1;
  }

  std::filesystem::path manifestPath = dir / MANIFEST_FILENAME;
  std::ofstream out(manifestPath);
  if (!out) {
    std::cerr << "ERROR: cannot open " << manifestPath << " for writing" << std::endl;
    return 1;
  }

  if (!generateManifest(dir, out)) {
    return 1;
  }

  std::cout << "Manifest written to " << manifestPath << std::endl;
  return 0;
}
