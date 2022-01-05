#include "os.h"

#include "exec.h"
#include "strExtras.h"
#include <algorithm>

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

static std::filesystem::path posixPath(const std::filesystem::path& path)
{
#ifdef WIN32
  // create msys2 compatible path
  std::u32string u32Path = path.u32string();
  // replace disk
  if (u32Path.size() >= 3 && u32Path[1] == ':' && u32Path[2] == '\\') {
    char32_t disk = u32Path[0];
    u32Path[0] = '/';
    u32Path[1] = disk;
    u32Path[2] = '/';
  }

  for (size_t i = 0, ie = u32Path.size(); i != ie; ++i) {
    if (u32Path[i] == '\\')
      u32Path[i] = '/';
  }

  return std::filesystem::path(u32Path);
#else
  return path;
#endif
}

static std::filesystem::path cmakePath(const std::filesystem::path& path)
{
#ifdef WIN32
  std::u32string u32Path = path.u32string();
  for (size_t i = 0, ie = u32Path.size(); i != ie; ++i) {
    if (u32Path[i] == '\\')
      u32Path[i] = '/';
  }

  return std::filesystem::path(u32Path);
#else
  return path;
#endif
}


std::string systemProcessorNormalize(const std::string_view processor)
{
  if (processor == "arm64" || processor == "ARM64")
    return "aarch64";
  else if (processor == "AMD64" || processor == "x64")
    return "x86_64";
  else if (processor == "i386" || processor == "i686")
    return "x86";
  else
    return std::string(processor);
}

std::string osGetSystemName()
{
#ifdef WIN32
  return "Windows";
#else
  std::string capturedOut;
  std::string capturedErr;
  std::filesystem::path path;
  if (!run(".", "uname", { "-s" }, {}, path, capturedOut, capturedErr, true))
    return std::string();

  std::string_view result;
  StringSplitter splitter(capturedOut, "\r\n");
  if (splitter.next())
    result = splitter.get();
  return !splitter.next() ? std::string(result) : std::string();
#endif
}

std::string osGetSystemProcessor()
{
#ifdef WIN32
  // Try use IsWow64Process2 if possible
  HMODULE hModule = GetModuleHandle("kernel32.dll");
  if (hModule != NULL) {
    typedef BOOL __stdcall procTy(HANDLE, USHORT*, USHORT*);
    procTy* proc = reinterpret_cast<procTy*>(GetProcAddress(hModule, "IsWow64Process2"));
    if (proc) {
      USHORT processMachine;
      USHORT nativeMachine;
      if (proc(GetCurrentProcess(), &processMachine, &nativeMachine)) {
        switch (nativeMachine) {
        case IMAGE_FILE_MACHINE_AMD64: return "x86_64";
        case IMAGE_FILE_MACHINE_ARM: return "arm";
        case IMAGE_FILE_MACHINE_ARM64: return "aarch64";
        case IMAGE_FILE_MACHINE_I386: return "x86";
        }
      }
    }
  }

  SYSTEM_INFO systemInfo;
  GetNativeSystemInfo(&systemInfo);

  switch (systemInfo.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
  case PROCESSOR_ARCHITECTURE_ARM: return "arm";
  case PROCESSOR_ARCHITECTURE_ARM64: return "aarch64";
  case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
  default: return std::string();
  }
#else
  std::string capturedOut;
  std::string capturedErr;
  std::filesystem::path path;
  if (!run(".", "uname", { "-m" }, {}, path, capturedOut, capturedErr, true))
    return std::string();

  std::string_view result;
  StringSplitter splitter(capturedOut, "\r\n");
  if (splitter.next())
    result = splitter.get();
  return !splitter.next() ? systemProcessorNormalize(result) : std::string();
#endif
}

EPathType pathTypeFromString(const std::string &type)
{
  if (type == "native")
    return EPathType::Native;
  else if (type == "posix")
    return EPathType::Posix;
  else if (type == "cmake")
    return EPathType::CMake;
  return EPathType::Unknown;
}

std::filesystem::path pathConvert(const std::filesystem::path& path, EPathType type)
{
  switch (type) {
    case EPathType::Native: {
      std::filesystem::path result(path);
      result.make_preferred();
      return result;
    }
    case EPathType::Posix: {
      return posixPath(path);
    }
    case EPathType::CMake: {
      return cmakePath(path);
    }
    default: {
      fprintf(stderr, "ERROR: this path type not supported!\n");
      return path;
    }
  }
}

bool doBuildTypeMapping(const std::string &buildType, const std::string &mapping, std::vector<CBuildType> &out)
{
  // not need binary tree or hashtable here
  std::vector<CBuildType> btMap;
  std::string defaultBt;

  // Read mapping
  {
    StringSplitter semicolonSplitter(mapping, ";");
    while (semicolonSplitter.next()) {
      StringSplitter colonSplitter(semicolonSplitter.get(), ":");
      if (!colonSplitter.next()) {
        fprintf(stderr, "ERROR: invalid build type mapping format: %s\n", mapping.c_str());
        return false;
      }
      auto from = colonSplitter.get();

      if (!colonSplitter.next()) {
        fprintf(stderr, "ERROR: invalid build type mapping format: %s\n", mapping.c_str());
        return false;
      }
      auto to = colonSplitter.get();

      if (colonSplitter.next()) {
        fprintf(stderr, "ERROR: invalid build type mapping format: %s\n", mapping.c_str());
        return false;
      }

      if (from == "*") {
        if (!defaultBt.empty()) {
          fprintf(stderr, "ERROR: build type mapping contains more than one default mapping: %s\n", mapping.c_str());
          return false;
        }

        defaultBt.assign(to.begin(), to.end());
      } else {
        btMap.emplace_back(from, to);
      }
    }
  }

  // Read build types
  {
    StringSplitter semicolonSplitter(buildType, ";");
    while (semicolonSplitter.next()) {
      std::string mapped;
      auto cfg = semicolonSplitter.get();
      for (const auto &m: btMap) {
        if (cfg == m.Name) {
          mapped = m.MappedTo;
          break;
        }
      }

      if (mapped.empty() && !defaultBt.empty())
        mapped = defaultBt;
      if (mapped.empty())
        mapped = cfg;

      out.emplace_back(cfg, mapped);
    }
  }

  return true;
}

void uniqueBuildTypes(const std::vector<CBuildType> &in, std::vector<std::string> &out)
{
  // not need binary tree or hashtable here
  std::vector<std::string> visited;
  for (const auto &buildType: in) {
    if (std::find(visited.begin(), visited.end(), buildType.MappedTo) == visited.end()) {
      out.push_back(buildType.MappedTo);
      visited.push_back(buildType.MappedTo);
    }
  }
}

std::filesystem::path userHomeDir()
{
  char homedir[512];
#ifdef _WIN32
  snprintf(homedir, sizeof(homedir), "%s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"));
#else
  snprintf(homedir, sizeof(homedir), "%s", getenv("HOME"));
#endif
  return homedir;
}

std::filesystem::path whereami(const char *argv0)
{
#ifdef WIN32
  std::wstring moduleFileName;
  moduleFileName.resize(32768);
  DWORD nSize = GetModuleFileNameW(NULL, moduleFileName.data(), 32768+1);
  if (!nSize)
    return std::filesystem::path();

  return std::filesystem::path(moduleFileName.c_str());
#else
  std::filesystem::path result;
  char *cwd = getcwd(nullptr, 0);
  std::filesystem::path currentDirectory(cwd);
  free(cwd);
  if (argv0[0] == '/') {
    result = argv0;
  } else if (argv0[0] == '.' && argv0[1] == '/') {
    result = currentDirectory / (argv0 + 2);
  } else if (argv0[0] == '~' && argv0[1] == '/') {
    result = userHomeDir() / (argv0 + 2);
  } else if (strchr(argv0, '/') != nullptr) {
    result = currentDirectory / argv0;
  } else {
    char *pathEnv = getenv("PATH");
    StringSplitter splitter(pathEnv, ":");
    std::vector<std::string_view> pathDirectories;
    while (splitter.next())
      pathDirectories.push_back(splitter.get());
    for (auto I = pathDirectories.rbegin(), IE = pathDirectories.rend(); I != IE; ++I) {
      std::filesystem::path path = std::filesystem::path(*I) / argv0;
      if (std::filesystem::exists(path)) {
        result = path;
        break;
      }
    }
  }

  return std::filesystem::exists(result) ? result : std::filesystem::path();
#endif
}
