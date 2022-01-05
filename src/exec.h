#pragma once

#include <filesystem>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

class PathCache {
public:
  PathCache();
	void update();
  std::filesystem::path get(const std::filesystem::path &name);

private:
  std::shared_mutex Mutex_;
  std::unordered_map<std::string, std::filesystem::path> Cache_;
  std::vector<std::filesystem::path> AllPath_;
};

void updatePath();

bool run(const std::filesystem::path &workingDirectory,
	     const std::filesystem::path &path,
	     const std::vector<std::string> &arguments,
	     const std::vector<std::string> &environmentVariables,
	     std::filesystem::path &fullPath,
	     std::string &stdOut,
	     std::string &stdErr,
	     bool executableMustExists);

bool runCaptureLog(const std::filesystem::path &workingDirectory,
	               const std::filesystem::path &path,
	               const std::vector<std::string> &arguments,
	               const std::vector<std::string> &environmentVariables, 
	               FILE *log,
	               bool executableMustExists);

bool runNoCapture(const std::filesystem::path &workingDirectory, 
	              const std::filesystem::path &path, 
	              const std::vector<std::string> &arguments,
	              const std::vector<std::string> &environmentVariables,
	              bool executableMustExists);

#ifdef WIN32
void terminateAllChildProcess();
#endif
