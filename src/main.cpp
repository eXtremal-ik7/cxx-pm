#include "cxx-pm-config.h"
#include "cxx-pm.h"
#include "exec.h"
#include "strExtras.h"
#include "compilers/common.h"
#include "bs/cmake.h"
#include "os.h"
#include "package.h"
#include "sha3Tools.h"
#include "ecdsa.h"
#include "manifest.h"
#include "version.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <vector>


enum CmdLineOptsTy {
  clOptHelp = 1,
  clOptCompilerCommand,
  clOptCompilerFlags,
  clOptCompilerDefinitions,
  clOptBuildType,
  clOptBuildTypeMapping,
  clOptVSInstallDir,
  clOptVCToolset,
  clOptCxxpmRoot,
  clOptUseFlags,
  clOptSystemName,
  clOptSystemProcessor,
  clOptISysRoot,
  clOptKey,
  clOptPackageList,
  clOptSearchPath,
  clOptSearchPathType,
  clOptInstall,
  clOptExportCMake,
  clOptPackageExtraDirectory,
  clOptFile,
  clOptVerbose,
  clOptVersion,
  clOptUpdate,
  clOptRepository
};

enum EModeTy {
  ENoMode = 0,
  EPackageList,
  ESearchPath,
  EInstall,
  EUpdate
};

static option cmdLineOpts[] = {
  {"compiler", required_argument, nullptr, clOptCompilerCommand},
  {"compiler-flags", required_argument, nullptr, clOptCompilerFlags},
  {"compiler-definitions", required_argument, nullptr, clOptCompilerDefinitions},
  {"use-flags", required_argument, nullptr, clOptUseFlags},
  {"system-name", required_argument, nullptr, clOptSystemName},
  {"system-processor", required_argument, nullptr, clOptSystemProcessor},
  {"isysroot", required_argument, nullptr, clOptISysRoot},
  {"build-type", required_argument, nullptr, clOptBuildType},
  {"build-type-mapping", required_argument, nullptr, clOptBuildTypeMapping},
  {"vs-install-dir", required_argument, nullptr, clOptVSInstallDir},
  {"vc-toolset", required_argument, nullptr, clOptVCToolset},
  {"cxxpm-root", required_argument, nullptr, clOptCxxpmRoot},
  // modes
  {"help", no_argument, nullptr, clOptHelp},
  {"package-list", optional_argument, nullptr, clOptPackageList},
  {"search-path", required_argument, nullptr, clOptSearchPath},
  {"search-path-type", required_argument, nullptr, clOptSearchPathType},
  {"install", required_argument, nullptr, clOptInstall},
  {"export-cmake", required_argument, nullptr, clOptExportCMake},
  {"version", no_argument, nullptr, clOptVersion},
  {"update", no_argument, nullptr, clOptUpdate},
  {"repository", required_argument, nullptr, clOptRepository},
  // extra parameters
  {"package-extra-dir", required_argument, nullptr, clOptPackageExtraDirectory},
  // arguments
  {"file", required_argument, nullptr, clOptFile},
  // other
  {"verbose", no_argument, nullptr, clOptVerbose},
  {nullptr, 0, nullptr, 0}
};

struct CContext {
  CxxPmSettings GlobalSettings;
  CSystemInfo SystemInfo;
  CompilersArray Compilers;
  ToolsArray Tools;
};

bool loadVariables(const std::filesystem::path &path, const std::vector<std::string> &names, std::vector<std::string> &variables)
{
  std::string capturedOut;
  std::string capturedErr;
  std::filesystem::path fullPath;
  std::string args;

  args = "set -e; source ";
  args.append(pathConvert(path, EPathType::Posix).string());
  args.append("; ");
  for (const auto &v: names) {
    args.append("echo \"$");
    args.append(v);
    args.append("\"@; ");
  }

  if (!run(path.parent_path(), "bash", {"-c", args}, {}, fullPath, capturedOut, capturedErr, true)) {
    if (!fullPath.empty())
      fprintf(stderr, "%s\n", capturedErr.c_str());
    return false;
  }

  StringSplitter splitter(capturedOut, "\r\n");
  while (splitter.next()) {
    auto v = splitter.get();
    if (v.back() != '@')
      continue;
    variables.emplace_back(v.begin(), v.end()-1);
  }

  return names.size() == variables.size();
}

bool loadSingleVariable(const std::filesystem::path &path, const std::string &name, std::string &variable)
{
  std::string capturedOut;
  std::string capturedErr;
  std::filesystem::path fullPath;
  std::string args;

  args = "set -e; source ";
  args.append(pathConvert(path, EPathType::Posix).string());
  args.append("; echo \"$");
  args.append(name);
  args.append("\"@;");

  if (!run(path.parent_path(), "bash", {"-c", args}, {}, fullPath, capturedOut, capturedErr, true)) {
    if (!fullPath.empty())
      fprintf(stderr, "%s\n", capturedErr.c_str());
    return false;
  }

  StringSplitter splitter(capturedOut, "\r\n");
  while (splitter.next()) {
    auto v = splitter.get();
    if (!v.empty() && v.back() == '@') {
      variable = std::string(v.begin(), v.end()-1);
      return true;
    }
  }

  return false;
}

std::vector<std::string> collectAvailableVersions(const CPackage &package)
{
  std::vector<std::string> versions;

  auto scanDir = [&versions](const std::filesystem::path &dir) {
    if (!std::filesystem::exists(dir))
      return;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      std::string filename = entry.path().filename().string();
      if (filename.size() > 6 && filename.substr(filename.size() - 6) == ".build") {
        std::string version = filename.substr(0, filename.size() - 6);
        if (version != "meta")
          versions.push_back(version);
      }
    }
  };

  scanDir(package.Path);
  for (const auto &extraPath : package.ExtraPath)
    scanDir(extraPath);

  return versions;
}

bool packageQueryVersion(CPackage &package, const std::string &requestedVersion, bool verbose)
{
  std::string version = requestedVersion;

  // Load default version from meta build
  if (version.empty() || version == "default") {
    if (!loadSingleVariable(package.Path / "meta.build", "DEFAULT_VERSION", version)) {
      fprintf(stderr, "ERROR: can't load DEFAULT_VERSION from %s\n", (package.Path / "meta.build").string().c_str());
      return false;
    }

    // Protect against recursion
    if (version == "default") {
      fprintf(stderr, "ERROR: DEFAULT_VERSION cannot be 'default' in %s\n", (package.Path / "meta.build").string().c_str());
      return false;
    }

    if (verbose)
      printf("Default version for %s is %s\n", package.Name.c_str(), version.c_str());
  }

  // Resolve wildcard version
  if (version.find('*') != std::string::npos) {
    std::vector<std::string> available = collectAvailableVersions(package);
    std::string bestVersion;

    for (const auto &v : available) {
      if (versionMatchesWildcard(v, version)) {
        if (bestVersion.empty() || compareVersions(v, bestVersion) > 0)
          bestVersion = v;
      }
    }

    if (bestVersion.empty()) {
      fprintf(stderr, "ERROR: no version matching %s found for package %s\n", version.c_str(), package.Name.c_str());
      return false;
    }

    if (verbose)
      printf("Selected version %s for pattern %s\n", bestVersion.c_str(), version.c_str());
    version = bestVersion;
  }

  package.Version = version;
  return true;
}

bool locatePackageBuildFile(CPackage &package)
{
  if (std::filesystem::exists(package.Path / (package.Version+".build"))) {
    package.BuildFile = package.Path / (package.Version+".build");
    return true;
  } else {
    for (const auto &extraPath: package.ExtraPath) {
      if (std::filesystem::exists(extraPath / (package.Version+".build"))) {
        package.BuildFile = extraPath / (package.Version+".build");
        return true;
      }
    }
  }

  return false;
}

bool inspectPackage(const CContext &context, CPackage &package, const std::string &requestedVersion, bool verbose)
{
  if (!packageQueryVersion(package, requestedVersion, verbose))
    return false;

  if (!locatePackageBuildFile(package)) {
    fprintf(stderr, "ERROR: package %s doen not contains build file for version %s\n", package.Name.c_str(), package.Version.c_str());
    return false;
  }

  // Query package compilers
  std::vector<std::string> variables;
  std::string packageTypeVariable;
  std::string compilersVariable;
  if (!loadVariables(package.BuildFile, { "PACKAGE_TYPE", "LANGS" }, variables)) {
    fprintf(stderr, "ERROR: can't load PACKAGE_TYPE, LANGS variables from %s\n", package.BuildFile.string().c_str());
    return false;
  }

  packageTypeVariable = variables[0];
  compilersVariable = variables[1];

  // Check package type
  if (packageTypeVariable.empty()) {
    fprintf(stderr, "ERROR: package type not specified in %s\n", package.BuildFile.string().c_str());
    return false;
  }

  if (packageTypeVariable == "binary") {
    package.IsBinary = true;
    return true;
  } else if (packageTypeVariable == "source") {
    package.IsBinary = false;

    StringSplitter splitter(compilersVariable, ",");
    while (splitter.next()) {
      auto langS = splitter.get();
      ELanguage lang = languageFromString(langS);
      if (lang == ELanguage::Unknown) {
        fprintf(stderr, "ERROR: unsupported language %s\n", langS.data());
        return false;
      }
      package.Languages.push_back(lang);
    }

    if (package.Languages.empty()) {
      fprintf(stderr, "ERROR: compilers not specified at %s\n", package.BuildFile.string().c_str());
      return false;
    }

    return true;
  } else {
    fprintf(stderr, "ERROR: package type can be 'source' or 'binary', %s found\n", packageTypeVariable.c_str());
    return false;
  }
}

void updatePackagePrefix(const CContext &context, CPackage &package, const std::string &buildType, bool verbose)
{
  package.Prefix = packagePrefix(context.GlobalSettings.HomeDir, package, context.Compilers, context.SystemInfo, buildType, verbose);
}

void createManifestForDirectory(FILE *hLog, const std::filesystem::path &directory, const std::filesystem::path &relativePath)
{
  for(const auto &element: std::filesystem::directory_iterator{directory}) {
    if (element.is_directory()) {
      createManifestForDirectory(hLog, element, relativePath / element.path().filename());
    } else {
      std::string hash = sha3FileHash(element);
      fprintf(hLog, "%s!%s\n", (relativePath / element.path().filename()).string().c_str(), hash.c_str());
    }
  }
}

bool downloadPackageFiles(const CContext& context,
                          const CPackage& package,
                          const std::filesystem::path &sourceDir,
                          const std::filesystem::path &binaryInstallDir)
{
  std::string type;
  std::string url;
  std::string sha3;
  std::string tag;
  std::string commit;
  std::filesystem::path destination;
  if (!package.IsBinary) {
    std::vector<std::string> variables;
    if (!loadVariables(package.BuildFile, { "TYPE", "URL", "SHA3", "TAG", "COMMIT" }, variables)) {
      fprintf(stderr, "ERROR, can't load TYPE, URL, SHA3, TAG, COMMIT from %s\n", package.BuildFile.string().c_str());
      return false;
    }

    type = std::move(variables[0]);
    url = std::move(variables[1]);
    sha3 = std::move(variables[2]);
    tag = std::move(variables[3]);
    commit = std::move(variables[4]);
    destination = sourceDir;
  } else {
    std::vector<std::string> variableNames;
    std::vector<std::string> variables;
    std::string namePrefix = context.SystemInfo.HostSystemName + "_" + context.SystemInfo.HostSystemProcessor + "_";
    for (const auto &name : { "TYPE", "URL", "SHA3", "TAG", "COMMIT" })
      variableNames.emplace_back(namePrefix + name);

    if (!loadVariables(package.BuildFile, variableNames, variables)) {
      fprintf(stderr, "ERROR, can't load TYPE, URL, SHA3, TAG, COMMIT from %s\n", package.BuildFile.string().c_str());
      return false;
    }

    type = std::move(variables[0]);
    url = std::move(variables[1]);
    sha3 = std::move(variables[2]);
    destination = binaryInstallDir;
  }

  printf("Downloading package %s:%s\n", package.Name.c_str(), package.Version.c_str());
  if (type == "archive") {
    if (url.empty()) {
      fprintf(stderr, "ERROR: URL must be specified for 'archive'\n");
      return false;
    }
    if (sha3.size() != 64) {
      fprintf(stderr, "ERROR: SHA3 256 bit hash must be specified for 'archive'\n");
      return false;
    }

    // get file name from url
    size_t pos = 0;
    size_t nextPos;
    while ((nextPos = url.find('/', pos)) != url.npos)
      pos = nextPos + 1;
    if (url.size() - pos < 2) {
      fprintf(stderr, "ERROR: invalid url: %s\n", url.c_str());
      return false;
    }
    std::filesystem::path archiveFilePath = context.GlobalSettings.DistrDir / (url.data() + pos);

    // Check presence & hash
    bool fileExists = false;
    if (std::filesystem::exists(archiveFilePath)) {
      std::string existingHash = sha3FileHash(archiveFilePath);
      if (existingHash.empty()) {
        fprintf(stderr, "ERROR: can't calculate SHA3 hash of %s\n", archiveFilePath.string().c_str());
        return false;
      }

      if (existingHash == sha3) {
        printf("Archive %s already exists\n", archiveFilePath.string().c_str());
        fileExists = true;
      }
      else {
        fprintf(stderr, "SHA3 mismatch: sha3(%s)=%s, required %s\n", archiveFilePath.string().c_str(), existingHash.c_str(), sha3.c_str());
        if (!std::filesystem::remove(archiveFilePath)) {
          fprintf(stderr, "ERROR: can't delete file %s\n", archiveFilePath.string().c_str());
          return false;
        }
      }
    }

    if (!fileExists) {
      // Downloading file
      if (!runNoCapture(".", "wget", { url, "-O", archiveFilePath.string() }, {}, true)) {
        fprintf(stderr, "Can't download file %s\n", url.c_str());
        return false;
      }

      std::string downloadedHash = sha3FileHash(archiveFilePath);
      if (downloadedHash != sha3) {
        fprintf(stderr, "SHA3 mismatch: sha3(%s)=%s, required %s\n", archiveFilePath.string().c_str(), downloadedHash.c_str(), sha3.c_str());
        return false;
      }
    }

    // Unpacking file
    // Detect archive type
    auto archiveFilePathPosix = pathConvert(archiveFilePath, EPathType::Posix);
    auto destinationPosix = pathConvert(destination, EPathType::Posix);

    if (endsWith(archiveFilePathPosix.string(), ".zip")) {
      if (!runNoCapture(".", "unzip", { archiveFilePathPosix.string(), "-d", destinationPosix.string()}, {}, true)) {
        fprintf(stderr, "Unpacking error\n");
        return false;
      }
    } else if (endsWith(archiveFilePathPosix.string(), ".tar.gz")) {
      if (!runNoCapture(".", "tar", { "-xzf", archiveFilePathPosix.string(), "-C", destinationPosix.string() }, {}, true)) {
        fprintf(stderr, "Unpacking error\n");
        return false;
      }
    } else if (endsWith(archiveFilePathPosix.string(), ".tar.bz2")) {
      if (!runNoCapture(".", "tar", { "-xjf", archiveFilePathPosix.string(), "-C", destinationPosix.string() }, {}, true)) {
        fprintf(stderr, "Unpacking error\n");
        return false;
      }
    } else if (endsWith(archiveFilePathPosix.string(), ".tar.lz") || endsWith(archiveFilePathPosix.string(), ".tar.lzma")) {
        if (!runNoCapture(".", "tar", { "--lzip", "-xvf", archiveFilePathPosix.string(), "-C", destinationPosix.string() }, {}, true)) {
          fprintf(stderr, "Unpacking error\n");
          return false;
        }
    } else if (endsWith(archiveFilePathPosix.string(), ".tar.zst")) {
      std::string tmpFileName = archiveFilePathPosix.filename().string();
      std::filesystem::path tmpFilePath = context.GlobalSettings.DistrDir / ("tmp-" + tmpFileName.substr(0, tmpFileName.size() - 4));
      std::filesystem::path tmpFilePathPosix = pathConvert(tmpFilePath, EPathType::Posix);
      bool success = true;
      if (!runNoCapture(".", "unzstd", { archiveFilePathPosix.string(), "-o", tmpFilePathPosix.string() }, {}, true) ||
          !runNoCapture(".", "tar", { "-xf", tmpFilePathPosix.string(), "-C", destinationPosix.string() }, {}, true))
        success = false;
      std::error_code ec;
      std::filesystem::remove(tmpFilePath, ec);
      if (!success) {
        fprintf(stderr, "Unpacking error\n");
        return false;
      }
    } else {
      fprintf(stderr, "Unknown archive file: %s\n", archiveFilePath.string().c_str());
      return false;

    }
  } else if (type == "git") {
    std::vector<std::string> gitArgs = {"clone", url, "."};
    if (!tag.empty()) {
      gitArgs.emplace_back("-b");
      gitArgs.emplace_back(tag);
    }

    if (!runNoCapture(destination, "git", gitArgs, {}, true)) {
      fprintf(stderr, "git clone error url: %s tag: %s\n", url.c_str(), tag.c_str());
      return false;
    }

    if (!commit.empty()) {
      if (!runNoCapture(destination, "git", {"reset", "--hard", commit}, {}, true)) {
        fprintf(stderr, "git reset hard error commit: %s\n", commit.c_str());
      }
    }

    return true;
  } else {
    fprintf(stderr, "ERROR: unsupported type: %s\n", type.c_str());
    return false;
  }

  return true;
}

static bool removeDirectory(const std::filesystem::path &path)
{
  std::error_code ec;
  if (std::filesystem::exists(path) && (!std::filesystem::remove_all(path, ec) || ec != std::error_code()))  {
#ifdef WIN32
    // Windows implementation of std::filesystem::remove_all contains a bug
    if (!runNoCapture(".", "rm", { "-rf", path.string() }, {}, true) || std::filesystem::exists(path)) {
      fprintf(stderr, "ERROR: can't delete folder %s\n", path.string().c_str());
      fprintf(stderr, "%s\n", ec.message().c_str());
      return false;
    }
#else
    fprintf(stderr, "ERROR: can't delete folder %s\n", path.string().c_str());
    return false;
#endif
  }

  return true;
}

bool install(CContext &context, std::map<std::string, CPackage> &allPackages, CPackage &package, const std::string &buildType, bool verbose, const std::filesystem::path &externalPrefix="")
{
  printf("Installing package %s (%s) to %s\n", package.Name.c_str(), buildType.c_str(), package.Prefix.string().c_str());

  // Downloading
  // Load package source information
  // Get distr type and url
  std::string type;
  std::string url;
  std::string sha3;
  {
    std::vector<std::string> variables;
    if (!loadVariables(package.BuildFile, {"TYPE", "URL", "SHA3", "TAG", "COMMIT"}, variables)) {
      fprintf(stderr, "ERROR, can't load DISTR_TYPE, DISTR_URL and DISTR_SHA3 from %s\n", package.BuildFile.string().c_str());
      return false;
    }

    type = std::move(variables[0]);
    url = std::move(variables[1]);
    sha3 = std::move(variables[2]);
  }

  std::filesystem::path sourceDir = context.GlobalSettings.HomeDir / ".s";
  std::filesystem::path buildDir = context.GlobalSettings.HomeDir / ".b";
  std::filesystem::path installDir = externalPrefix.empty() ? package.Prefix / "install" : externalPrefix / "install";
  bool installDirNeedCreate = externalPrefix.empty();

  // Check for already installed
  {
    std::ifstream hManifest(package.Prefix / "manifest.txt");
    if (hManifest) {
      //char *line = nullptr;
      size_t length = 0;
      //ssize_t nRead;
      auto beginPt = std::chrono::steady_clock::now();
      unsigned count = 0;
      uint64_t ms = 0;
      bool packageInstalled = true;
      bool allFilesChecked = true;
      std::string line;
      while (std::getline(hManifest, line)) {
        size_t pos = line.find('!');
        if (pos == std::string::npos || pos == 0 || line.size()-pos < 64) {
          fprintf(stderr, "WARNING: broken manifest %s\n", (package.Prefix / "manifest.txt").string().c_str());
          packageInstalled = false;
          break;
        }

        // get next file hash
        std::string relativePath = line.substr(0, pos);
        std::string expectedHash = line.substr(pos+1, 64);
        std::string hash = sha3FileHash(installDir / relativePath);
        if (hash.empty()) {
          fprintf(stderr, "WARNING: can't read package file %s\n", (installDir / relativePath).string().c_str());
          packageInstalled = false;
          break;
        }

        if (hash != expectedHash) {
          fprintf(stderr, "WARNING: file %s corrupted, need reinstall\n", (installDir / relativePath).string().c_str());
          packageInstalled = false;
          break;
        }

        count++;
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beginPt).count();
        if (ms >= 125) {
          allFilesChecked = false;
          break;
        }
      }

      if (packageInstalled) {
        printf("Verified %u%s files in %u milliseconds\n", count, allFilesChecked ? "(all!)" : "", static_cast<unsigned>(ms));
        printf("Package %s seems to be already installed\n", package.Name.c_str());
        return true;
      }
    }
  }

  if (!removeDirectory(package.Prefix))
    return false;

  if (!package.IsBinary) {
    if (!removeDirectory(sourceDir))
      return false;
    if (!std::filesystem::create_directories(sourceDir)) {
      fprintf(stderr, "ERROR: can't create directory at %s\n", sourceDir.string().c_str());
      return false;
    }

    if (!removeDirectory(buildDir))
      return false;
    if (!std::filesystem::create_directories(buildDir)) {
      fprintf(stderr, "ERROR: can't create directory at %s\n", buildDir.string().c_str());
      return false;
    }
  }

  if (installDirNeedCreate && !std::filesystem::create_directories(installDir)) {
    fprintf(stderr, "ERROR: can't create directory at %s\n", installDir.string().c_str());
    return false;
  }

  // Install depends
  {
    std::string dependsVariable;
    if (loadSingleVariable(package.BuildFile, "DEPENDS", dependsVariable) && !dependsVariable.empty()) {
      StringSplitter splitter(dependsVariable, "\r\n ");
      while (splitter.next()) {
        std::string d(splitter.get());

        // Parse package@version format
        std::string dependName;
        std::string dependVersion;
        size_t atPos = d.find('@');
        if (atPos != std::string::npos) {
          dependName = d.substr(0, atPos);
          dependVersion = d.substr(atPos + 1);
        } else {
          dependName = d;
        }

        // search package
        auto It = allPackages.find(dependName);
        if (It == allPackages.end()) {
          fprintf(stderr, "ERROR: %s depends on non-existent package %s\n", package.Name.c_str(), dependName.c_str());
          return false;
        }

        auto &dependPackage = It->second;
        if (!inspectPackage(context, dependPackage, dependVersion, verbose))
          return false;
        if (!searchCompilers(dependPackage.Languages, context.Compilers, context.Tools, context.SystemInfo, verbose))
          return false;
        updatePackagePrefix(context, dependPackage, buildType, verbose);

        if (!install(context, allPackages, dependPackage, buildType, verbose))
          return false;
      }

      // Recreate source/build directories after installing dependencies
      // (they may have been cleaned up by dependency builds)
      if (!package.IsBinary) {
        if (!std::filesystem::exists(sourceDir) && !std::filesystem::create_directories(sourceDir)) {
          fprintf(stderr, "ERROR: can't create directory at %s\n", sourceDir.string().c_str());
          return false;
        }
        if (!std::filesystem::exists(buildDir) && !std::filesystem::create_directories(buildDir)) {
          fprintf(stderr, "ERROR: can't create directory at %s\n", buildDir.string().c_str());
          return false;
        }
      }
    }
  }

  // Install binary depends (into the same install directory as this package)
  {
    std::string dependsBinaryVariable;
    if (loadSingleVariable(package.BuildFile, "DEPENDS_BINARY", dependsBinaryVariable) && !dependsBinaryVariable.empty()) {
      StringSplitter splitter(dependsBinaryVariable, "\r\n ");
      while (splitter.next()) {
        std::string d(splitter.get());

        // Parse package@version format
        std::string dependName;
        std::string dependVersion;
        size_t atPos = d.find('@');
        if (atPos != std::string::npos) {
          dependName = d.substr(0, atPos);
          dependVersion = d.substr(atPos + 1);
        } else {
          dependName = d;
        }

        // search package
        auto It = allPackages.find(dependName);
        if (It == allPackages.end()) {
          fprintf(stderr, "ERROR: %s depends on non-existent package %s\n", package.Name.c_str(), dependName.c_str());
          return false;
        }

        auto &dependPackage = It->second;
        if (!inspectPackage(context, dependPackage, dependVersion, verbose))
          return false;
        if (!searchCompilers(dependPackage.Languages, context.Compilers, context.Tools, context.SystemInfo, verbose))
          return false;
        updatePackagePrefix(context, dependPackage, buildType, verbose);

        if (!install(context, allPackages, dependPackage, buildType, verbose, package.Prefix))
          return false;
      }
    }
  }

  if (!downloadPackageFiles(context, package, sourceDir, installDir))
    return false;

  if (!package.IsBinary) {
    // Build
    // Prepare environment
    std::vector<std::string> env;
    prepareBuildEnvironment(env, package, context.GlobalSettings, context.SystemInfo, context.Compilers, context.Tools, buildType, verbose);

    // Run building
    printf("Build %s\n", package.Name.c_str());
    FILE* hLog = fopen((package.Prefix / "build.log").string().c_str(), "w+");
    if (!hLog) {
      fprintf(stderr, "Can't open log file %s\n", (package.Prefix / "build.log").string().c_str());
      return false;
    }

    std::string args;
    args = "set -x; set -e; source ";
    args.append(pathConvert(package.BuildFile, EPathType::Posix).string());
    args.append("; build;");
    if (!runCaptureLog(package.BuildFile.parent_path(), "bash", { "-c", args }, env, hLog, true)) {
      fprintf(hLog, "Build command for %s failed\n", package.Name.c_str());
      fprintf(stderr, "Build command for %s failed\n", package.Name.c_str());
      fclose(hLog);
      return false;
    }

    fclose(hLog);
  }

  if (externalPrefix.empty()) {
    // Create manifest
    FILE* hManifest = fopen((package.Prefix / "manifest.txt").string().c_str(), "w+");
    if (!hManifest) {
      fprintf(stderr, "Can't open manifest file %s\n", (package.Prefix / "manifest.txt").string().c_str());
      return false;
    }

    printf("Create manifest...\n");
    createManifestForDirectory(hManifest, installDir, "");
    fclose(hManifest);
  }

  // Cleanup
  if (!package.IsBinary) {
    printf("Cleanup...\n");
    if (!removeDirectory(sourceDir) || !removeDirectory(buildDir))
      return false;
  }

  return true;
}

std::filesystem::path searchPath(const std::filesystem::path& prefix, const std::filesystem::path &name)
{
  std::filesystem::path result;
  std::ifstream hManifest(prefix / "manifest.txt");
  if (hManifest) {
    size_t length = 0;
    std::string line;
    while (std::getline(hManifest, line)) {
      size_t pos = line.find('!');
      if (pos == std::string::npos || pos == 0 || line.size() - pos < 64) {
        fprintf(stderr, "WARNING: broken manifest %s\n", (prefix / "manifest.txt").string().c_str());
        return std::filesystem::path();
      }

      // get next file hash
      std::string relativePath = line.substr(0, pos);
      if (endsWith(relativePath, name.string())) {
        if (!result.empty()) {
          fprintf(stderr, "ERROR: more than one file in package\n");
          return std::filesystem::path();
        }

        result = prefix / "install" / relativePath;
      }
    }
  } else {
    fprintf(stderr, "ERROR: manifest not found, package not installed\n");
    return std::filesystem::path();
  }

  return result;
}



void packageList(const std::filesystem::path &cxxpmRoot,
                 const std::vector<std::filesystem::path> &extraPackageDirs,
                 const std::map<std::string, CPackage> &packages)
{
  puts("Package repositories:");
  printf("  %s\n", (cxxpmRoot / "packages").string().c_str());
  for (const auto &dir : extraPackageDirs)
    printf("  %s\n", dir.string().c_str());

  puts("\nAvailable packages:");
  for (const auto &[name, package] : packages)
    printf("  %s\n", name.c_str());
}

void packageVersionList(const CPackage &package)
{
  std::vector<std::string> versions = collectAvailableVersions(package);

  // Sort versions
  std::sort(versions.begin(), versions.end(), [](const std::string &a, const std::string &b) {
    return compareVersions(a, b) > 0;
  });

  // Load default version
  std::string defaultVersion;
  if (!loadSingleVariable(package.Path / "meta.build", "DEFAULT_VERSION", defaultVersion))
    defaultVersion = "(unknown)";

  printf("Package: %s\n", package.Name.c_str());
  printf("Default version: %s\n", defaultVersion.c_str());
  puts("\nAvailable versions:");
  for (const auto &v : versions)
    printf("  %s\n", v.c_str());
}

bool verifyManifest(const std::filesystem::path &dir)
{
  std::filesystem::path manifestPath = dir / MANIFEST_FILENAME;
  std::filesystem::path signPath = dir / SIGN_FILENAME;

  if (!std::filesystem::exists(manifestPath)) {
    fprintf(stderr, "ERROR: MANIFEST not found in %s\n", dir.string().c_str());
    return false;
  }

  // Verify signature
  if (std::filesystem::exists(signPath)) {
    std::string manifestHash = sha3FileHash(manifestPath);
    if (manifestHash.empty()) {
      fprintf(stderr, "ERROR: failed to compute manifest hash\n");
      return false;
    }

    std::ifstream signFile(signPath);
    std::string signature;
    std::getline(signFile, signature);

    uint8_t hashBytes[32];
    hex2bin(manifestHash.c_str(), 64, hashBytes);

    if (!ecdsaVerify(hashBytes, signature)) {
      fprintf(stderr, "ERROR: invalid manifest signature\n");
      return false;
    }
    puts("Signature verified");
  } else {
    puts("WARNING: manifest is not signed (SIGN file missing)");
  }

  // Read manifest
  std::map<std::string, std::string> expectedEntries;
  std::ifstream manifest(manifestPath);
  std::string line;

  while (std::getline(manifest, line)) {
    size_t spacePos = line.find(' ');
    if (spacePos == std::string::npos || spacePos == 0) {
      fprintf(stderr, "ERROR: invalid MANIFEST format\n");
      return false;
    }
    std::string name = line.substr(0, spacePos);
    std::string hash = line.substr(spacePos + 1);
    expectedEntries[name] = hash;
  }

  // Collect actual entries (excluding .git, MANIFEST, SIGN)
  std::set<std::string> actualEntries;
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    if (name == ".git" || name == MANIFEST_FILENAME || name == SIGN_FILENAME)
      continue;
    actualEntries.insert(name);
  }

  // Check for extra files
  bool success = true;
  for (const auto &name : actualEntries) {
    if (expectedEntries.find(name) == expectedEntries.end()) {
      fprintf(stderr, "ERROR: unexpected file/directory: %s\n", name.c_str());
      success = false;
    }
  }

  // Check for missing files and verify hashes
  for (const auto &[name, expectedHash] : expectedEntries) {
    std::filesystem::path entryPath = dir / name;
    if (!std::filesystem::exists(entryPath)) {
      fprintf(stderr, "ERROR: missing: %s\n", name.c_str());
      success = false;
      continue;
    }

    std::string actualHash;
    if (std::filesystem::is_directory(entryPath))
      actualHash = sha3DirectoryHash(entryPath, MANIFEST_FILENAME);
    else
      actualHash = sha3FileHash(entryPath);

    if (actualHash != expectedHash) {
      fprintf(stderr, "ERROR: hash mismatch for %s\n", name.c_str());
      fprintf(stderr, "  expected: %s\n", expectedHash.c_str());
      fprintf(stderr, "  actual:   %s\n", actualHash.c_str());
      success = false;
    }
  }

  if (success)
    puts("Manifest verification passed");

  return success;
}

bool updateRepository(const std::filesystem::path &cxxpmRoot, const std::string &expectedRepository)
{
  std::filesystem::path packagesDir = cxxpmRoot / "packages";
  std::filesystem::path gitDir = packagesDir / ".git";

  // Create packages directory if it doesn't exist
  if (!std::filesystem::exists(packagesDir)) {
    printf("Creating directory %s\n", packagesDir.string().c_str());
    if (!std::filesystem::create_directories(packagesDir)) {
      fprintf(stderr, "ERROR: failed to create directory %s\n", packagesDir.string().c_str());
      return false;
    }
  }

  // Clone if .git doesn't exist
  if (!std::filesystem::exists(gitDir)) {
    // Check if directory is not empty
    if (!std::filesystem::is_empty(packagesDir)) {
      fprintf(stderr, "ERROR: %s is not empty but has no .git directory\n", packagesDir.string().c_str());
      fprintf(stderr, "Please remove the directory and try again:\n");
      fprintf(stderr, "  rm -rf %s\n", packagesDir.string().c_str());
      return false;
    }
    printf("Cloning repository to %s\n", packagesDir.string().c_str());
    if (!runNoCapture(packagesDir, "git", {"clone", expectedRepository, "."}, {}, true, true))
      return false;
    return verifyManifest(packagesDir);
  }

  // Update existing repository
  printf("Updating repository at %s\n", packagesDir.string().c_str());

  // Check current remote URL
  std::filesystem::path fullPath;
  std::string capturedOut;
  std::string capturedErr;

  if (!run(packagesDir, "git", {"remote", "get-url", "origin"}, {}, fullPath, capturedOut, capturedErr, true, true)) {
    fprintf(stderr, "ERROR: failed to get remote URL\n");
    if (!capturedErr.empty())
      fprintf(stderr, "%s", capturedErr.c_str());
    return false;
  }

  // Remove trailing newline
  while (!capturedOut.empty() && (capturedOut.back() == '\n' || capturedOut.back() == '\r'))
    capturedOut.pop_back();

  printf("%s\n", capturedOut.c_str());

  if (capturedOut != expectedRepository) {
    fprintf(stderr, "ERROR: repository mismatch\n");
    fprintf(stderr, "  current:  %s\n", capturedOut.c_str());
    fprintf(stderr, "  expected: %s\n", expectedRepository.c_str());
    return false;
  }

  if (!runNoCapture(packagesDir, "git", {"pull"}, {}, true, true))
    return false;
  return verifyManifest(packagesDir);
}

void printHelp()
{
  puts("Usage: cxx-pm [options]");
  puts("Options:");
  puts("  --help\t\t\tShow this help message");
  puts("  --version\t\t\tShow version");
  puts("  --verbose\t\t\tEnable verbose output");
  puts("Modes:");
  puts("  --package-list [package]\tList available packages or versions");
  puts("  --install <package[@version]>\tInstall a package");
  puts("  --search-path <package[@version]>");
  puts("  \t\t\t\tGet package install path");
  puts("  --update\t\t\tUpdate package repository");
  puts("  --repository <url>\t\tRepository URL (for --update)");
  puts("Compiler options:");
  puts("  --compiler <lang:path>\tSet compiler path (e.g., cxx:/usr/bin/g++)");
  puts("  --compiler-flags <lang:flags>");
  puts("  --compiler-definitions <lang:defs>");
  puts("  --use-flags <flags>");
  puts("Build options:");
  puts("  --build-type <type>\t\tBuild type (default: Release)");
  puts("  --build-type-mapping <mapping>");
  puts("  --system-name <name>\t\tTarget system name");
  puts("  --system-processor <arch>\tTarget processor architecture");
  puts("  --isysroot <path>\t\tSystem root path");
  puts("Package options:");
  puts("  --package-extra-dir <dir>\tAdditional package directory");
  puts("  --export-cmake <path>\t\tExport CMake config");
  puts("  --search-path-type <type>\tPath type (native, posix, windows)");
  puts("  --file <name>\t\t\tSearch for file in package");
  puts("Other:");
  puts("  --cxxpm-root <path>\t\tSet cxx-pm root directory");
  puts("  --vs-install-dir <path>\tVisual Studio install directory");
  puts("  --vc-toolset <toolset>\tVC toolset version");
}

#ifdef WIN32
BOOL WINAPI ctrlHandler(DWORD dwCtrlType)
{
  terminateAllChildProcess();
  return FALSE;
}
#endif

int main(int argc, char **argv)
{
  {
    // some utils like cmake makes stdout buffer too much, that disables realtime output
    // fix it with setvbuf
    static char buffer[256];
    setvbuf(stdout, buffer, _IOLBF, sizeof(buffer));
  }

  std::filesystem::path cxxpmRoot;
  std::vector<std::filesystem::path> extraPackageDirs;
  EModeTy mode = ENoMode;
  std::string packageName;
  std::string packageVersion;
  std::string toolchainSystemName;
  std::string toolchainSystemProcessor;
  std::filesystem::path isysRoot;
  std::string buildType = "Release";
  std::string buildTypeMapping = "Debug:Debug;*:Release";
  std::string fileArgument;
  std::filesystem::path outputPath;
  bool exportCmake = false;
  bool verbose = false;
  EPathType pathType = EPathType::Native;
  std::string repository = "https://github.com/eXtremal-ik7/cxx-pm-repo";
  CContext context;

#ifdef WIN32
  SetConsoleCtrlHandler(ctrlHandler, TRUE);
#endif

  int res;
  int index = 0;
  while ((res = getopt_long(argc, argv, "", cmdLineOpts, &index)) != -1) {
    switch (res) {
      case clOptCxxpmRoot: {
        cxxpmRoot = optarg;
        break;
      }
      // compilers
      case clOptCompilerCommand : {
        if (!parseCompilerOption(ECompilerOptionType::Command, context.Compilers, optarg))
          return 1;
        break;
      }
      case clOptCompilerFlags: {
        if (!parseCompilerOption(ECompilerOptionType::Flags, context.Compilers, optarg))
          return 1;
        break;
      }
      case clOptCompilerDefinitions: {
        if (!parseCompilerOption(ECompilerOptionType::Definitions, context.Compilers, optarg))
          return 1;
        break;
      }
      case clOptSystemName :
        toolchainSystemName = optarg;
        break;
      case clOptSystemProcessor :
        toolchainSystemProcessor = optarg;
        break;
      case clOptISysRoot :
        isysRoot = optarg;
        break;
      case clOptBuildType :
        buildType = optarg;
        break;
      case clOptBuildTypeMapping :
        buildTypeMapping = optarg;
        break;
      case clOptVSInstallDir :
        context.SystemInfo.VCInstallDir = optarg;
        break;
      case clOptVCToolset :
        context.SystemInfo.VCToolSet = optarg;
        break;
      // modes
      case clOptHelp :
        printHelp();
        exit(0);
      case clOptVersion :
        printf("%s\n", CXXPM_VERSION);
        exit(0);
      case clOptPackageList :
        if (mode != ENoMode) {
          fprintf(stderr, "ERROR: mode already specified\n");
          exit(1);
        }
        mode = EPackageList;
        if (optarg)
          packageName = optarg;
        break;
      case clOptSearchPath :
        if (mode != ENoMode) {
          fprintf(stderr, "ERROR: mode already specified\n");
          exit(1);
        }
        mode = ESearchPath;
        {
          std::string arg = optarg;
          size_t atPos = arg.find('@');
          if (atPos != std::string::npos) {
            packageName = arg.substr(0, atPos);
            packageVersion = arg.substr(atPos + 1);
          } else {
            packageName = arg;
          }
        }
        break;
      case clOptSearchPathType : {
        pathType = pathTypeFromString(optarg);
        if (pathType == EPathType::Unknown) {
          fprintf(stderr, "ERROR: unknown path type: %s\n", optarg);
          return 1;
        }
        break;
      }
      case clOptInstall : {
        if (mode != ENoMode) {
          fprintf(stderr, "ERROR: mode already specified\n");
          exit(1);
        }
        mode = EInstall;
        {
          std::string arg = optarg;
          size_t atPos = arg.find('@');
          if (atPos != std::string::npos) {
            packageName = arg.substr(0, atPos);
            packageVersion = arg.substr(atPos + 1);
          } else {
            packageName = arg;
          }
        }
        break;
      }
      case clOptUpdate : {
        if (mode != ENoMode) {
          fprintf(stderr, "ERROR: mode already specified\n");
          exit(1);
        }
        mode = EUpdate;
        break;
      }
      case clOptRepository :
        repository = optarg;
        break;
      case clOptExportCMake : {
        exportCmake = true;
        outputPath = optarg;
        break;
      }
      case clOptPackageExtraDirectory :
        extraPackageDirs.push_back(optarg);
        break;
      case clOptFile :
        fileArgument = optarg;
        break;
      case clOptVerbose :
        verbose = true;
        break;
      case ':' :
        fprintf(stderr, "Error: option %s missing argument\n", cmdLineOpts[index].name);
        break;
      case '?' :
        exit(1);
      default :
        break;
    }
  }

  context.SystemInfo.Self = whereami(argv[0]);
  if (context.SystemInfo.Self.empty()) {
    fprintf(stderr, "ERROR: can't find self cxx-pm executable\n");
    return 1;
  }
#ifdef WIN32
  // Search msys2 bundle
  std::filesystem::path bashPath = context.SystemInfo.Self.parent_path() / "usr" / "bin" / "bash.exe";
  if (!std::filesystem::exists(bashPath)) {
    bashPath = userHomeDir() / ".cxxpm" / "self" / "usr" / "bin" / "bash.exe";
    if (!std::filesystem::exists(bashPath)) {
      fprintf(stderr, "ERROR: msys2 bundle not found, installation error\n");
      return 1;
    }
  }

  // Add msys2 bin directory to path
  context.SystemInfo.MSys2Path = bashPath.parent_path();
  DWORD size = GetEnvironmentVariableW(L"PATH", NULL, 0);
  std::wstring path;
  path.resize(size - 1);
  GetEnvironmentVariableW(L"PATH", path.data(), size);
  path.push_back(';');
  path.append(bashPath.parent_path().c_str());
  SetEnvironmentVariableW(L"PATH", path.c_str());
  updatePath();
#endif

  if (cxxpmRoot.empty())
    cxxpmRoot = userHomeDir() / ".cxxpm" / "self";

  if (!std::filesystem::exists(cxxpmRoot)) {
    fprintf(stderr, "ERROR: path not exists: %s\n", cxxpmRoot.string().c_str());
    exit(1);
  }

  // Handle update mode early, before loading packages
  if (mode == EUpdate) {
    if (!updateRepository(cxxpmRoot, repository))
      return 1;
    return 0;
  }

  if (!std::filesystem::exists(cxxpmRoot / "packages")) {
    fprintf(stderr, "ERROR: path not exists: %s\n", (cxxpmRoot/"packages").string().c_str());
    exit(1);
  }

  // Initialize
  // Paths
  context.GlobalSettings.PackageRoot = cxxpmRoot;
  context.GlobalSettings.HomeDir = userHomeDir() / ".cxxpm";
  context.GlobalSettings.DistrDir = context.GlobalSettings.HomeDir / "distr";
  // Toolchain data
  context.SystemInfo.HostSystemName = osGetSystemName();
  context.SystemInfo.HostSystemProcessor = osGetSystemProcessor();
  if (context.SystemInfo.HostSystemName.empty() || context.SystemInfo.HostSystemProcessor.empty()) {
    fprintf(stderr, "ERROR: can't detect system name and/or system processor architecture\n");
    exit(1);
  }

  context.SystemInfo.TargetSystemName = toolchainSystemName.empty() ? context.SystemInfo.HostSystemName : toolchainSystemName;
  context.SystemInfo.TargetSystemProcessor = toolchainSystemProcessor.empty() ? context.SystemInfo.HostSystemProcessor : systemProcessorNormalize(toolchainSystemProcessor);

#ifdef __APPLE__
  if (isysRoot.empty()) {
    // try xcrun --show-sdk-path
    std::filesystem::path fullPath;
    std::string stdout;
    std::string stderr;
    if (run(".", "xcrun", {"--show-sdk-path"}, {}, fullPath, stdout, stderr, true)) {
      context.SystemInfo.ISysRoot = stdout.substr(0, stdout.size() - 1);
    } else {
      context.SystemInfo.ISysRoot = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
    }
  } else {
    context.SystemInfo.ISysRoot = isysRoot;
  }

  // resolve symlink
  if (std::filesystem::exists(context.SystemInfo.ISysRoot) && std::filesystem::is_symlink(context.SystemInfo.ISysRoot)) {
    auto link = std::filesystem::read_symlink(context.SystemInfo.ISysRoot);
    if (link.is_absolute())
      context.SystemInfo.ISysRoot = link;
    else
      context.SystemInfo.ISysRoot = context.SystemInfo.ISysRoot.parent_path() / link;
  }

  if (!std::filesystem::exists(context.SystemInfo.ISysRoot)) {
    fprintf(stderr, "ERROR: isysroot path not exists: %s\n", context.SystemInfo.ISysRoot.string().c_str());
    exit(1);
  }
#endif

  if (!doBuildTypeMapping(buildType, buildTypeMapping, context.SystemInfo.BuildType))
    return 1;

  std::filesystem::create_directories(context.GlobalSettings.HomeDir);
  std::filesystem::create_directories(context.GlobalSettings.DistrDir);

  // Load all packages
  std::map<std::string, CPackage> packages;
  std::set<std::filesystem::path> visited;
  for (const auto &folder: std::filesystem::directory_iterator{cxxpmRoot / "packages"}) {
    std::string name = folder.path().filename().string();
    if (name.empty() || name[0] == '.' || name == MANIFEST_FILENAME || name == SIGN_FILENAME)
      continue;
    CPackage package;
    package.Name = name;
    package.Path = folder.path();
    packages.insert(std::make_pair(package.Name, package));
  }

  for (const auto &extraPackageDir: extraPackageDirs) {
    if (!visited.insert(extraPackageDir).second) {
      fprintf(stderr, "ERROR: extra package directory %s specified twice\n", extraPackageDir.string().c_str());
      exit(1);
    }
    for (const auto &folder: std::filesystem::directory_iterator{cxxpmRoot / "packages"}) {
      std::string name = folder.path().filename().string();
      if (name.empty() || name[0] == '.' || name == MANIFEST_FILENAME || name == SIGN_FILENAME)
        continue;
      auto It = packages.find(name);
      if (It == packages.end()) {
        // New package found
        CPackage package;
        package.Name = name;
        package.Path = folder.path();
        packages.insert(std::make_pair(package.Name, package));
      } else {
        // Already known package, check version intersection
        It->second.ExtraPath.push_back(folder.path());
      }
    }
  }

  switch (mode) {
    case ENoMode : {
      fprintf(stderr, "You must specify mode, see --help\n");
      exit(1);
    }
    case EPackageList : {
      if (packageName.empty()) {
        packageList(cxxpmRoot, extraPackageDirs, packages);
      } else {
        const auto It = packages.find(packageName);
        if (It == packages.end()) {
          fprintf(stderr, "ERROR: unknown package: %s\n", packageName.c_str());
          return 1;
        }
        packageVersionList(It->second);
      }
      break;
    }
    case EUpdate :
      // Handled earlier
      break;
    case ESearchPath : {
      if (context.SystemInfo.BuildType.size() != 1) {
        fprintf(stderr, "ERROR: search path mode supports only single build type\n");
        return 1;
      }

      std::string buildType = context.SystemInfo.BuildType[0].MappedTo;
      const auto It = packages.find(packageName);
      if (It == packages.end()) {
        fprintf(stderr, "ERROR: unknown package: %s\n", packageName.c_str());
        return 1;
      }

      CPackage &package = It->second;
      if (!inspectPackage(context, package, packageVersion, verbose))
        return 1;
      if (!searchCompilers(package.Languages, context.Compilers, context.Tools, context.SystemInfo, verbose))
        return 1;
      updatePackagePrefix(context, package, buildType, verbose);

      if (!fileArgument.empty()) {
        auto path = searchPath(package.Prefix, std::filesystem::path(fileArgument).make_preferred());
        if (!path.empty()) {
          printf("%s\n", pathConvert(path, pathType).string().c_str());
        } else {
          fprintf(stderr, "ERROR: no file %s in package %s\n", fileArgument.c_str(), packageName.c_str());
          exit(1);
        }
      } else {
        printf("%s\n", pathConvert(package.Prefix, pathType).string().c_str());
      }
      break;
    }
    case EInstall : {
      const auto It = packages.find(packageName);
      if (It == packages.end()) {
        fprintf(stderr, "ERROR: unknown package: %s\n", packageName.c_str());
        exit(1);
      }

      CPackage &package = It->second;
      if (!inspectPackage(context, package, packageVersion, verbose))
        return 1;
      if (!searchCompilers(package.Languages, context.Compilers, context.Tools, context.SystemInfo, verbose))
        return 1;

      std::vector<std::string> buildTypes;
      uniqueBuildTypes(context.SystemInfo.BuildType, buildTypes);

      for (const auto &buildType: buildTypes) {
        updatePackagePrefix(context, package, buildType, verbose);
        if (!install(context, packages, package, buildType, verbose))
          return 1;
      }

      // CMake export
      if (exportCmake) {
        if (!cmakeExport(package, context.GlobalSettings, context.Compilers, context.Tools, context.SystemInfo, outputPath, verbose))
          return 1;
      }
      break;
    }
  }

  return 0;
}
