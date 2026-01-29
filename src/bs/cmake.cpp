#include "cmake.h"
#include "cxx-pm.h"
#include "exec.h"
#include "package.h"
#include "strExtras.h"
#include "compilers/gnu.h"
#include "json/json11.hpp"
#include <fstream>
#include <unordered_set>
#ifdef WIN32
#include <Windows.h>
#include "compilers/msvc.h"
#endif

#ifdef WIN32
std::string cmakeGetMSVCConfigureArgs(const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string& buildType)
{
  std::string args = "(";
  args.append("-DCMAKE_CONFIGURATION_TYPES=");
    args.append(buildType);
    args.push_back(' ');

  std::string platform = getVsArch(systemInfo.TargetSystemProcessor);
  if (!platform.empty()) {
    args.append("-DCMAKE_GENERATOR_PLATFORM=");
    args.append(platform);
    args.push_back(' ');
  }
    
  // CMAKE_GENERATOR_INSTANCE <- env(VCINSTALLDIR)
  std::string vsInstallDir;
  {
    DWORD size = GetEnvironmentVariable("VSINSTALLDIR", NULL, 0);
    if (size) {
      vsInstallDir.resize(size - 1);
      GetEnvironmentVariable("VSINSTALLDIR", vsInstallDir.data(), size);
    }
  }

  args.append("-DCMAKE_GENERATOR_INSTANCE=\"");
    args.append(pathConvert(std::filesystem::path(vsInstallDir), EPathType::CMake).string());
    args.append("\" ");

  if (!systemInfo.VCToolSet.empty()) {
    args.append("-DCMAKE_GENERATOR_PLATFORM=");
    args.append(platform);
  }

  args.push_back(')');
  return args;
}

std::string cmakeGetMSVCBuildArgs(const CPackage& package, const CompilersArray& compilers, const ToolsArray& tools, const CSystemInfo& systemInfo, const std::string& buildType)
{
  std::string args = "(";
  args.append("--config ");
  args.append(buildType);
  args.push_back(')');
  return args;
}
#endif

std::string cmakeGetConfigureArgs(const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string &buildType)
{
#ifdef WIN32
  if (systemInfo.TargetSystemSubType == "msvc")
    return cmakeGetMSVCConfigureArgs(package, compilers, tools, systemInfo, buildType);
#endif

  std::string args = "(";
  args.append("-DCMAKE_BUILD_TYPE=");
    args.append(buildType);
    args.push_back(' ');

  args.append("-DCMAKE_SYSTEM_NAME=");
    args.append(systemInfo.TargetSystemName);
    args.push_back(' ');

  args.append("-DCMAKE_SYSTEM_PROCESSOR=");
    args.append(systemInfo.TargetSystemProcessor);
    args.push_back(' ');

  if (systemInfo.TargetSystemName == "Darwin") {
    // For MacOS X need to use CMAKE_OSX_ARCHITECTURES
    args.append("-DCMAKE_OSX_ARCHITECTURES=");
    args.append(gnuClangProcessorFromNormalized(systemInfo.TargetSystemProcessor));
    args.push_back(' ');

    args.append("-DCMAKE_OSX_SYSROOT=");
    args.append(systemInfo.ISysRoot.string());
    args.push_back(' ');
  }

  for (ELanguage lang: package.Languages) {
    if (lang == ELanguage::C) {
      const CCompilerInfo &info = compilers[static_cast<size_t>(ELanguage::C)];
      args.append("-DCMAKE_C_COMPILER=");
      args.append(info.Command.string());
      args.push_back(' ');
    } else if (lang == ELanguage::CPP) {
      const CCompilerInfo &info = compilers[static_cast<size_t>(ELanguage::CPP)];
      args.append("-DCMAKE_CXX_COMPILER=");
      args.append(info.Command.string());
      args.push_back(' ');
    }
  }

  args.push_back(')');
  return args;
}

std::string cmakeGetBuildArgs(const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string &buildType)
{
#ifdef WIN32
  if (systemInfo.TargetSystemSubType == "msvc")
    return cmakeGetMSVCBuildArgs(package, compilers, tools, systemInfo, buildType);
#endif

  return std::string();
}

static bool collectPackageArtifacts(const CPackage &package,
                                    const CxxPmSettings &globalSettings,
                                    const CompilersArray &compilers,
                                    const ToolsArray &tools,
                                    const CSystemInfo &systemInfo,
                                    bool verbose,
                                    std::vector<CArtifact> &artifacts,
                                    std::vector<std::filesystem::path> &prefixes)
{
  bool firstRun = true;
  for (size_t i = 0, ie = systemInfo.BuildType.size(); i != ie; ++i) {
    std::string args;
    std::vector<std::string> env;
    prepareBuildEnvironment(env, package, globalSettings, systemInfo, compilers, tools, systemInfo.BuildType[i].MappedTo, verbose);

    args = "set -x; set -e; source ";
#ifdef WIN32
    args.append(pathConvert(package.BuildFile, EPathType::CMake).string());
#else
    args.append(pathConvert(package.BuildFile, EPathType::Posix).string());
#endif
    args.append("; artifacts;");
    std::filesystem::path fullPath;
    std::string capturedOut;
    std::string capturedErr;
    if (!run(package.BuildFile.parent_path(), "bash", {"-c", args}, env, fullPath, capturedOut, capturedErr, true)) {
      fprintf(stderr, "ERROR: can't get build artifacts for %s\n", package.Name.c_str());
      fprintf(stderr, "%s\n", capturedErr.c_str());
      return false;
    }

    std::string parseError;
    auto json = json11::Json::parse(capturedOut, parseError);
    if (!parseError.empty()) {
      fprintf(stderr, "%s\n", capturedOut.c_str());
      fprintf(stderr, "ERROR: invalid json: %s\n", parseError.c_str());
      return false;
    }

    if (!json.is_array()) {
      return false;
    }

    size_t artIdx = 0;
    for (const auto &artifactJson: json.array_items()) {
      CArtifact a;
      if (!a.loadFromJson(artifactJson)) {
        fprintf(stderr, "ERROR: artifact parse error\n");
        return false;
      }

      if (firstRun) {
        artifacts.emplace_back(std::move(a));
      } else {
        if (artIdx >= artifacts.size()) {
          fprintf(stderr, "ERROR: %s and %s confirutations have a different artifacts number, aborting...\n", systemInfo.BuildType[i-1].Name.c_str(), systemInfo.BuildType[i].Name.c_str());
          return false;
        }

        if (!artifacts[artIdx].merge(a)) {
          fprintf(stderr, "ERROR: artifact merge error\n");
          return false;
        }
      }

      artIdx++;
    }

    prefixes.emplace_back(packagePrefix(globalSettings.HomeDir, package, compilers, systemInfo, systemInfo.BuildType[i].MappedTo, verbose));
    firstRun = false;
  }

  return true;
}

static bool writeArtifactsCmake(std::ofstream &generatedFile,
                                const std::vector<CArtifact> &artifacts,
                                const std::vector<std::filesystem::path> &prefixes,
                                const CSystemInfo &systemInfo,
                                const std::unordered_set<std::string> &libSet,
                                std::vector<std::string> &libraryTargets)
{
  for (const auto &a: artifacts) {
    switch (a.Type) {
      case EArtifactType::IncludeDirectory : {
        std::string cmakePath;
        if (systemInfo.BuildType.size() == 1) {
          std::filesystem::path aPath = prefixes[0] / "install" / a.RelativePaths[0];
          if (!std::filesystem::exists(aPath)) {
            fprintf(stderr, "ERROR: artifact %s not exists\n", aPath.string().c_str());
            return false;
          }

          cmakePath = pathConvert(aPath, EPathType::CMake).string();
        } else if (systemInfo.BuildType.size() >= 2) {
          // Build cmake expression
          // $<$<CONFIG:Debug>:some_dbg>$<$<CONFIG:Release>:some_release>
          for (size_t i = 0, ie = prefixes.size(); i != ie; ++i) {
            std::filesystem::path aPath = prefixes[i] / "install" / a.RelativePaths[i];
            if (!std::filesystem::exists(aPath)) {
              fprintf(stderr, "ERROR: artifact %s not exists\n", aPath.string().c_str());
              return false;
            }

            cmakePath.append("$<$<CONFIG:");
            cmakePath.append(systemInfo.BuildType[i].Name);
            cmakePath.append(">:");
            cmakePath.append(pathConvert(aPath, EPathType::CMake).string());
            cmakePath.push_back('>');
          }
        }

        generatedFile << "set(" << a.Name << ' ' << cmakePath << " PARENT_SCOPE)\n";
        break;
      }

      case EArtifactType::StaticLibrary :
      case EArtifactType::SharedLibrary : {
        std::string libType = a.Type == EArtifactType::StaticLibrary ? "STATIC" : "SHARED";
        generatedFile << "add_library(" << a.Name << ' ' << libType << ' ' << "IMPORTED GLOBAL)\n";
        libraryTargets.push_back(a.Name);

        if (systemInfo.BuildType.size() == 1) {
          auto libraryFile = a.Type == EArtifactType::SharedLibrary && systemInfo.TargetSystemName == "Windows" ?
            a.DllPaths[0] :
            a.RelativePaths[0];
          std::filesystem::path aPath = prefixes[0] / "install" / libraryFile;
          if (!std::filesystem::exists(aPath)) {
            fprintf(stderr, "ERROR: library artifact %s not exists\n", aPath.string().c_str());
            return false;
          }

          generatedFile << "set_target_properties(" << a.Name << ' ' << "PROPERTIES IMPORTED_LOCATION " << pathConvert(aPath, EPathType::CMake) << ")\n";
        } else {
          for (size_t i = 0, ie = prefixes.size(); i != ie; ++i) {
            std::string buildTypeUpperCase = systemInfo.BuildType[i].Name;
            for (auto& c : buildTypeUpperCase)
              c = toupper(c);

            generatedFile << "set_target_properties(" << a.Name;
            auto libraryFile = a.Type == EArtifactType::SharedLibrary && systemInfo.TargetSystemName == "Windows" ?
              a.DllPaths[i] :
              a.RelativePaths[i];

            std::filesystem::path aPath = prefixes[i] / "install" / libraryFile;
            if (!std::filesystem::exists(aPath)) {
              fprintf(stderr, "ERROR: library artifact %s not exists\n", aPath.string().c_str());
              return false;
            }

            generatedFile << "  PROPERTIES IMPORTED_LOCATION_" << buildTypeUpperCase << ' ' << pathConvert(aPath, EPathType::CMake) << ")\n";
          }
          generatedFile << "\n";
        }

        if (a.Type == EArtifactType::SharedLibrary && systemInfo.TargetSystemName == "Windows") {
          if (systemInfo.BuildType.size() == 1) {
            std::filesystem::path aPath = prefixes[0] / "install" / a.ImplibPath[0];
            if (!std::filesystem::exists(aPath)) {
              fprintf(stderr, "ERROR: implib artifact %s not exists\n", aPath.string().c_str());
              return false;
            }

            generatedFile << "set_target_properties(" << a.Name << ' ' << "PROPERTIES IMPORTED_IMPLIB " << pathConvert(aPath, EPathType::CMake) << ")\n";
          } else {
            for (size_t i = 0, ie = prefixes.size(); i != ie; ++i) {
              std::string buildTypeUpperCase = systemInfo.BuildType[i].Name;
              for (auto& c : buildTypeUpperCase)
                c = toupper(c);

              generatedFile << "set_target_properties(" << a.Name;

              std::filesystem::path aPath = prefixes[i] / "install" / a.ImplibPath[i];
              if (!std::filesystem::exists(aPath)) {
                fprintf(stderr, "ERROR: implib artifact %s not exists\n", aPath.string().c_str());
                return false;
              }

              generatedFile << "  PROPERTIES IMPORTED_IMPLIB_" << buildTypeUpperCase << ' ' << pathConvert(aPath, EPathType::CMake) << ")\n";
            }
            generatedFile << "\n";
          }
        }

        if (!a.IncludeLinks.empty()) {
          // Search linked include directories
          // NOTE: O(n^2)
          std::vector<size_t> linkedIncludes;
          for (const auto &link: a.IncludeLinks) {
            bool found = false;
            for (size_t i = 0, ie = artifacts.size(); i != ie; ++i) {
              if (artifacts[i].Type == EArtifactType::IncludeDirectory && artifacts[i].Name == link) {
                found = true;
                linkedIncludes.push_back(i);
              }
            }

            if (!found) {
              fprintf(stderr, "ERROR: library %s requires include non-existing include directory %s\n", a.Name.c_str(), link.c_str());
              return false;
            }
          }

          std::string cmakePath;
          for (size_t linkIdx: linkedIncludes) {
            if (systemInfo.BuildType.size() == 1) {
              cmakePath.append("\n  ");
              std::filesystem::path aPath = prefixes[0] / "install" / artifacts[linkIdx].RelativePaths[0];
              cmakePath.append(pathConvert(aPath, EPathType::CMake).string());
            } else {
              // Build cmake expression
              // $<$<CONFIG:Debug>:some_dbg>$<$<CONFIG:Release>:some_release>
              cmakePath.append("\n  ");
              for (size_t i = 0, ie = prefixes.size(); i != ie; ++i) {
                std::filesystem::path aPath = prefixes[i] / "install" / artifacts[linkIdx].RelativePaths[i];
                cmakePath.append("$<$<CONFIG:");
                cmakePath.append(systemInfo.BuildType[i].Name);
                cmakePath.append(">:");
                cmakePath.append(pathConvert(aPath, EPathType::CMake).string());
                cmakePath.push_back('>');
              }
            }
          }

          generatedFile << "set_target_properties(" << a.Name << " PROPERTIES INTERFACE_INCLUDE_DIRECTORIES" << cmakePath << "\n)\n";
        }

        bool hasDefinitions = false;
        for (const auto &defList: a.Definitions) {
          if (!defList.empty()) {
            hasDefinitions = true;
            break;
          }
        }

        if (hasDefinitions) {
          std::string defs;
          if (systemInfo.BuildType.size() == 1) {
            defs.append("\n  \"");
            bool firstDef = true;
            for (const auto &def: a.Definitions[0]) {
              if (!firstDef)
                defs.push_back(';');
              defs.append(def);
              firstDef = false;
            }
            defs.push_back('\"');
          } else {
            // Build cmake expression
            // $<$<CONFIG:Debug>:some_dbg>$<$<CONFIG:Release>:some_release>
            defs.append("\n  ");
            for (size_t i = 0, ie = prefixes.size(); i != ie; ++i) {
              defs.append("$<$<CONFIG:");
              defs.append(systemInfo.BuildType[i].Name);
              defs.append(">:");
              {
                defs.push_back('\"');
                bool firstDef = true;
                for (const auto &def: a.Definitions[i]) {
                  if (!firstDef)
                    defs.push_back(';');
                  defs.append(def);
                  firstDef = false;
                }
                defs.push_back('\"');
              }
              defs.push_back('>');
            }
          }
          generatedFile << "set_target_properties(" << a.Name << " PROPERTIES INTERFACE_COMPILE_DEFINITIONS" << defs << "\n)\n";
        }

        break;
      }

    case EArtifactType::Executable : {
      generatedFile << "add_executable(" << a.Name << ' ' << "IMPORTED)\n";

      if (systemInfo.BuildType.size() == 1) {
        std::filesystem::path aPath = prefixes[0] / "install" / a.RelativePaths[0];
        if (!std::filesystem::exists(aPath)) {
          fprintf(stderr, "ERROR: artifact %s not exists\n", aPath.string().c_str());
          return false;
        }

        generatedFile << "set_target_properties(" << a.Name << ' ' << "PROPERTIES IMPORTED_LOCATION " << pathConvert(aPath, EPathType::CMake) << ")\n";
      } else {
        for (size_t i = 0, ie = prefixes.size(); i != ie; ++i) {
          std::string buildTypeUpperCase = systemInfo.BuildType[i].Name;
          for (auto& c : buildTypeUpperCase)
            c = toupper(c);

          generatedFile << "set_target_properties(" << a.Name;
          std::filesystem::path aPath = prefixes[i] / "install" / a.RelativePaths[i];
          if (!std::filesystem::exists(aPath)) {
            fprintf(stderr, "ERROR: artifact %s not exists\n", aPath.string().c_str());
            return false;
          }

          generatedFile << "  PROPERTIES IMPORTED_LOCATION_" << buildTypeUpperCase << ' ' << pathConvert(aPath, EPathType::CMake) << ")\n";
        }
        generatedFile << "\n";
      }
      break;
    }

      case EArtifactType::LibSet : {
        generatedFile << "set(" << a.Name;
        for (const auto &lib: a.Libs) {
          if (!libSet.count(lib)) {
            fprintf(stderr, "ERROR: libset %s has link to non-existent library %s\n", a.Name.c_str(), lib.c_str());
            return false;
          }

          generatedFile << ' ' << lib;
        }
        generatedFile << " PARENT_SCOPE)\n";
        break;
      }

      case EArtifactType::CMakeModule : {
        generatedFile << "include(" << pathConvert(prefixes[0] / "install" / a.RelativePaths[0], EPathType::CMake) << ")\n";
        break;
      }

      case EArtifactType::SystemLibrary :
        // Handled below
        break;

      default:
        break;
    }
  }

  // Collect system libraries matching the target system
  std::string sysLibList;
  for (const auto &a : artifacts) {
    if (a.Type != EArtifactType::SystemLibrary)
      continue;
    if (!a.SystemName.empty() && a.SystemName != systemInfo.TargetSystemName)
      continue;
    if (!sysLibList.empty())
      sysLibList.push_back(';');
    sysLibList.append(a.Name);
  }

  if (!sysLibList.empty()) {
    for (const auto &lib : libraryTargets) {
      generatedFile << "set_property(TARGET " << lib << " APPEND PROPERTY INTERFACE_LINK_LIBRARIES \"" << sysLibList << "\")\n";
    }
  }

  return true;
}

static bool exportSinglePackage(const CPackage &package,
                                const CxxPmSettings &globalSettings,
                                const CompilersArray &compilers,
                                const ToolsArray &tools,
                                const CSystemInfo &systemInfo,
                                const std::filesystem::path &output,
                                bool verbose,
                                std::vector<std::string> &libraryTargets)
{
  std::vector<CArtifact> artifacts;
  std::vector<std::filesystem::path> prefixes;
  if (!collectPackageArtifacts(package, globalSettings, compilers, tools, systemInfo, verbose, artifacts, prefixes))
    return false;

  std::unordered_set<std::string> libSet;
  for (const auto &a : artifacts) {
    if (a.Type == EArtifactType::SharedLibrary || a.Type == EArtifactType::StaticLibrary)
      libSet.insert(a.Name);
  }

  std::ofstream generatedFile(output);
  generatedFile << "# This is automatically generated file by cxx-pm\n";
  generatedFile << "# Package name: " << package.Name << '\n';
  generatedFile << "# Configurations: ";
  for (const auto &buildType: systemInfo.BuildType)
    generatedFile << buildType.Name << ';';
  generatedFile << "\n\n";

  // Include guard via GLOBAL property
  std::string guardProp = "_CXXPM_";
  for (char c : package.Name)
    guardProp.push_back(toupper(static_cast<unsigned char>(c)));
  guardProp.append("_INCLUDED");
  generatedFile << "get_property(__included GLOBAL PROPERTY " << guardProp << ")\n";
  generatedFile << "if(__included)\n";
  generatedFile << "  return()\n";
  generatedFile << "endif()\n";
  generatedFile << "set_property(GLOBAL PROPERTY " << guardProp << " TRUE)\n\n";

  if (!writeArtifactsCmake(generatedFile, artifacts, prefixes, systemInfo, libSet, libraryTargets))
    return false;

  return true;
}

bool cmakeExport(const CPackage &package,
                 const std::vector<CPackage> &dependencies,
                 const CxxPmSettings &globalSettings,
                 const CompilersArray &compilers,
                 const ToolsArray &tools,
                 const CSystemInfo &systemInfo,
                 const std::filesystem::path &output,
                 bool verbose)
{
  std::filesystem::path outputDir = output.parent_path();

  // Export each dependency into its own .cmake file
  std::vector<std::string> depLibraryTargets;
  for (const auto &dep : dependencies) {
    std::filesystem::path depOutput = outputDir / (dep.Name + ".cmake");
    std::vector<std::string> depLibs;
    if (!exportSinglePackage(dep, globalSettings, compilers, tools, systemInfo, depOutput, verbose, depLibs))
      return false;
    depLibraryTargets.insert(depLibraryTargets.end(), depLibs.begin(), depLibs.end());
  }

  // Export main package
  std::vector<std::string> mainLibraryTargets;
  if (!exportSinglePackage(package, globalSettings, compilers, tools, systemInfo, output, verbose, mainLibraryTargets))
    return false;

  // Append include() and INTERFACE_LINK_LIBRARIES to main package file
  std::ofstream generatedFile(output, std::ios::app);

  // Include dependency cmake files
  for (const auto &dep : dependencies) {
    generatedFile << "include(${CMAKE_CURRENT_LIST_DIR}/" << dep.Name << ".cmake)\n";
  }

  // Link main package libraries to dependency libraries
  if (!depLibraryTargets.empty()) {
    std::string depList;
    for (size_t i = 0; i < depLibraryTargets.size(); ++i) {
      if (i > 0) depList.push_back(';');
      depList.append(depLibraryTargets[i]);
    }

    for (const auto &lib : mainLibraryTargets) {
      generatedFile << "set_property(TARGET " << lib << " APPEND PROPERTY INTERFACE_LINK_LIBRARIES \"" << depList << "\")\n";
    }
  }

  return true;
}
