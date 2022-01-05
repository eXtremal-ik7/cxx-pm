#pragma once

#include "compilers/common.h"

namespace json11 {
class Json;
}

struct CxxPmSettings;

struct CPackage {
  std::string Name;
  std::filesystem::path Path;
  std::vector<std::filesystem::path> ExtraPath;
  // This members fill by inspect function
  std::string Version;
  bool IsBinary;
  std::filesystem::path Prefix;
  std::filesystem::path BuildFile;
  std::vector<ELanguage> Languages;
};

enum class EArtifactType : unsigned {
  Unknown = 0,
  IncludeDirectory,
  StaticLibrary,
  SharedLibrary,
  Executable,
  LibSet,
  CMakeModule
};

EArtifactType artifactTypeFromString(std::string_view type);
const char *artifactTypeToString(EArtifactType type);

struct CArtifact {
  EArtifactType Type;
  std::string Name;
  std::vector<std::string> Libs;
  std::vector<std::string> IncludeLinks;

  std::vector<std::filesystem::path> RelativePaths;
  std::vector<std::filesystem::path> DllPaths;
  std::vector<std::filesystem::path> ImplibPath;
  std::vector<std::vector<std::string>> Definitions;

  bool loadFromJson(const json11::Json &json);
  bool merge(const CArtifact &artifact);
};

std::filesystem::path packagePrefix(const std::filesystem::path &cxxPmHome, const CPackage &package, const CompilersArray &compilers, const CSystemInfo &systemInfo, const std::string &buildType, bool verbose);


void prepareBuildEnvironment(std::vector<std::string> &env,
                             const CPackage &package,
                             const CxxPmSettings &globalSettings,
                             const CSystemInfo &systemInfo,
                             const CompilersArray &compilers,
                             const ToolsArray &tools,
                             const std::string &buildType,
                             bool verbose);

