#include "sha3Tools.h"
#include "ecdsa.h"
#include "strExtras.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <getopt.h>
#include <stdio.h>

static const char *gManifestFilename = "MANIFEST";
static const char *gSignFilename = "SIGN";

enum OptsTy {
  optHelp = 1,
  optPrivateKey,
  optTarget
};

static option cmdLineOpts[] = {
  {"help", no_argument, nullptr, optHelp},
  {"private-key", required_argument, nullptr, optPrivateKey},
  {"target", required_argument, nullptr, optTarget},
  {nullptr, 0, nullptr, 0}
};

static std::string computeHash(const std::filesystem::path &path)
{
  if (std::filesystem::is_directory(path))
    return sha3DirectoryHash(path, gManifestFilename);
  else
    return sha3FileHash(path);
}

std::string generateManifestContent(const std::filesystem::path &dir)
{
  std::map<std::string, std::string> entries;

  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    if (name == gManifestFilename || name == gSignFilename || (!name.empty() && name[0] == '.'))
      continue;
    std::string hash = computeHash(entry.path());
    if (hash.empty()) {
      fprintf(stderr, "ERROR: failed to compute hash for %s\n", entry.path().string().c_str());
      return std::string();
    }
    entries[name] = hash;
  }

  std::ostringstream oss;
  for (const auto &[name, hash] : entries)
    oss << name << " " << hash << "\n";

  return oss.str();
}

bool loadPrivateKey(const std::filesystem::path &keyPath, uint8_t privateKey[32])
{
  std::ifstream file(keyPath);
  if (!file)
    return false;

  std::string line;
  std::string hexKey;

  while (std::getline(file, line)) {
    // Skip PEM headers
    if (line.find("-----") != std::string::npos)
      continue;
    // Remove whitespace
    for (char c : line) {
      if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        hexKey += c;
    }
  }

  // Expect 64 hex characters (32 bytes)
  if (hexKey.size() == 64) {
    hex2bin(hexKey.c_str(), 64, privateKey);
    return true;
  }

  return false;
}

void printHelp()
{
  puts("Usage: cxx-pm-manifestupdate --private-key <file> [options]");
  puts("Options:");
  puts("  --private-key <file>\tPath to private key file (required)");
  puts("  --target <dir>\tTarget directory (default: current directory)");
  puts("  --help\t\tShow this help message");
}

int main(int argc, char **argv)
{
  std::filesystem::path dir = std::filesystem::current_path();
  std::filesystem::path keyPath;

  int res;
  int index = 0;
  while ((res = getopt_long(argc, argv, "", cmdLineOpts, &index)) != -1) {
    switch (res) {
      case optHelp:
        printHelp();
        return 0;
      case optPrivateKey:
        keyPath = optarg;
        break;
      case optTarget:
        dir = optarg;
        break;
      case '?':
        return 1;
    }
  }

  if (keyPath.empty()) {
    fprintf(stderr, "ERROR: --private-key is required\n");
    printHelp();
    return 1;
  }

  if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
    fprintf(stderr, "ERROR: %s is not a directory\n", dir.string().c_str());
    return 1;
  }

  std::string content = generateManifestContent(dir);
  if (content.empty())
    return 1;

  std::filesystem::path manifestPath = dir / gManifestFilename;
  std::ofstream manifestOut(manifestPath);
  if (!manifestOut) {
    fprintf(stderr, "ERROR: cannot open %s for writing\n", manifestPath.string().c_str());
    return 1;
  }
  manifestOut << content;
  manifestOut.close();

  // Sign the manifest file
  std::string manifestHash = sha3FileHash(manifestPath);
  if (manifestHash.empty()) {
    fprintf(stderr, "ERROR: failed to compute manifest hash\n");
    return 1;
  }

  uint8_t privateKey[32];
  if (!loadPrivateKey(keyPath, privateKey)) {
    fprintf(stderr, "ERROR: cannot load private key from %s\n", keyPath.string().c_str());
    return 1;
  }

  uint8_t hashBytes[32];
  hex2bin(manifestHash.c_str(), 64, hashBytes);

  std::string signature = ecdsaSign(hashBytes, privateKey);
  if (signature.empty()) {
    fprintf(stderr, "ERROR: failed to sign manifest\n");
    return 1;
  }

  std::filesystem::path signPath = dir / gSignFilename;
  std::ofstream signOut(signPath);
  if (!signOut) {
    fprintf(stderr, "ERROR: cannot open %s for writing\n", signPath.string().c_str());
    return 1;
  }
  signOut << signature << "\n";

  printf("Manifest written to %s\n", manifestPath.string().c_str());
  printf("Signature written to %s\n", signPath.string().c_str());
  return 0;
}
