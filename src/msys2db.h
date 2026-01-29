#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <stdint.h>

struct CMsys2Package {
  std::string Name;
  std::string Version;
  std::string Filename;
  std::string Sha256;
  size_t CompressedSize = 0;
  std::vector<std::string> Depends;
  std::vector<std::string> Provides;
};

// Parse msys.db (zstd-compressed tar) into package map
// Key: package name (and also provides aliases)
bool msys2ParseDatabase(const void *data, size_t size, std::map<std::string, CMsys2Package> &packages);

// Resolve transitive dependencies for a list of root package names
// Returns ordered list of packages to install (dependencies first)
bool msys2ResolveDependencies(const std::map<std::string, CMsys2Package> &packages,
                              const std::vector<std::string> &rootNames,
                              std::vector<const CMsys2Package*> &resolved);

// Default root package set for cxx-pm msys2 environment
const std::vector<std::string> &msys2DefaultPackages();

// Download, resolve and install msys2 packages into installDir
// If packageNames is empty, uses default set
bool msys2Install(const std::filesystem::path &installDir,
                  const std::vector<std::string> &packageNames);
