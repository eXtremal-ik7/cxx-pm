#pragma once

#include <string>
#include <vector>

struct CVersionPart {
  int Number;
  std::string Suffix;
};

std::vector<CVersionPart> parseVersion(const std::string &version);
int compareVersionParts(const CVersionPart &p1, const CVersionPart &p2);
bool versionMatchesWildcard(const std::string &version, const std::string &pattern);
int compareVersions(const std::string &v1, const std::string &v2);
