#pragma once

#include "common.h"
#include "strExtras.h"

struct CCpuMapping {
  const char *GNUName;
  const char *NormalizedName;
  bool IsBidirectional;
};

struct CSystemMapping {
  const char *GNUName;
  const char *SystemName;
  const char *SubSystemName;
  bool IsBidirectional;
};

RawData gnuCpuToNormalized(RawData cpu);
RawData gnuCpuFromNormalized(RawData cpu);
std::pair<RawData, RawData> gnuSystemToNormalized(RawData systemName);
RawData gnuSystemFromNormalized(RawData systemName, RawData subSystemName);
RawData clangCpuFromNormalized(RawData cpu);

bool loadGNUSettings(CCompilerInfo &info, bool verbose);
bool gnuSearchTools(ToolsArray &tools, CompilersArray& compilers, CSystemInfo &info);
std::string gnuClangProcessorFromNormalized(const std::string &arch);
