#pragma once

#include "common.h"

std::string getVsArch(std::string_view processor);
bool loadMSVCSettings(CCompilerInfo &info, CSystemInfo &systemInfo, bool verbose);
bool msvcLookupVersion(CSystemInfo &info);
bool msvcSearchTools(ToolsArray &tools, CompilersArray &compilers, CSystemInfo &info);
