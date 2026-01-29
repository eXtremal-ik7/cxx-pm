#include "msys2db.h"
#include "tar.h"
#include "httpdownload.h"
#include "hex.h"
#include "exec.h"
extern "C" {
#include "sha256.h"
}
#include <zstd.h>
#include <stdio.h>
#include <string.h>
#include <unordered_set>
#include <algorithm>
#include <future>

static bool zstdDecompressBuffer(const std::vector<uint8_t> &compressed, std::vector<uint8_t> &decompressed);

// Strip version constraint from dependency string
// e.g. "perl>=5.14.0" -> "perl", "libcurl=8.18.0" -> "libcurl"
static std::string stripVersionConstraint(const std::string &dep)
{
  size_t pos = dep.find_first_of(">=<");
  if (pos != std::string::npos)
    return dep.substr(0, pos);
  return dep;
}

// Parse a single desc file content into CMsys2Package
static bool parseDesc(const std::string &content, CMsys2Package &pkg)
{
  std::string currentField;
  size_t pos = 0;
  size_t len = content.size();

  while (pos < len) {
    // Find end of line
    size_t eol = content.find('\n', pos);
    if (eol == std::string::npos)
      eol = len;
    std::string line(content, pos, eol - pos);
    pos = eol + 1;

    // Remove trailing \r
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line.empty()) {
      currentField.clear();
      continue;
    }

    // Check for field header: %FIELDNAME%
    if (line.size() >= 3 && line.front() == '%' && line.back() == '%') {
      currentField = line.substr(1, line.size() - 2);
      continue;
    }

    // Field value
    if (currentField == "NAME") {
      pkg.Name = line;
    } else if (currentField == "VERSION") {
      pkg.Version = line;
    } else if (currentField == "FILENAME") {
      pkg.Filename = line;
    } else if (currentField == "SHA256SUM") {
      pkg.Sha256 = line;
    } else if (currentField == "CSIZE") {
      pkg.CompressedSize = strtoull(line.c_str(), nullptr, 10);
    } else if (currentField == "DEPENDS") {
      pkg.Depends.push_back(line);
    } else if (currentField == "PROVIDES") {
      pkg.Provides.push_back(line);
    }
  }

  return !pkg.Name.empty() && !pkg.Filename.empty();
}

bool msys2ParseDatabase(const void *data, size_t size, std::map<std::string, CMsys2Package> &packages)
{
  // Decompress zstd
  std::vector<uint8_t> decompressed;
  if (!zstdDecompressBuffer({static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size}, decompressed))
    return false;

  // Parse tar
  std::vector<TarEntry> entries;
  if (!tarParse(decompressed.data(), decompressed.size(), entries)) {
    fprintf(stderr, "tar parse error\n");
    return false;
  }

  // Parse desc files
  for (const auto &entry : entries) {
    // Entries are like "git-2.52.0-2/desc"
    if (entry.Name.size() < 5)
      continue;
    size_t slash = entry.Name.find('/');
    if (slash == std::string::npos)
      continue;
    std::string filename = entry.Name.substr(slash + 1);
    if (filename != "desc")
      continue;

    CMsys2Package pkg;
    if (!parseDesc(entry.Data, pkg))
      continue;

    // Register by name
    std::string name = pkg.Name;
    packages[name] = pkg;

    // Register provides aliases
    for (const auto &provide : pkg.Provides) {
      std::string alias = stripVersionConstraint(provide);
      if (packages.find(alias) == packages.end())
        packages[alias] = pkg;
    }
  }

  return true;
}

bool msys2ResolveDependencies(const std::map<std::string, CMsys2Package> &packages,
                              const std::vector<std::string> &rootNames,
                              std::vector<const CMsys2Package*> &resolved)
{
  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> pending; // on stack but not yet resolved

  // Iterative DFS with two passes: first mark visiting, then add to resolved
  struct DfsEntry { std::string name; bool expanded; };
  std::vector<DfsEntry> dfsStack;
  for (auto it = rootNames.rbegin(); it != rootNames.rend(); ++it)
    dfsStack.push_back({*it, false});

  while (!dfsStack.empty()) {
    auto current = dfsStack.back();
    dfsStack.pop_back();

    std::string lookupName = stripVersionConstraint(current.name);

    // Resolve alias to canonical package name
    auto it = packages.find(lookupName);
    if (it == packages.end()) {
      if (!current.expanded)
        fprintf(stderr, "WARNING: package '%s' not found in database\n", lookupName.c_str());
      continue;
    }
    const std::string &name = it->second.Name;

    if (current.expanded) {
      // Second visit: add to result
      if (visited.count(name))
        continue;
      visited.insert(name);
      resolved.push_back(&it->second);
      continue;
    }

    if (visited.count(name) || pending.count(name))
      continue;

    pending.insert(name);

    // Push expanded marker
    dfsStack.push_back({name, true});

    // Push dependencies (in reverse order so first dep is processed first)
    const auto &deps = it->second.Depends;
    for (auto dit = deps.rbegin(); dit != deps.rend(); ++dit)
      dfsStack.push_back({*dit, false});
  }

  return true;
}

const std::vector<std::string> &msys2DefaultPackages()
{
  static const std::vector<std::string> defaults = {
    "msys2-runtime", "tar", "libgpgme", "wget", "zstd", "unzip", "patch", "git"
  };
  return defaults;
}

static bool readFile(const std::filesystem::path &path, std::vector<uint8_t> &data)
{
  FILE *f = fopen(path.string().c_str(), "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  data.resize(static_cast<size_t>(sz));
  size_t rd = fread(data.data(), 1, data.size(), f);
  fclose(f);
  return rd == data.size();
}

static bool writeFile(const std::filesystem::path &path, const std::vector<uint8_t> &data)
{
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  FILE *f = fopen(path.string().c_str(), "wb");
  if (!f)
    return false;
  size_t wr = fwrite(data.data(), 1, data.size(), f);
  fclose(f);
  return wr == data.size();
}

static std::string sha256hex(const void *data, size_t size)
{
  uint8_t hash[32];
  sha256(data, size, hash);
  char hex[65] = {0};
  bin2hexLowerCase(hash, hex, 32);
  return hex;
}

static bool zstdDecompressBuffer(const std::vector<uint8_t> &compressed, std::vector<uint8_t> &decompressed)
{
  ZSTD_DCtx *ctx = ZSTD_createDCtx();
  if (!ctx)
    return false;

  ZSTD_inBuffer input = { compressed.data(), compressed.size(), 0 };
  size_t const outChunkSize = ZSTD_DStreamOutSize();
  decompressed.clear();

  while (input.pos < input.size) {
    size_t oldSize = decompressed.size();
    decompressed.resize(oldSize + outChunkSize);
    ZSTD_outBuffer output = { decompressed.data() + oldSize, outChunkSize, 0 };
    size_t ret = ZSTD_decompressStream(ctx, &output, &input);
    if (ZSTD_isError(ret)) {
      fprintf(stderr, "zstd decompression error: %s\n", ZSTD_getErrorName(ret));
      ZSTD_freeDCtx(ctx);
      return false;
    }
    decompressed.resize(oldSize + output.pos);
  }

  ZSTD_freeDCtx(ctx);
  return true;
}

bool msys2Install(const std::filesystem::path &installDir,
                  const std::vector<std::string> &packageNames)
{
  const auto &names = packageNames.empty() ? msys2DefaultPackages() : packageNames;
  const size_t maxBatchBytes = 256 * 1024 * 1024; // 256 MB memory budget per batch

  static const std::string MSYS2_REPO = "https://repo.msys2.org/msys/x86_64/";

  std::error_code ec;
  std::filesystem::path sigDir = installDir / "msys2.sig";
  std::filesystem::create_directories(sigDir, ec);

  // Step 1: Download database (with signature-based caching)
  std::vector<uint8_t> dbData;
  {
    printf("Checking msys2 package database...\n");
    fflush(stdout);
    std::vector<uint8_t> remoteSig;
    if (!httpDownloadToMemory(MSYS2_REPO + "msys.db.sig", remoteSig)) {
      fprintf(stderr, "ERROR: failed to download msys.db.sig\n");
      return false;
    }

    std::vector<uint8_t> localSig;
    bool cached = false;
    if (readFile(sigDir / "msys.db.sig", localSig) && localSig == remoteSig) {
      if (readFile(sigDir / "msys.db", dbData)) {
        printf("Database unchanged, using cached copy\n");
        cached = true;
      }
    }

    if (!cached) {
      printf("Downloading msys2 package database...\n");
      fflush(stdout);
      if (!httpDownloadToMemory(MSYS2_REPO + "msys.db", dbData)) {
        fprintf(stderr, "ERROR: failed to download msys.db\n");
        return false;
      }
      writeFile(sigDir / "msys.db", dbData);
      writeFile(sigDir / "msys.db.sig", remoteSig);
    }
  }

  std::map<std::string, CMsys2Package> packages;
  if (!msys2ParseDatabase(dbData.data(), dbData.size(), packages)) {
    fprintf(stderr, "ERROR: failed to parse msys.db\n");
    return false;
  }
  printf("Database: %zu packages\n", packages.size());

  // Step 2: Resolve dependencies
  std::vector<const CMsys2Package*> resolved;
  if (!msys2ResolveDependencies(packages, names, resolved)) {
    fprintf(stderr, "ERROR: failed to resolve dependencies\n");
    return false;
  }
  printf("Resolved %zu packages\n", resolved.size());

  // Step 3: Download signatures in parallel, determine what needs install
  std::filesystem::create_directories(installDir, ec);

  struct PkgSigResult {
    std::vector<uint8_t> sig;
    bool ok;
  };

  std::vector<PkgSigResult> sigResults(resolved.size());
  std::vector<bool> needsInstall(resolved.size(), false);

  const size_t maxSigConcurrent = 32;
  printf("Checking package signatures...\n");
  fflush(stdout);
  for (size_t batch = 0; batch < resolved.size(); batch += maxSigConcurrent) {
    size_t batchEnd = std::min(batch + maxSigConcurrent, resolved.size());
    std::vector<std::future<PkgSigResult>> futures;

    for (size_t i = batch; i < batchEnd; i++) {
      std::string url = MSYS2_REPO + resolved[i]->Filename + ".sig";
      futures.push_back(std::async(std::launch::async, [url]() -> PkgSigResult {
        PkgSigResult r;
        r.ok = httpDownloadToMemory(url, r.sig);
        return r;
      }));
    }

    for (size_t j = 0; j < futures.size(); j++) {
      size_t i = batch + j;
      sigResults[i] = futures[j].get();
      if (!sigResults[i].ok) {
        fprintf(stderr, "ERROR: failed to download %s.sig\n", resolved[i]->Filename.c_str());
        return false;
      }

      std::vector<uint8_t> localSig;
      if (readFile(sigDir / (resolved[i]->Filename + ".sig"), localSig) && localSig == sigResults[i].sig) {
        // up to date
      } else {
        needsInstall[i] = true;
      }
    }
  }

  size_t toInstall = 0;
  for (bool b : needsInstall)
    if (b) toInstall++;
  printf("%zu to install, %zu up to date\n\n", toInstall, resolved.size() - toInstall);

  // Step 4: Download packages in parallel, extract sequentially
  // Collect indices of packages that need install
  std::vector<size_t> installIndices;
  for (size_t i = 0; i < resolved.size(); i++)
    if (needsInstall[i])
      installIndices.push_back(i);

  struct PkgDownloadResult {
    size_t index;
    std::vector<uint8_t> data;
    bool ok;
  };

  size_t dlCount = 0;
  size_t doneCount = 0;
  size_t pos = 0;
  while (pos < installIndices.size()) {
    // Build batch by memory budget (always include at least one package)
    size_t batchBytes = 0;
    size_t batchEnd = pos;
    while (batchEnd < installIndices.size()) {
      size_t pkgSize = resolved[installIndices[batchEnd]]->CompressedSize;
      if (batchEnd > pos && batchBytes + pkgSize > maxBatchBytes)
        break;
      batchBytes += pkgSize;
      batchEnd++;
    }

    std::vector<std::future<PkgDownloadResult>> futures;

    // Print and launch downloads
    for (size_t b = pos; b < batchEnd; b++) {
      size_t idx = installIndices[b];
      const auto &pkg = *resolved[idx];
      dlCount++;
      if (pkg.CompressedSize >= 1024 * 1024)
        printf("  [%zu/%zu] downloading %s-%s (%.1f MB)\n", dlCount, toInstall,
               pkg.Name.c_str(), pkg.Version.c_str(), pkg.CompressedSize / (1024.0 * 1024.0));
      else
        printf("  [%zu/%zu] downloading %s-%s (%.1f KB)\n", dlCount, toInstall,
               pkg.Name.c_str(), pkg.Version.c_str(), pkg.CompressedSize / 1024.0);
      fflush(stdout);
      std::string url = MSYS2_REPO + pkg.Filename;
      futures.push_back(std::async(std::launch::async, [url, idx]() -> PkgDownloadResult {
        PkgDownloadResult r;
        r.index = idx;
        r.ok = httpDownloadToMemory(url, r.data);
        return r;
      }));
    }

    // Wait for all downloads, collect results
    std::vector<PkgDownloadResult> results;
    for (auto &f : futures)
      results.push_back(f.get());

    // Sort by original index to maintain dependency order for extraction
    std::sort(results.begin(), results.end(),
              [](const PkgDownloadResult &a, const PkgDownloadResult &b) { return a.index < b.index; });

    // Extract sequentially in dependency order
    for (auto &r : results) {
      const auto &pkg = *resolved[r.index];
      doneCount++;
      printf("  [%zu/%zu] installing %s-%s\n", doneCount, toInstall, pkg.Name.c_str(), pkg.Version.c_str());
      fflush(stdout);

      if (!r.ok) {
        fprintf(stderr, "ERROR: failed to download %s\n", pkg.Filename.c_str());
        return false;
      }

      // Verify SHA256
      if (!pkg.Sha256.empty()) {
        std::string hash = sha256hex(r.data.data(), r.data.size());
        if (hash != pkg.Sha256) {
          fprintf(stderr, "ERROR: SHA256 mismatch for %s\n  expected: %s\n  got:      %s\n",
                  pkg.Filename.c_str(), pkg.Sha256.c_str(), hash.c_str());
          return false;
        }
      }

      // Decompress zstd
      std::vector<uint8_t> tarData;
      if (!zstdDecompressBuffer(r.data, tarData)) {
        fprintf(stderr, "ERROR: failed to decompress %s\n", pkg.Filename.c_str());
        return false;
      }

      // Extract tar
      if (!tarExtract(tarData.data(), tarData.size(), installDir)) {
        fprintf(stderr, "ERROR: failed to extract %s\n", pkg.Filename.c_str());
        return false;
      }

      // Save signature after successful install
      writeFile(sigDir / (pkg.Filename + ".sig"), sigResults[r.index].sig);
    }

    pos = batchEnd;
  }

#ifdef WIN32
  // Post-install: create tmp and etc/fstab for MSYS2 runtime
  std::filesystem::create_directories(installDir / "tmp", ec);
  {
    std::filesystem::path fstabPath = installDir / "etc" / "fstab";
    std::filesystem::create_directories(fstabPath.parent_path(), ec);
    FILE *f = fopen(fstabPath.string().c_str(), "wb");
    if (f) {
      fprintf(f, "none / cygdrive binary,posix=0,noacl,user 0 0\n");
      fprintf(f, "none /tmp usertemp binary,posix=0,noacl 0 0\n");
      fclose(f);
    }
  }

  // Post-install: update CA certificates
  printf("Running update-ca-trust...\n");
  fflush(stdout);
  {
    std::filesystem::path fullPath;
    std::string stdOut, stdErr;
    run(installDir / "usr" / "bin", "bash", {"update-ca-trust"}, {}, fullPath, stdOut, stdErr, false);
  }
#endif

  printf("\nInstalled %zu, up to date %zu (total %zu packages)\n",
         toInstall, resolved.size() - toInstall, resolved.size());
  return true;
}
