
#ifndef PSICASHLIB_URL_H
#define PSICASHLIB_URL_H

#include "error.h"

namespace psicash {

class URL {
public:
  error::Error Parse(const std::string& s);
  std::string ToString() const;

  // If `full` is true, the whole string will be percent-hex encoded, rather than allowing some
  // characters through unchanged.
  static std::string Encode(const std::string& s, bool full);

public:
  std::string scheme_host_path_;
  std::string query_;
  std::string fragment_;
};

} // namespace psicash

#endif //PSICASHLIB_URL_H
