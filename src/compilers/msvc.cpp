#pragma once

#include "msvc.h"
#include "exec.h"
#include "os.h"
#include "strExtras.h"
#include <Windows.h>

std::string getVsArch(std::string_view processor)
{
  if (processor == "x86")
    return "Win32";
  else if (processor == "x86_64")
    return "x64";
  else if (processor == "aarch64")
    return "ARM64";

  return std::string();
}

static bool vcEnvironmentInitialized(std::filesystem::path &vsInstallDir)
{
  DWORD hasVsInstallDirEnv = GetEnvironmentVariable("VSINSTALLDIR", NULL, 0);
  DWORD hasIncludeEnv = GetEnvironmentVariable("INCLUDE", NULL, 0);
  DWORD hasLibEnv = GetEnvironmentVariable("LIB", NULL, 0);
  if (hasVsInstallDirEnv) {
    std::string v;
    v.resize(hasVsInstallDirEnv + 1);
    GetEnvironmentVariable("VSINSTALLDIR", v.data(), hasVsInstallDirEnv);
    vsInstallDir = v;
  }

  return hasVsInstallDirEnv && hasIncludeEnv && hasLibEnv;
}

static std::string selectVCVarsPlatform(const CSystemInfo& info)
{
  if (info.HostSystemProcessor == "x86") {
    if (info.TargetSystemProcessor == "x86") return "x86";
    if (info.TargetSystemProcessor == "x86_64") return "x86_x64";
    if (info.TargetSystemProcessor == "aarch64") return "x86_arm64";
  } else if (info.HostSystemProcessor == "x86_64") {
    if (info.TargetSystemProcessor == "x86") return "x64_x86";
    if (info.TargetSystemProcessor == "x86_64") return "x64";
    if (info.TargetSystemProcessor == "aarch64") return "x64_arm64";
  } else if (info.HostSystemProcessor == "aarch64") {
    // No native compilers for arm64, use x86_64 emulation
    if (info.TargetSystemProcessor == "x86") return "x64_x86";
    if (info.TargetSystemProcessor == "x86_64") return "x64";
    if (info.TargetSystemProcessor == "aarch64") return "x64_arm64";
  }

  return std::string();
}

static bool loadClExecutableSettings(CCompilerInfo& info, bool verbose)
{
  std::string capturedOut;
  std::string capturedErr;
  std::filesystem::path path;
  if (!run(".", info.Command, {}, {}, path, capturedOut, capturedErr, false))
    return false;

  info.Command = path;

  std::string_view firstOutputLine;
  {
    StringSplitter splitter(capturedErr, "\r\n");
    if (splitter.next())
      firstOutputLine = splitter.get();
  }

  if (firstOutputLine.empty())
    return false;

  unsigned knownCount = 0;
  std::vector<std::string_view> firstLineTokens;
  {
    StringSplitter splitter(firstOutputLine, " ");
    while (splitter.next()) {
      auto token = splitter.get();
      firstLineTokens.push_back(token);
      if (token == "Microsoft" || token == "(R)" || token == "C/C++")
        knownCount++;
    }
  }

  if (knownCount == 3 && firstLineTokens.size() >= 3) {
    // extract version
    std::string_view archReported = firstLineTokens[firstLineTokens.size() - 1];
    std::string_view versionReported = firstLineTokens[firstLineTokens.size() - 3];

    info.Id = "cl-" + std::string(archReported) + "-" + std::string(versionReported);
    info.Type = ECompilerType::MSVC;
    info.DetectedSystemProcessor = systemProcessorNormalize(std::string(archReported));
  }

  info.DetectedSystemName = "Windows";
  info.SystemSubType = "msvc";
  return true;
}

bool loadMSVCSettings(CCompilerInfo& info, CSystemInfo& systemInfo, bool verbose)
{
  // case 1: compiler info has VC install path
  // Call vcvarsall.bat first for initialize environment
  if (!systemInfo.VCInstallDir.empty()) {
    std::filesystem::path vcvarsPath = systemInfo.VCInstallDir / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";
    if (!std::filesystem::exists(vcvarsPath)) {
      fprintf(stderr, "ERROR: can't find vcvarsall.bat into VS install directory %s\n", systemInfo.VCInstallDir.string().c_str());
      return false;
    }

    std::string vcvarsPlatform = selectVCVarsPlatform(systemInfo);
    if (vcvarsPlatform.empty()) {
      fprintf(stderr, "ERROR: can't initialize environment for host %s and target %s\n", systemInfo.HostSystemProcessor.c_str(), systemInfo.TargetSystemProcessor.c_str());
      return false;
    }

    // Retrieve environment variables
    // cmd /k call "vcvarsall.bat" x64 & SET & exit 0

    // If we have initialized __VSCMD_PREINIT_PATH, need restore original PATH
    wchar_t buffer[1024];
    DWORD preinitPathSize = GetEnvironmentVariableW(L"__VSCMD_PREINIT_PATH", buffer, sizeof(buffer)/sizeof(wchar_t));
    if (preinitPathSize > 1023) {
      std::unique_ptr<wchar_t[]> buffer(new wchar_t[preinitPathSize + 1]);
      GetEnvironmentVariableW(L"__VSCMD_PREINIT_PATH", buffer.get(), preinitPathSize + 1);
      SetEnvironmentVariableW(L"PATH", buffer.get());
      updatePath();
    } else if (preinitPathSize > 0) {
      std::wstring path(buffer);
      path.push_back(';');
      path.append(systemInfo.MSys2Path);
      SetEnvironmentVariableW(L"PATH", path.c_str());
      updatePath();
    }

    std::string capturedOut;
    std::string capturedErr;
    std::filesystem::path path;
    std::string arg = "call \"";
      arg.append(vcvarsPath.string());
      arg.append("\" ");
      arg.append(vcvarsPlatform);
      arg.append(" & SET & exit 0");
      
    if (!run(".", "cmd.exe", { "/k", arg }, {}, path, capturedOut, capturedErr, true)) {
      fprintf(stderr, "ERROR: can't retrieve VS environment variables");
      return false;
    }

    std::istringstream stream(capturedOut);
    std::string line;
    while (std::getline(stream, line)) {
      size_t pos = line.find('=');
      if (pos == 0 || pos == line.npos || pos == line.size() - 1)
        continue;

      std::string name = line.substr(0, pos);
      std::string value = line.substr(pos + 1);
      while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
        value.pop_back();
      if (!SetEnvironmentVariable(name.c_str(), value.c_str())) {
        fprintf(stderr, "ERROR: SetEnvironmentVariable failed (%s=%s)\n", name.c_str(), value.c_str());
      }
    }

    updatePath();
    
    std::filesystem::path vcInstallDir;
    if (vcEnvironmentInitialized(vcInstallDir)) {
      info.Command = "cl";
      if (loadClExecutableSettings(info, verbose)) {
        return true;
      } else {
        fprintf(stderr, "ERROR: found msvc environment with unknown compiler at %s\n", vcInstallDir.string().c_str());
        return false;
      }
    } else {
      fprintf(stderr, "ERROR: broken VS installation at %s\n", systemInfo.VCInstallDir.string().c_str());
    }

    return false;
  }

  // case 2: VC install path is unknown, but msvc environment initialized (vcvars***.bat was called)
  // Check environment variables 
  std::filesystem::path vcInstallDir;
  if (vcEnvironmentInitialized(vcInstallDir)) {
    info.Command = "cl";
    if (loadClExecutableSettings(info, verbose)) {
      // store VC install path
      return true;
    } else {
      fprintf(stderr, "ERROR: found msvc environment with unknown compiler at %s\n", vcInstallDir.string().c_str());
      return false;
    }
  }

  // case 3: no environment or VC install path, autosearch
  {
    return false;
  }
}

bool msvcLookupVersion(CSystemInfo& info)
{
  char buffer[64];
  DWORD size = GetEnvironmentVariable("VCToolsVersion", buffer, sizeof(buffer) / sizeof(char));
  if (size == 0 || size >= sizeof(buffer) / sizeof(char)) {
    fprintf(stderr, "ERROR: can't found environment variable VCToolsVersion\n");
    return false;
  }
   
  std::string version;
  unsigned counter = 0;
  StringSplitter splitter(buffer, ".");
  while (splitter.next()) {
    if (counter++ < 2)
      version.append(splitter.get());
  }

  if (counter >= 2 && version.size() >= 3) {
    info.VSToolSetVersion = "v" + std::string(version.begin(), version.end()-1);
    return true;
  } else {
    fprintf(stderr, "ERROR: invalid format of VCToolsVersion: %s\n", buffer);
    return false;
  }
}

bool msvcSearchTools(ToolsArray& tools, CompilersArray& compilers, CSystemInfo& info)
{
  return true;
}
