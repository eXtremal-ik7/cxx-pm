#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class EPathType : unsigned {
  Unknown = 0,
  Native,
  Posix,
  CMake
};

struct CBuildType {
  std::string Name;
  std::string MappedTo;
  CBuildType() {}
  CBuildType(std::string_view name, std::string_view mappedTo) : Name(name), MappedTo(mappedTo) {}
};

struct CSystemInfo {
  std::filesystem::path Self;
  std::filesystem::path MSys2Path;
  // Host 
  std::string HostSystemName;
  std::string HostSystemProcessor;
  // Target
  std::string TargetSystemName;
  std::string TargetSystemProcessor;
  std::string TargetSystemSubType;
  // Other
  std::vector<CBuildType> BuildType;
  // MSVC specific
  std::filesystem::path VCInstallDir;
  std::string VCToolSet;
  std::string VSToolSetVersion;
  // MacOS specific
  std::filesystem::path ISysRoot;
};

std::string systemProcessorNormalize(const std::string_view processor);
std::string osGetSystemName();
std::string osGetSystemProcessor();

EPathType pathTypeFromString(const std::string &type);
std::filesystem::path pathConvert(const std::filesystem::path& path, EPathType type);

bool doBuildTypeMapping(const std::string &buildType, const std::string &mapping, std::vector<CBuildType> &out);
void uniqueBuildTypes(const std::vector<CBuildType> &in, std::vector<std::string> &out);
std::filesystem::path userHomeDir();
std::filesystem::path whereami(const char *argv0);
