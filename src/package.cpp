#include "package.h"
#include "cxx-pm.h"
#include "os.h"
#include "sha3.h"
#include "strExtras.h"
#include "bs/autotools.h"
#include "bs/cmake.h"
#include "json/json11.hpp"
#include <thread>
#ifdef WIN32
#include "compilers/msvc.h"
#endif

EArtifactType artifactTypeFromString(std::string_view type)
{
  if (type == "include") return EArtifactType::IncludeDirectory;
  if (type == "static_lib") return EArtifactType::StaticLibrary;
  if (type == "shared_lib") return EArtifactType::SharedLibrary;
  if (type == "executable") return EArtifactType::Executable;
  if (type == "libset") return EArtifactType::LibSet;
  if (type == "cmake_module") return EArtifactType::CMakeModule;
  return EArtifactType::Unknown;
}

const char *artifactTypeToString(EArtifactType type)
{
  switch (type) {
    case EArtifactType::IncludeDirectory : return "include";
    case EArtifactType::StaticLibrary : return "static_lib";
    case EArtifactType::SharedLibrary : return "shared_lib";
    case EArtifactType::Executable : return "executable";
    case EArtifactType::LibSet : return "libset";
    case EArtifactType::CMakeModule : return "cmake_module";
    default: return "<unknown>";
  }
}



std::filesystem::path packagePrefix(const std::filesystem::path &cxxPmHome, const CPackage &package, const CompilersArray &compilers, const CSystemInfo &systemInfo, const std::string &buildType, bool verbose)
{
  if (package.IsBinary) {
    return cxxPmHome / "binary-packages" / (package.Name + "-" + package.Version);
  } else {
    // build toolchain id
    // ${ToolchainSystemName}
    // ${ToolchainSystemProcessor}
    // ${ToolchainCompiler1Id}
    // ...
    // ${ToolchainCompilerNId}
    // ${ToolchainBuildType}
    std::string toolchainString;
    std::string packageIdString;
    toolchainString.append(systemInfo.TargetSystemName);
    toolchainString.push_back('-');
    toolchainString.append(systemInfo.TargetSystemProcessor);

    packageIdString.append(package.Version);
    packageIdString.push_back('-');
    packageIdString.append(buildType);
    std::string previousId;
    for (const ELanguage lang: package.Languages) {
      const CCompilerInfo &info = compilers[static_cast<size_t>(lang)];
      if (info.Id != previousId) {
        toolchainString.push_back('-');
        toolchainString.append(info.Id);
      }
      previousId = info.Id;
    }

    std::string toolchainId = sha3StringHash(toolchainString).substr(0, 32);
    std::string packageId = sha3StringHash(packageIdString).substr(0, 32);

    if (verbose) {
      printf("toolchain: %s id: %s\n", toolchainString.c_str(), toolchainId.c_str());
      printf("package-id: %s id: %s\n", packageIdString.c_str(), packageId.c_str());
    }

    return cxxPmHome / toolchainId / package.Name / (package.Version + "-" + buildType + "-" + packageId);
  }
}

static void addEnv(std::vector<std::string> &env, const std::string &name, const std::string &value)
{
  env.emplace_back(name);
  env.back().push_back('=');
  env.back().append(value);
}

bool CArtifact::loadFromJson(const json11::Json &json)
{
  if (!json.is_object() ||
      !json["type"].is_string() ||
      !json["name"].is_string()) {
    fprintf(stderr, "ERROR: type and name fields must be strings\n");
    return false;
  }

  Type = artifactTypeFromString(json["type"].string_value());
  if (Type == EArtifactType::Unknown) {
    fprintf(stderr, "ERROR: invalid artifact type %s\n", json["type"].string_value().c_str());
    return false;
  }

  Name = json["name"].string_value();
  switch (Type) {
    case EArtifactType::IncludeDirectory :
    case EArtifactType::Executable :
    case EArtifactType::CMakeModule : {
      if (!json["path"].is_string()) {
        fprintf(stderr, "ERROR: path field must be a string\n");
        return false;
      }
      RelativePaths.emplace_back(json["path"].string_value());
      break;
    }

    case EArtifactType::SharedLibrary :
    case EArtifactType::StaticLibrary : {
      if (!json["path"].is_string()) {
        fprintf(stderr, "ERROR: path field must be a string\n");
        return false;
      }

      if (json.object_items().count("includes")) {
        if (!json["includes"].is_array()) {
          fprintf(stderr, "ERROR: includes must be array\n");
          return false;
        }

        for (const auto &dir: json["includes"].array_items()) {
          if (!dir.is_string()) {
            fprintf(stderr, "ERROR: include link must be a string");
            return false;
          }

          IncludeLinks.push_back(dir.string_value());
        }
      }

      std::vector<std::string> definitions;
      if (json.object_items().count("definitions")) {
        if (!json["definitions"].is_array()) {
          fprintf(stderr, "ERROR: definitions must be array\n");
          return false;
        }

        for (const auto &def: json["definitions"].array_items()) {
          if (!def.is_string()) {
            fprintf(stderr, "ERROR: definition link must be a string");
            return false;
          }

          definitions.push_back(def.string_value());
        }
      }

      Definitions.emplace_back(std::move(definitions));
      RelativePaths.emplace_back(json["path"].string_value());

      if (Type == EArtifactType::SharedLibrary) {
        if (!json["dll"].is_string()) {
          fprintf(stderr, "ERROR: dll field must be a string\n");
          return false;
        }

        if (!json["implib"].is_string()) {
          fprintf(stderr, "ERROR: implib field must be a string\n");
          return false;
        }

        DllPaths.emplace_back(json["dll"].string_value());
        ImplibPath.emplace_back(json["implib"].string_value());
      }
      break;
    }

    case EArtifactType::LibSet : {
      if (!json["libs"].is_array()) {
        fprintf(stderr, "ERROR: libs field must be array\n");
        return false;
      }
      for (const auto &lib: json["libs"].array_items()) {
        if (!lib.is_string()) {
          fprintf(stderr, "ERROR: elements of 'libs' field must be strings\n");
          return false;
        }

        Libs.emplace_back(lib.string_value());
      }
      break;
    }

    default:
      return false;
  }

  return true;
}

bool CArtifact::merge(const CArtifact &artifact)
{
  if (Type != artifact.Type ||
      Name != artifact.Name) {
    fprintf(stderr, "ERROR: can't merge artifacts: <%s/%s> and <%s/%s>\n", artifactTypeToString(Type), Name.c_str(), artifactTypeToString(artifact.Type), artifact.Name.c_str());
    return false;
  }

  switch (Type) {
    case EArtifactType::SharedLibrary :
    case EArtifactType::StaticLibrary : {
      if (artifact.RelativePaths.empty()) {
        fprintf(stderr, "ERROR: artifact %s/%s has empty relative paths\n", artifactTypeToString(artifact.Type), artifact.Name.c_str());
        return false;
      }

      if (artifact.RelativePaths.size() != 1 ||
          artifact.Definitions.size() != 1)
        return false;
      RelativePaths.emplace_back(artifact.RelativePaths[0]);
      Definitions.emplace_back(artifact.Definitions[0]);

      if (Type == EArtifactType::SharedLibrary) {
        if (artifact.DllPaths.size() != 1 ||
            artifact.ImplibPath.size() != 1)
          return false;
        DllPaths.emplace_back(artifact.DllPaths[0]);
        ImplibPath.emplace_back(artifact.ImplibPath[0]);
      }
      break;
    }

    case EArtifactType::IncludeDirectory :
    case EArtifactType::Executable :
    case EArtifactType::CMakeModule : {
      if (artifact.RelativePaths.empty()) {
        fprintf(stderr, "ERROR: artifact %s/%s has empty relative paths\n", artifactTypeToString(artifact.Type), artifact.Name.c_str());
        return false;
      }
      RelativePaths.emplace_back(artifact.RelativePaths[0]);
      break;
    }

    case EArtifactType::LibSet : {
      if (Libs != artifact.Libs) {
        fprintf(stderr, "ERROR: artifact %s libs mismatch in different configurations\n", Name.c_str());
        return false;
      }
      break;
    }

    default:
      return false;
  }

  return true;
}

void prepareBuildEnvironment(std::vector<std::string> &env,
                        const CPackage &package,
                        const CxxPmSettings &globalSettings,
                        const CSystemInfo &systemInfo,
                        const CompilersArray &compilers,
                        const ToolsArray &tools,
                        const std::string &buildType,
                        bool verbose)
{
  // Global settings
  std::string args = "--package-root=";
    args.append(globalSettings.PackageRoot.string());
  addEnv(env, "CXXPM_ARGS", args);

  addEnv(env, "CXXPM_NPROC", std::to_string(std::thread::hardware_concurrency()+1));

  // Toolchain settings
  addEnv(env, "CXXPM_EXECUTABLE", pathConvert(systemInfo.Self, EPathType::Posix).string());
  addEnv(env, "CXXPM_SYSTEM_NAME", systemInfo.TargetSystemName);
  addEnv(env, "CXXPM_SYSTEM_PROCESSOR", systemInfo.TargetSystemProcessor);
  addEnv(env, "CXXPM_BUILD_TYPE", buildType);
  addEnv(env, "CXXPM_SYSTEM_SUBTYPE", systemInfo.TargetSystemSubType);
  addEnv(env, "CXXPM_MSVC_TOOLSET", systemInfo.VSToolSetVersion);
  addEnv(env, "CXXPM_ISYSROOT", pathConvert(systemInfo.ISysRoot, EPathType::Posix).string());

  // Compilers
  auto compilerEnvName = [](ELanguage lang, const std::string &envName) -> std::string {
    std::string s("CXXPM_COMPILER_");
    s.append(languageToStringEnv(lang));
    s.push_back('_');
    s.append(envName);
    return s;
  };

  for (const ELanguage lang: package.Languages) {
    const CCompilerInfo &info = compilers[static_cast<size_t>(lang)];
    addEnv(env, compilerEnvName(lang, "COMMAND"), pathConvert(info.Command, EPathType::Posix).string());
    addEnv(env, compilerEnvName(lang, "TYPE"), compilerTypeToString(info.Type));
  }

  // Tools
  auto toolEnvName = [](EToolType tool, const std::string &envName) -> std::string {
    std::string s("CXXPM_TOOL_");
    s.append(toolTypeToStringEnv(tool));
    s.push_back('_');
    s.append(envName);
    return s;
  };

  for (size_t i = 1, ie = static_cast<size_t>(EToolType::NumberOf); i != ie; ++i) {
    const CToolInfo &info = tools[i];
    addEnv(env, toolEnvName(static_cast<EToolType>(i), "COMMAND"), pathConvert(info.Command, EPathType::Posix).string());
  }

  // Build systems
  // cmake
  std::string cmakeConfigureArgs = cmakeGetConfigureArgs(package, compilers, tools, systemInfo, buildType);
  std::string cmakeBuildArgs = cmakeGetBuildArgs(package, compilers, tools, systemInfo, buildType);
  addEnv(env, "CXXPM_CMAKE_CONFIGURE_ARGS", cmakeConfigureArgs);
  addEnv(env, "CXXPM_CMAKE_BUILD_ARGS", cmakeBuildArgs);
  // autotools
  addAutotoolsEnv(env, package, compilers, tools, systemInfo, buildType);
#ifdef WIN32
  addEnv(env, "CXXPM_MSVC_ARCH", getVsArch(systemInfo.TargetSystemProcessor));
#endif

  // Directories
  addEnv(env, "CXXPM_SOURCE_DIR", pathConvert(globalSettings.HomeDir / ".s", EPathType::Posix).string());
  addEnv(env, "CXXPM_BUILD_DIR", pathConvert(globalSettings.HomeDir / ".b", EPathType::Posix).string());
  addEnv(env, "CXXPM_INSTALL_DIR", pathConvert(package.Prefix / "install", EPathType::Posix).string());
  addEnv(env, "CXXPM_PACKAGE_DIR", pathConvert(package.BuildFile.parent_path(), EPathType::Posix).string());

  // Package settings
  addEnv(env, "CXXPM_PACKAGE_VERSION", package.Version);

  // Library prefix & suffix
  std::string libraryPrefix;
  std::string staticLibrarySuffix;
  std::string sharedLibrarySuffix;
  std::string executableSuffix;
  if (systemInfo.TargetSystemName == "Windows") {
    if (systemInfo.TargetSystemSubType == "msvc") {
      staticLibrarySuffix = ".lib";
      sharedLibrarySuffix = ".dll";
    } else if (startsWith(systemInfo.TargetSystemSubType, "mingw")) {
      libraryPrefix = "lib";
      staticLibrarySuffix = ".a";
      sharedLibrarySuffix = ".dll";
    } else if (systemInfo.TargetSystemSubType == "cygwin") {
      libraryPrefix = "lib";
      staticLibrarySuffix = ".a";
      sharedLibrarySuffix = ".so";
    }

    executableSuffix = ".exe";
  } else if (systemInfo.TargetSystemName == "Darwin") {
    libraryPrefix = "lib";
    staticLibrarySuffix = ".a";
    sharedLibrarySuffix = ".dylib";
  } else {
    libraryPrefix = "lib";
    staticLibrarySuffix = ".a";
    sharedLibrarySuffix = ".so";
  }

  addEnv(env, "CXXPM_LIBRARY_PREFIX", libraryPrefix);
  addEnv(env, "CXXPM_STATIC_LIBRARY_SUFFIX", staticLibrarySuffix);
  addEnv(env, "CXXPM_SHARED_LIBRARY_SUFFIX", sharedLibrarySuffix);
  addEnv(env, "CXXPM_EXECUTABLE_SUFFIX", executableSuffix);

  if (verbose) {
    for (const auto &e: env)
      printf("%s\n", e.c_str());
  }
}
