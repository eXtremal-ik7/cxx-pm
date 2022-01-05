#pragma once

#include <filesystem>

struct CxxPmSettings {
  std::filesystem::path PackageRoot;
  std::filesystem::path HomeDir;
  std::filesystem::path DistrDir;
};
