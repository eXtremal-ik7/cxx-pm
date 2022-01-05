#include "gnu.h"
#include "exec.h"
#include "os.h"
#include "strExtras.h"
#include <stdio.h>
#include <string.h>
#include <fstream>

// TODO: move to separate text file
static const CCpuMapping gProcessorMapping[] = {
  {"arm64", "aarch64", true},
  {"i386", "x86", false},
  {"i486", "x86", false},
  {"i586", "x86", false},
  {"i686", "x86", true}
};

static const CSystemMapping gSystemNameMapping[] = {
  {"apple-darwin", "Darwin", "", true},
  {"linux-gnu", "Linux", "", true},
  {"pc-cygwin", "Windows", "cygwin", true},
  {"w64-mingw32", "Windows", "mingw-w64", true}
};

RawData gnuCpuToNormalized(RawData cpu)
{
  for (const auto &mapping: gProcessorMapping) {
    if (cpu == mapping.GNUName)
      return mapping.NormalizedName;
  }

  return cpu;
}

RawData gnuCpuFromNormalized(RawData cpu)
{
  for (const auto &mapping: gProcessorMapping) {
    if (cpu == mapping.NormalizedName && mapping.IsBidirectional)
      return mapping.GNUName;
  }

  return cpu;
}

std::pair<RawData, RawData> gnuSystemToNormalized(RawData systemName)
{
  for (const auto &mapping: gSystemNameMapping) {
    if (startsWith(systemName, mapping.GNUName))
      return std::pair(mapping.SystemName, mapping.SubSystemName);
  }

  return std::make_pair(systemName, "");
}

RawData gnuSystemFromNormalized(RawData systemName, RawData subSystemName)
{
  for (const auto &mapping: gSystemNameMapping) {
    if (systemName == mapping.SystemName && subSystemName == mapping.SubSystemName && mapping.IsBidirectional)
      return mapping.GNUName;
  }

  return systemName;
}

RawData clangCpuFromNormalized(RawData cpu)
{
  if (cpu == "aarch64")
    return "arm64";
  return cpu;
}

bool loadGNUSettings(CCompilerInfo &info, bool verbose)
{
  std::string capturedOut;
  std::string capturedErr;
  if (!run(".", info.Command, {"-v"}, {}, info.Command, capturedOut, capturedErr, false)) {
    if (verbose)
      fprintf(stderr, "Can't run %s\n", info.Command.string().c_str());
    return false;
  }

  std::string id;
  StringSplitter splitter(capturedErr, "\r\n");
  while (splitter.next()) {
    size_t pos;
    auto line = splitter.get();
    pos = line.find("Target: ");
    if (pos != line.npos) {
      const char *begin = line.data() + pos + strlen("Target: ");
      const char *end = line.data() + line.size();
      info.ReportedTarget = std::string(begin, end-begin);
    }

    pos = line.find("gcc");
    if (pos != line.npos) {
      info.Type = ECompilerType::GCC;
      id = std::string(line);
      if (id.back() == ' ')
        id.pop_back();
    }

    pos = line.find("clang");
    if (pos != line.npos) {
      info.Type = ECompilerType::Clang;
      id = std::string(line);
      if (id.back() == ' ')
        id.pop_back();
    }
  }

  if (!id.empty()) {
    info.Id = id + "-" + info.ReportedTarget;
  } else {
    return false;
  }

  {
    auto pos = info.ReportedTarget.find('-');
    if (pos != info.ReportedTarget.npos && pos != 0) {
      std::string processor = info.ReportedTarget.substr(0, pos);
      std::string target = info.ReportedTarget.substr(pos+1);

      // Detect target processor
      RawData::assign(info.DetectedSystemProcessor, gnuCpuToNormalized(processor));

      const auto systemName = gnuSystemToNormalized(target);
      RawData::assign(info.DetectedSystemName, systemName.first);
      RawData::assign(info.SystemSubType, systemName.second);
    }
  }

  if (info.Type == ECompilerType::Clang && info.DetectedSystemName == "Darwin") {
    std::filesystem::path tmpDir = getenv("TMPDIR");
    std::filesystem::path tmpFilePath = tmpDir / "cxxpm-clang-check.c";
    std::filesystem::path tmpFileOutPath = tmpDir / "cxxpm-clang-check";
    if (std::filesystem::exists(tmpFilePath)) {
      std::error_code ec;
      if (!std::filesystem::remove(tmpFilePath, ec)) {
        fprintf(stderr, "ERROR: can't remove temporary file %s\n", tmpFilePath.string().c_str());
        return false;
      }
    }

    {
      // TODO: check error
      std::ofstream stream(tmpFilePath);
      stream << "int main(){return 0;}\n";
    }

    const char *clangArch[] = {"arm64", "x86_64", "x86"};
    const char *cxxpmArch[] = {"aarch64", "x86_64", "x86"};
    for (size_t i = 0, ie = sizeof(clangArch)/sizeof(char*); i != ie; ++i) {
      std::filesystem::path fullPath;
      std::string capturedOut;
      std::string capturedErr;
      if (run(".", info.Command, {"-arch", clangArch[i], tmpFilePath.string(), "-o", tmpFileOutPath.string()}, {}, fullPath, capturedOut, capturedErr, true)) {
        info.DetectedMultiArch.emplace_back(cxxpmArch[i]);
      }
    }

    std::error_code ec;
    std::filesystem::remove(tmpFilePath, ec);
    std::filesystem::remove(tmpFileOutPath, ec);
  }

  return true;
}

bool gnuSearchTools(ToolsArray &tools, CompilersArray &compilers, CSystemInfo &info)
{
  std::string reportedTarget;
  std::filesystem::path compilerPath;
  const CCompilerInfo &c = compilers[static_cast<size_t>(ELanguage::C)];
  const CCompilerInfo &cpp = compilers[static_cast<size_t>(ELanguage::CPP)];
  if (!c.Id.empty()) {
    compilerPath = c.Command.parent_path();
    reportedTarget = c.ReportedTarget;
  } else if (!cpp.Id.empty()) {
    compilerPath = cpp.Command.parent_path();
    reportedTarget = cpp.ReportedTarget;
  } else {
    return true;
  }

  if (info.TargetSystemName == "Windows") {
    // Resource compiler
    std::filesystem::path windres;
    if (info.HostSystemName == "Windows")
      windres = compilerPath / "windres.exe";
    else
      windres = reportedTarget + u8"-windres";

    std::string capturedOut;
    std::string capturedErr;
    if (!run(".", windres, {"--help"}, {}, tools[static_cast<size_t>(EToolType::ResourceCompiler)].Command, capturedOut, capturedErr, true))
      return false;
  }

  return true;
}

std::string gnuClangProcessorFromNormalized(const std::string &arch)
{
  if (arch == "x86") return "i686";
  if (arch == "aarch64") return "arm64";
  return arch;
}
