#include "exec.h"
#include <strExtras.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/wait.h>
extern char** environ;
#else
#include <windows.h>
#endif

#include <mutex>

#ifdef WIN32
struct JobSingletone {
public:
    JobSingletone() {
      Job = CreateJobObjectW(NULL, NULL);
    }
public:
    HANDLE Job;
};

JobSingletone gJob;
#endif

PathCache::PathCache()
{
  update();
}

void PathCache::update()
{
  std::vector<std::string> allPath;

#ifndef WIN32
  StringSplitter splitter(getenv("PATH"), ":");
#else
  std::string path;
  DWORD pathSize = GetEnvironmentVariable("PATH", NULL, 0);
  if (pathSize) {
    path.resize(pathSize - 1);
    GetEnvironmentVariable("PATH", path.data(), pathSize);
  }
  StringSplitter splitter(path, ";");
#endif

  std::unique_lock lock(Mutex_);
  AllPath_.clear();
  while (splitter.next()) {
    AllPath_.emplace_back(splitter.get());
  }
}

std::filesystem::path PathCache::get(const std::filesystem::path &name)
{
  {
    std::shared_lock lock(Mutex_);
    const auto &It = Cache_.find(name.string());
    if (It != Cache_.end())
      return It->second;
  }

  // Search executable
  std::filesystem::path nameForSearch = name;
#ifdef WIN32
  if (nameForSearch.extension() != ".exe")
    nameForSearch += ".exe";
#endif

  for (auto I = AllPath_.rbegin(), IE = AllPath_.rend(); I != IE; ++I) {
    std::filesystem::path current = *I / nameForSearch;
    if (std::filesystem::exists(current) && !std::filesystem::is_directory(current)) {
      {
        std::unique_lock lock(Mutex_);
        Cache_[name.string()] = current;
      }
      return current;
    }
  }

  return std::filesystem::path();
}

static PathCache gPathCache;

void updatePath()
{
  gPathCache.update();
}

bool run(const std::filesystem::path &workingDirectory,
         const std::filesystem::path &path,
         const std::vector<std::string> &arguments,
         const std::vector<std::string> &environmentVariables,
         std::filesystem::path &fullPath,
         std::string &stdOut,
         std::string &stdErr,
         bool executableMustExists)
{
  fullPath = path.is_absolute() ? path : gPathCache.get(path);
  if (fullPath.empty()) {
    if (executableMustExists)
      fprintf(stderr, "ERROR: can't found executable %s\n", path.string().c_str());
    return false;
  }

#ifdef WIN32
  // Command line
  std::wstring cmdLine(fullPath);
  for (const auto& arg : arguments) {
    cmdLine.push_back(' ');
    if (arg.find(' ') != arg.npos) {
      cmdLine.push_back('\"');
      cmdLine.append(std::wstring(arg.begin(), arg.end()));
      cmdLine.push_back('\"');
    } else  {
      cmdLine.append(std::wstring(arg.begin(), arg.end()));
    }
  }
  
  // Environment variables
  char* envPtr = GetEnvironmentStringsA();
  char* p = envPtr;
  while (p[0] != 0 || p[1] != 0)
    p++;
  std::string childProcessEnv(envPtr, p + 1);
  for (const auto variable : environmentVariables) {
    childProcessEnv.append(variable);
    childProcessEnv.push_back('\0');
  }
  childProcessEnv.push_back('\0');
  FreeEnvironmentStringsA(envPtr);

  HANDLE stdoutRead;
  HANDLE stdoutWrite;
  HANDLE stderrRead;
  HANDLE stderrWrite;
  SECURITY_ATTRIBUTES attrs;
  attrs.nLength = sizeof(attrs);
  attrs.bInheritHandle = TRUE;
  attrs.lpSecurityDescriptor = NULL;
  if (!CreatePipe(&stdoutRead, &stdoutWrite, &attrs, 0))
    return false;
  if (!CreatePipe(&stderrRead, &stderrWrite, &attrs, 0))
    return false;

  STARTUPINFOW startupInfo;
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  startupInfo.hStdOutput = stdoutWrite;
  startupInfo.hStdError = stderrWrite;
  startupInfo.wShowWindow = SW_HIDE;
  startupInfo.cb = sizeof(startupInfo);

  PROCESS_INFORMATION processInfo = { 0 };
  BOOL result = CreateProcessW(NULL, const_cast<LPWSTR>(cmdLine.c_str()), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, const_cast<char*>(childProcessEnv.c_str()), workingDirectory.c_str(), &startupInfo, &processInfo);
  CloseHandle(stdoutWrite);
  CloseHandle(stderrWrite);
  if (!result) {
    CloseHandle(stdoutRead);
    CloseHandle(stderrRead);
    return false;
  }

  AssignProcessToJobObject(gJob.Job, processInfo.hProcess);

  bool finished = false;
  while (!finished) {
    DWORD dwRead = 0;
    char buffer[4096];
    finished = WaitForSingleObject(processInfo.hProcess, 10) == WAIT_OBJECT_0;
      
    while (ReadFile(stdoutRead, buffer, sizeof(buffer), &dwRead, NULL) && dwRead)
      stdOut.append(buffer, dwRead);
    while (ReadFile(stderrRead, buffer, sizeof(buffer), &dwRead, NULL) && dwRead)
      stdErr.append(buffer, dwRead);
  }

  DWORD exitCode = 1;
  CloseHandle(stdoutRead);
  CloseHandle(stderrRead);
  BOOL exitCodeReceived = GetExitCodeProcess(processInfo.hProcess, &exitCode);
  CloseHandle(processInfo.hProcess);
  return exitCodeReceived && exitCode == 0;
#else
  std::vector<char*> cmdLine;
  std::vector<char*> env;
  // command line
  cmdLine.push_back(const_cast<char*>(path.c_str()));
  for (const auto &arg: arguments)
    cmdLine.push_back(const_cast<char*>(arg.c_str()));
  cmdLine.push_back(0);
  // environment
  {
    char **envPtr = environ;
    while (*envPtr) {
      env.push_back(*envPtr);
      envPtr++;
    }
  }
  for (const auto &envPtr: environmentVariables)
    env.push_back(const_cast<char*>(envPtr.c_str()));
  env.push_back(0);



  int stdoutPipe[2];
  int stderrPipe[2];
  if (pipe(stdoutPipe) == -1)
    return false;
  if (pipe(stderrPipe) == -1)
    return false;
  pid_t pid = fork();
  if (pid == -1)
    return false;
  if (pid == 0) {
    dup2(stdoutPipe[1], STDOUT_FILENO);
    dup2(stderrPipe[1], STDERR_FILENO);
    close(stdoutPipe[0]);
    close(stdoutPipe[1]);
    close(stderrPipe[0]);
    close(stderrPipe[1]);
    if (chdir(workingDirectory.c_str()) == -1) {
      fprintf(stderr, "chdir ERROR %s: \"", strerror(errno));
      exit(1);
    }

    if (execve(fullPath.c_str(), &cmdLine[0], &env[0]) == -1) {
      fprintf(stderr, "execv ERROR %s: \"", strerror(errno));
      for (size_t i = 0, ie = cmdLine.size() - 1; i != ie; ++i) {
        fprintf(stderr, i != (ie-1) ? "%s " : "%s", cmdLine[i]);
      }
      fprintf(stderr, "\"\n");
      exit(1);
    } else {
      exit(0);
    }
  } else {
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    ssize_t bytesRead = 0;
    char buffer[4096];
    while ( (bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer))) != 0)
      stdOut.append(buffer, bytesRead);
    while ( (bytesRead = read(stderrPipe[0], buffer, sizeof(buffer))) != 0)
      stdErr.append(buffer, bytesRead);
    int exitCode;
    do {
      waitpid(pid, &exitCode, WUNTRACED);
     } while (!WIFEXITED(exitCode) && !WIFSIGNALED(exitCode));
     return exitCode == 0;
  }

  return true;
#endif
}

bool runCaptureLog(const std::filesystem::path &workingDirectory, const std::filesystem::path &path, const std::vector<std::string> &arguments, const std::vector<std::string> &environmentVariables, FILE *log, bool executableMustExists)
{
  std::filesystem::path fullPath = path.is_absolute() ? path : gPathCache.get(path);
  if (fullPath.empty()) {
    if (executableMustExists)
      fprintf(stderr, "ERROR: can't found executable %s\n", path.string().c_str());
    return false;
  }

#ifdef WIN32
  // Command line
  std::wstring cmdLine(fullPath);
  for (const auto& arg : arguments) {
    cmdLine.push_back(' ');
    if (arg.find(' ') != arg.npos) {
      cmdLine.push_back('\"');
      cmdLine.append(std::wstring(arg.begin(), arg.end()));
      cmdLine.push_back('\"');
    }
    else {
      cmdLine.append(std::wstring(arg.begin(), arg.end()));
    }
  }

  // Environment variables
  char *envPtr = GetEnvironmentStringsA();
  char* p = envPtr;
  while (p[0] != 0 || p[1] != 0)
    p++;
  std::string childProcessEnv(envPtr, p + 1);
  for (const auto variable : environmentVariables) {
    childProcessEnv.append(variable);
    childProcessEnv.push_back('\0');
  }
  childProcessEnv.push_back('\0');
  FreeEnvironmentStringsA(envPtr);

  HANDLE outputRead;
  HANDLE outputWrite;
  SECURITY_ATTRIBUTES attrs;
  attrs.nLength = sizeof(attrs);
  attrs.bInheritHandle = TRUE;
  attrs.lpSecurityDescriptor = NULL;
  if (!CreatePipe(&outputRead, &outputWrite, &attrs, 0))
    return false;

  STARTUPINFOW startupInfo;
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  startupInfo.hStdOutput = outputWrite;
  startupInfo.hStdError = outputWrite;
  startupInfo.wShowWindow = SW_HIDE;
  startupInfo.cb = sizeof(startupInfo);

  PROCESS_INFORMATION processInfo = { 0 };
  BOOL result = CreateProcessW(NULL, const_cast<LPWSTR>(cmdLine.c_str()), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, const_cast<char*>(childProcessEnv.c_str()), workingDirectory.c_str(), &startupInfo, &processInfo);
  CloseHandle(outputWrite);
  if (!result) {
    CloseHandle(outputRead);
    return false;
  }

  AssignProcessToJobObject(gJob.Job, processInfo.hProcess);

  bool finished = false;
  while (!finished) {
    DWORD dwRead = 0;
    char buffer[4096];
    finished = WaitForSingleObject(processInfo.hProcess, 10) == WAIT_OBJECT_0;

    while (ReadFile(outputRead, buffer, sizeof(buffer), &dwRead, NULL) && dwRead) {
      DWORD bytesWritten = 0;
      fwrite(buffer, 1, dwRead, log);
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, dwRead, &bytesWritten, NULL);
    }
  }

  DWORD exitCode = 1;
  CloseHandle(outputRead);
  BOOL exitCodeReceived = GetExitCodeProcess(processInfo.hProcess, &exitCode);
  CloseHandle(processInfo.hProcess);
  return exitCodeReceived && exitCode == 0;
#else
  std::vector<char*> cmdLine;
  std::vector<char*> env;
  // command line
  cmdLine.push_back(const_cast<char*>(path.c_str()));
  for (const auto &arg: arguments)
    cmdLine.push_back(const_cast<char*>(arg.c_str()));
  cmdLine.push_back(0);
  // environment
  {
    char **envPtr = environ;
    while (*envPtr) {
      env.push_back(*envPtr);
      envPtr++;
    }
  }
  for (const auto &envPtr: environmentVariables)
    env.push_back(const_cast<char*>(envPtr.c_str()));
  env.push_back(0);

  int logPipe[2];
  if (pipe(logPipe) == -1)
    return false;
  pid_t pid = fork();
  if (pid == -1)
    return false;
  if (pid == 0) {
    dup2(logPipe[1], STDOUT_FILENO);
    dup2(logPipe[1], STDERR_FILENO);
    close(logPipe[0]);
    close(logPipe[1]);
    if (chdir(workingDirectory.c_str()) == -1) {
      fprintf(stderr, "chdir ERROR %s: \"", strerror(errno));
      exit(1);
    }
    if (execve(fullPath.c_str(), &cmdLine[0], &env[0]) == -1) {
      fprintf(stderr, "execv ERROR %s: \"", strerror(errno));
      for (size_t i = 0, ie = cmdLine.size() - 1; i != ie; ++i) {
        fprintf(stderr, i != (ie-1) ? "%s " : "%s", cmdLine[i]);
      }
      fprintf(stderr, "\"\n");
      exit(1);
    } else {
      exit(0);
    }
  } else {
    close(logPipe[1]);
    ssize_t bytesRead = 0;
    char buffer[4096];
    while ( (bytesRead = read(logPipe[0], buffer, sizeof(buffer))) != 0) {
      fwrite(buffer, 1, bytesRead, log);
      fwrite(buffer, 1, bytesRead, stdout);
    }
    int exitCode;
    do {
      waitpid(pid, &exitCode, WUNTRACED);
     } while (!WIFEXITED(exitCode) && !WIFSIGNALED(exitCode));
     return exitCode == 0;
  }

  return true;
#endif
}

bool runNoCapture(const std::filesystem::path &workingDirectory, const std::filesystem::path &path, const std::vector<std::string> &arguments, const std::vector<std::string> &environmentVariables, bool executableMustExists)
{
  std::filesystem::path fullPath = path.is_absolute() ? path : gPathCache.get(path);
  if (fullPath.empty()) {
    if (executableMustExists)
      fprintf(stderr, "ERROR: can't found executable %s\n", path.string().c_str());
    return false;
  }

#ifdef WIN32
  // Command line
  std::wstring cmdLine(fullPath);
  for (const auto& arg : arguments) {
    cmdLine.push_back(' ');
    if (arg.find(' ') != arg.npos) {
      cmdLine.push_back('\"');
      cmdLine.append(std::wstring(arg.begin(), arg.end()));
      cmdLine.push_back('\"');
    }
    else {
      cmdLine.append(std::wstring(arg.begin(), arg.end()));
    }
  }

  // Environment variables
  char* envPtr = GetEnvironmentStringsA();
  char* p = envPtr;
  while (p[0] != 0 || p[1] != 0)
    p++;
  std::string childProcessEnv(envPtr, p + 1);
  for (const auto variable : environmentVariables) {
    childProcessEnv.append(variable);
    childProcessEnv.push_back('\0');
  }
  childProcessEnv.push_back('\0');
  FreeEnvironmentStringsA(envPtr);

  STARTUPINFOW startupInfo;
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  startupInfo.wShowWindow = SW_HIDE;
  startupInfo.cb = sizeof(startupInfo);

  PROCESS_INFORMATION processInfo = { 0 };
  BOOL result = CreateProcessW(NULL, const_cast<LPWSTR>(cmdLine.c_str()), NULL, NULL, TRUE, 0, const_cast<char*>(childProcessEnv.c_str()), workingDirectory.c_str(), &startupInfo, &processInfo);
  if (!result)
    return false;

  AssignProcessToJobObject(gJob.Job, processInfo.hProcess);

  bool finished = false;
  while (!finished)
    finished = WaitForSingleObject(processInfo.hProcess, 10) == WAIT_OBJECT_0;

  DWORD exitCode = 1;
  BOOL exitCodeReceived = GetExitCodeProcess(processInfo.hProcess, &exitCode);
  CloseHandle(processInfo.hProcess);
  return exitCodeReceived && exitCode == 0;
#else
  std::vector<char*> cmdLine;
  std::vector<char*> env;
  // command line
  cmdLine.push_back(const_cast<char*>(path.c_str()));
  for (const auto &arg: arguments)
    cmdLine.push_back(const_cast<char*>(arg.c_str()));
  cmdLine.push_back(0);
  // environment
  {
    char **envPtr = environ;
    while (*envPtr) {
      env.push_back(*envPtr);
      envPtr++;
    }
  }
  for (const auto &envPtr: environmentVariables)
    env.push_back(const_cast<char*>(envPtr.c_str()));
  env.push_back(0);

  pid_t pid = fork();
  if (pid == -1)
    return false;
  if (pid == 0) {
    if (chdir(workingDirectory.c_str()) == -1) {
      fprintf(stderr, "chdir ERROR %s: \"", strerror(errno));
      exit(1);
    }
    if (execve(fullPath.c_str(), &cmdLine[0], &env[0]) == -1) {
      fprintf(stderr, "execv ERROR %s: \"", strerror(errno));
      for (size_t i = 0, ie = cmdLine.size() - 1; i != ie; ++i) {
        fprintf(stderr, i != (ie-1) ? "%s " : "%s", cmdLine[i]);
      }
      fprintf(stderr, "\"\n");
      exit(1);
    } else {
      exit(0);
    }
  } else {
    int exitCode;
    do {
      waitpid(pid, &exitCode, WUNTRACED);
     } while (!WIFEXITED(exitCode) && !WIFSIGNALED(exitCode));
     return exitCode == 0;
  }

  return true;
#endif
}

#ifdef WIN32
void terminateAllChildProcess()
{
  TerminateJobObject(gJob.Job, 0);
}
#endif
