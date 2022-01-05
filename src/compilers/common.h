#pragma once

#include <array>
#include <filesystem>
#include <unordered_map>
#include <vector>

struct CSystemInfo;

enum class ELanguage : unsigned {
  Unknown = 0,
  C,
  CPP,
  NumberOf
};

enum class EToolType : unsigned {
  Unknown = 0,
  Linker,
  ResourceCompiler,
  NumberOf
};

enum class ECompilerType : unsigned {
  Unknown = 0,
  GCC,
  Clang,
  MSVC
};

enum class ECompilerOptionType : unsigned {
  Unknown = 0,
  Command,
  Flags,
  Definitions
};

struct CCompilerInfo {
  std::filesystem::path Command;
  std::string Id;

  ECompilerType Type = ECompilerType::Unknown;
  std::string SystemSubType;
  std::string DetectedSystemName;
  std::string DetectedSystemProcessor;
  std::vector<std::string> DetectedMultiArch;
  std::string ReportedTarget;
};

struct CToolInfo {
  std::filesystem::path Command;
};

using CompilersArray = std::array<CCompilerInfo, static_cast<size_t>(ELanguage::NumberOf)>;
using ToolsArray = std::array<CToolInfo, static_cast<size_t>(EToolType::NumberOf)>;

ELanguage languageFromString(std::string_view lang);
const char *languageToString(ELanguage lang);
const char *languageToStringEnv(ELanguage lang);
const char *compilerTypeToString(ECompilerType type);
const char *toolTypeToStringEnv(EToolType type);

bool parseCompilerOption(ECompilerOptionType type, CompilersArray &compilers, const char* option);
bool searchCompilers(std::vector<ELanguage> &langs, CompilersArray &compilers, ToolsArray &tools, CSystemInfo &systemInfo, bool verbose);
