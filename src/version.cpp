#include "version.h"

std::vector<CVersionPart> parseVersion(const std::string &version)
{
  std::vector<CVersionPart> parts;
  std::string current;

  auto parsePart = [](const std::string &s) -> CVersionPart {
    CVersionPart part = {0, ""};
    size_t i = 0;
    // Parse numeric prefix
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      part.Number = part.Number * 10 + (s[i] - '0');
      i++;
    }
    // Rest is suffix
    if (i < s.size())
      part.Suffix = s.substr(i);
    return part;
  };

  for (char c : version) {
    if (c == '.') {
      if (!current.empty()) {
        parts.push_back(parsePart(current));
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty())
    parts.push_back(parsePart(current));

  return parts;
}

int compareVersionParts(const CVersionPart &p1, const CVersionPart &p2)
{
  if (p1.Number < p2.Number) return -1;
  if (p1.Number > p2.Number) return 1;
  if (p1.Suffix < p2.Suffix) return -1;
  if (p1.Suffix > p2.Suffix) return 1;
  return 0;
}

bool versionMatchesWildcard(const std::string &version, const std::string &pattern)
{
  // Pattern like "1.*" or "1.5.*" or "0.9*"
  size_t starPos = pattern.find('*');
  if (starPos == std::string::npos)
    return version == pattern;

  std::string prefix = pattern.substr(0, starPos);
  if (prefix.empty())
    return true;  // "*" matches everything

  // Check if pattern ends with "." before "*" (e.g. "1.*")
  bool dotBeforeStar = !prefix.empty() && prefix.back() == '.';
  if (dotBeforeStar)
    prefix.pop_back();

  std::vector<CVersionPart> patternParts = parseVersion(prefix);
  std::vector<CVersionPart> versionParts = parseVersion(version);

  if (versionParts.size() < patternParts.size())
    return false;

  for (size_t i = 0; i < patternParts.size(); i++) {
    // For last pattern part without dot before star (e.g. "0.9*"),
    // only compare the numeric part as prefix
    if (i == patternParts.size() - 1 && !dotBeforeStar) {
      if (versionParts[i].Number != patternParts[i].Number)
        return false;
      // Suffix can be anything
    } else {
      if (compareVersionParts(versionParts[i], patternParts[i]) != 0)
        return false;
    }
  }
  return true;
}

int compareVersions(const std::string &v1, const std::string &v2)
{
  std::vector<CVersionPart> parts1 = parseVersion(v1);
  std::vector<CVersionPart> parts2 = parseVersion(v2);

  size_t maxLen = std::max(parts1.size(), parts2.size());
  for (size_t i = 0; i < maxLen; i++) {
    CVersionPart p1 = i < parts1.size() ? parts1[i] : CVersionPart{0, ""};
    CVersionPart p2 = i < parts2.size() ? parts2[i] : CVersionPart{0, ""};
    int cmp = compareVersionParts(p1, p2);
    if (cmp != 0)
      return cmp;
  }
  return 0;
}
