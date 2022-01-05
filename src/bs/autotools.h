#pragma once

#include "os.h"
#include "compilers/common.h"

struct CPackage;

void addAutotoolsEnv(std::vector<std::string> &env, const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string &buildType);
