#include "httpdownload.h"
#include <stdio.h>

#ifdef WIN32

#include <windows.h>
#include <winhttp.h>
#include <string>

// Parse URL into components
struct UrlComponents {
  bool https;
  std::wstring host;
  INTERNET_PORT port;
  std::wstring path;
};

static std::wstring utf8ToWide(const std::string &s)
{
  if (s.empty()) return {};
  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring result(len, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &result[0], len);
  return result;
}

static bool parseUrl(const std::string &url, UrlComponents &out)
{
  std::wstring wurl = utf8ToWide(url);

  URL_COMPONENTS uc = {};
  uc.dwStructSize = sizeof(uc);
  wchar_t hostBuf[256] = {};
  wchar_t pathBuf[2048] = {};
  uc.lpszHostName = hostBuf;
  uc.dwHostNameLength = sizeof(hostBuf) / sizeof(wchar_t);
  uc.lpszUrlPath = pathBuf;
  uc.dwUrlPathLength = sizeof(pathBuf) / sizeof(wchar_t);

  if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc))
    return false;

  out.https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
  out.host = uc.lpszHostName;
  out.port = uc.nPort;
  out.path = uc.lpszUrlPath;
  return true;
}

static bool httpDownloadImpl(const std::string &url, std::vector<uint8_t> &data)
{
  UrlComponents uc;
  if (!parseUrl(url, uc)) {
    fprintf(stderr, "ERROR: can't parse URL: %s\n", url.c_str());
    return false;
  }

  HINTERNET hSession = WinHttpOpen(L"cxx-pm/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    fprintf(stderr, "ERROR: WinHttpOpen failed\n");
    return false;
  }

  HINTERNET hConnect = WinHttpConnect(hSession, uc.host.c_str(), uc.port, 0);
  if (!hConnect) {
    fprintf(stderr, "ERROR: WinHttpConnect failed\n");
    WinHttpCloseHandle(hSession);
    return false;
  }

  DWORD flags = uc.https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", uc.path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) {
    fprintf(stderr, "ERROR: WinHttpOpenRequest failed\n");
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    fprintf(stderr, "ERROR: WinHttpSendRequest failed\n");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    fprintf(stderr, "ERROR: WinHttpReceiveResponse failed\n");
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Check status code
  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                       WINHTTP_NO_HEADER_INDEX);
  if (statusCode != 200) {
    fprintf(stderr, "ERROR: HTTP %lu for %s\n", statusCode, url.c_str());
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
  }

  // Read data
  uint8_t buffer[65536];
  DWORD bytesRead = 0;
  while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) {
    if (bytesRead == 0)
      break;
    data.insert(data.end(), buffer, buffer + bytesRead);
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return true;
}

#else

// On Linux/macOS, use wget (already installed)
#include <unistd.h>

static bool httpDownloadImpl(const std::string &url, std::vector<uint8_t> &data)
{
  // Download to unique temp file, then read
  char tmpPath[] = "/tmp/cxxpm-dl-XXXXXX";
  int fd = mkstemp(tmpPath);
  if (fd < 0) {
    fprintf(stderr, "ERROR: can't create temp file\n");
    return false;
  }
  close(fd);

  std::string cmd = "wget -q \"" + url + "\" -O \"" + tmpPath + "\"";
  if (system(cmd.c_str()) != 0) {
    remove(tmpPath);
    fprintf(stderr, "ERROR: wget failed for %s\n", url.c_str());
    return false;
  }

  FILE *f = fopen(tmpPath, "rb");
  if (!f) {
    remove(tmpPath);
    fprintf(stderr, "ERROR: can't open downloaded file\n");
    return false;
  }

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  data.resize(sz);
  fread(data.data(), 1, sz, f);
  fclose(f);
  remove(tmpPath);
  return true;
}

#endif

bool httpDownloadToMemory(const std::string &url, std::vector<uint8_t> &data)
{
  return httpDownloadImpl(url, data);
}

bool httpDownloadFile(const std::string &url, const std::filesystem::path &destPath)
{
  std::vector<uint8_t> data;
  if (!httpDownloadImpl(url, data))
    return false;

  FILE *f = fopen(destPath.string().c_str(), "wb");
  if (!f) {
    fprintf(stderr, "ERROR: can't create file %s\n", destPath.string().c_str());
    return false;
  }

  fwrite(data.data(), 1, data.size(), f);
  fclose(f);
  return true;
}
