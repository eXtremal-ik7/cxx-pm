#include "strExtras.h"

static inline bool isDelimiter(char s, const char *delimiters)
{
  const char *dp = delimiters;
  while (*dp) {
    if (s == *dp)
      return true;
    dp++;
  }

  return false;
}

bool StringSplitter::next()
{
  if (EndReached_)
    return false;

  if (P_ != Input_) {
    P_++;
    Last_ = P_;
  }
  while (P_ != End_) {
    if (isDelimiter(*P_, Delimiters_)) {
      if (P_ != Last_)
        return true;
      Last_ = P_ + 1;
    }

    P_++;
  }

  EndReached_ = true;
  return Last_ != P_;
}

std::string_view StringSplitter::get()
{
  return std::string_view(Last_, P_ - Last_);
}
