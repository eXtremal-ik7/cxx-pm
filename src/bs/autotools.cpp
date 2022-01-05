#include "autotools.h"
#include "compilers/gnu.h"

static void addEnv(std::vector<std::string> &env, const std::string &name, const std::string &value)
{
  env.emplace_back(name);
  env.back().push_back('=');
  env.back().append(value);
}

void addAutotoolsEnv(std::vector<std::string> &env, const CPackage &package, const CompilersArray &compilers, const ToolsArray &tools, const CSystemInfo &systemInfo, const std::string &buildType)
{
  std::string cpu;
  std::string systemName;
  std::string clangArch;
  RawData::assign(cpu, gnuCpuFromNormalized(systemInfo.TargetSystemProcessor));
  RawData::assign(systemName, gnuSystemFromNormalized(systemInfo.TargetSystemName, systemInfo.TargetSystemSubType));
  RawData::assign(clangArch, clangCpuFromNormalized(systemInfo.TargetSystemProcessor));

  addEnv(env, "CXXPM_AUTOTOOLS_PROCESSOR", cpu);
  addEnv(env, "CXXPM_AUTOTOOLS_SYSTEM_NAME", systemName);
  addEnv(env, "CXXPM_AUTOTOOLS_HOST", cpu + "-" + systemName);
  addEnv(env, "CXXPM_CLANG_ARCH", clangArch);
}
