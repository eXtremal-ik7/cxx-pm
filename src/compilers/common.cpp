#include "compilers/common.h"
#ifdef WIN32
#include "compilers/msvc.h"
#endif
#include "compilers/gnu.h"
#include "os.h"
#include <string.h>
#include <algorithm>

static std::string nameOfGCC(ELanguage lang)
{
  switch (lang) {
    case ELanguage::C: return "gcc";
    case ELanguage::CPP: return "g++";
    default: return std::string();
  }
}

static std::string nameOfClang(ELanguage lang)
{
  switch (lang) {
    case ELanguage::C: return "clang";
    case ELanguage::CPP: return "clang++";
    default: return std::string();
  }
}

static std::string nameOfDefaultUnixCompiler(ELanguage lang)
{
  switch (lang) {
    case ELanguage::C : return "cc";
    case ELanguage::CPP : return "c++";
    default : return std::string();
  }
}

ELanguage languageFromString(std::string_view lang)
{
  if (lang == "C")
    return ELanguage::C;
  else if (lang == "C++")
    return ELanguage::CPP;
  return ELanguage::Unknown;
}

const char *languageToString(ELanguage lang)
{
  switch (lang) {
    case ELanguage::C : return "C";
    case ELanguage::CPP : return "C++";
    default : return "<unknown>";
  }
}

const char *languageToStringEnv(ELanguage lang)
{
  switch (lang) {
    case ELanguage::C : return "C";
    case ELanguage::CPP : return "CXX";
    default : return "<unknown>";
  }
}

const char *compilerTypeToString(ECompilerType type)
{
  switch (type) {
    case ECompilerType::GCC : return "gcc";
    case ECompilerType::Clang : return "clang";
    case ECompilerType::MSVC : return "msvc";
    default : return "<unknown>";
  }
}

const char *toolTypeToStringEnv(EToolType type)
{
  switch (type) {
    case EToolType::Linker : return "LINKER";
    case EToolType::ResourceCompiler : return "RC";
    default : return "<unknown>";
  }
}

bool parseCompilerOption(ECompilerOptionType type, CompilersArray &compilers, const char *option)
{
  const char *pos = strchr(option, ':');
  if (!pos || pos == option || strlen(pos) == 0) {
    fprintf(stderr, "ERROR: can't parse compiler option: %s\n", option);
    return false;
  }

  std::string langS(option, pos);
  std::string value(pos + 1, pos + strlen(pos));
  ELanguage lang = languageFromString(langS);
  if (lang == ELanguage::Unknown) {
    fprintf(stderr, "ERROR: unsupported language %s\n", option);
    return false;
  }

  CCompilerInfo& info = compilers[static_cast<size_t>(lang)];
  switch (type) {
    case ECompilerOptionType::Command :
#ifdef __APPLE__
      if (value == "/Library/Developer/CommandLineTools/usr/bin/cc")
        value = "clang";
      else if (value == "/Library/Developer/CommandLineTools/usr/bin/c++")
        value = "clang++";
      else
        info.Command = value;
#else
      info.Command = value;
#endif
      break;
    default:
      fprintf(stderr, "ERROR: unsupported compiler option: %s\n", option);
      return false;
  }

  return true;
}

bool searchCompilers(std::vector<ELanguage> &langs, CompilersArray &compilers, ToolsArray &tools, CSystemInfo &systemInfo, bool verbose)
{
  // Search compiler for each language
  for (const ELanguage lang: langs) {
    CCompilerInfo &info = compilers[static_cast<size_t>(lang)];
    if (!info.Id.empty())
      continue;

    if (!info.Command.empty()) {
      // We have a compiler command
      // Try to detect a type of compiler
      bool compilerFound = false;
#ifdef WIN32
      if (loadMSVCSettings(info, systemInfo, verbose)) {
        fprintf(stderr, "ERROR: Direct path to MSVC compiler not supported\n");
        fprintf(stderr, "Use auto search with environment from vcvars***.bat or --vc-install-dir & --vc-toolset parameters\n");
        return false;
      }
#endif
      if (loadGNUSettings(info, verbose))
        continue;

      fprintf(stderr, "ERROR: can't interact with %s as compiler\n", info.Command.string().c_str());
      return false;
    } else {
      // No path specified, search compiler
#ifdef WIN32
      // For Win32 search priority is: visual studio, gcc, clang
      if (loadMSVCSettings(info, systemInfo, verbose))
        continue;

      info.Command = nameOfGCC(lang);
      if (loadGNUSettings(info, verbose))
        continue;

      info.Command = nameOfClang(lang);
      if (loadGNUSettings(info, verbose))
        continue;
#else
      // For *nix search GNU-style compiler only by default name
      info.Command = nameOfDefaultUnixCompiler(lang);
      if (loadGNUSettings(info, verbose))
        continue;
#endif

      if (verbose)
        fprintf(stderr, "ERROR: Can't found %s compiler\n", languageToString(lang));
      return false;
    }
  }

  // Check all compilers together, update system info
  systemInfo.TargetSystemSubType.clear();
  for (const ELanguage lang: langs) {
    CCompilerInfo &info = compilers[static_cast<size_t>(lang)];

    if (!info.DetectedSystemName.empty() && info.DetectedSystemName != systemInfo.TargetSystemName) {
      fprintf(stderr, "ERROR: target system is %s, %s compiler target is %s\n", systemInfo.TargetSystemName.c_str(), languageToString(lang), info.DetectedSystemName.c_str());
      return false;
    }

    if (!info.DetectedSystemProcessor.empty() && info.DetectedSystemProcessor != systemInfo.TargetSystemProcessor) {
      if (std::find(info.DetectedMultiArch.begin(), info.DetectedMultiArch.end(), systemInfo.TargetSystemProcessor) == info.DetectedMultiArch.end()) {
        fprintf(stderr, "ERROR: target processor is %s, %s compiler target is %s\n", systemInfo.TargetSystemProcessor.c_str(), languageToString(lang), info.DetectedSystemProcessor.c_str());
        return false;
      }
    }

    if (!systemInfo.TargetSystemSubType.empty()) {
      if (systemInfo.TargetSystemSubType != info.SystemSubType) {
        fprintf(stderr, "ERROR: compilers with different system subtypes (%s and %s) detected\n", systemInfo.TargetSystemSubType.c_str(), info.SystemSubType.empty() ? "<none>" : info.SystemSubType.c_str());
        return false;
      }
    } else {
      systemInfo.TargetSystemSubType = info.SystemSubType;
    }
  }

  const CCompilerInfo &c = compilers[static_cast<size_t>(ELanguage::C)];
  const CCompilerInfo &cpp = compilers[static_cast<size_t>(ELanguage::CPP)];
  if (!c.Id.empty() && !cpp.Id.empty() && (c.Type == ECompilerType::MSVC || cpp.Type == ECompilerType::MSVC)) {
    if (c.Command != cpp.Command || c.Type != cpp.Type) {
      fprintf(stderr, "ERROR: different C/C++ compilers of MSVC type not supported\nC: %s\nC++: %s\n", c.Command.string().c_str(), cpp.Command.string().c_str());
      return false;
    }
  }


  // Search binutils and other tools
  if (systemInfo.TargetSystemSubType == "msvc") {
#ifdef WIN32
    if (!msvcLookupVersion(systemInfo))
      return false;
    if (!msvcSearchTools(tools, compilers, systemInfo))
      return false;
#endif
  } else {
    if (!gnuSearchTools(tools, compilers, systemInfo))
      return false;
  }

  return true;
}
