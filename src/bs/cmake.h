#pragma once

#include "os.h"
#include "package.h"
#include "compilers/common.h"

struct CxxPmSettings;

std::string cmakeGetConfigureArgs(const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string &buildType);
std::string cmakeGetBuildArgs(const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string &buildType);

bool cmakeExport(const CPackage &package, const CxxPmSettings &globalSettings, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::filesystem::path &output, bool verbose);
