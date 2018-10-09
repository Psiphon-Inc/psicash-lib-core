#include <sstream>
#include <memory>
#include "error.h"

using namespace std;

namespace error {

Error::Error()
  : is_error_(false) {
}

Error::Error(const Error& src)
    : is_error_(src.is_error_), stack_(src.stack_) {
}

Error::Error(const char* message, const char* filename, const char* function, int line)
  : is_error_(true) {
  Wrap(message, filename, function, line);
}

Error& Error::Wrap(const char* message, const char* filename, const char* function, int line) {
  if (!is_error_) {
    // This is a non-error, so there's nothing to wrap.
    return *this;
  }

  // We don't want the full absolute file path.
  string f = filename ? filename : "";
  auto last_slash = f.rfind("/");
  if (last_slash != string::npos) {
    f = f.substr(last_slash+1);
  }

  stack_.push_back({message ? message : "", f, function ? function : "", line});
  return *this;
}

Error& Error::Wrap(const char* filename, const char* function, int line) {
  return Wrap(nullptr, filename, function, line);
}

Error::operator bool() const {
  return is_error_;
}

string Error::ToString() const {
  if (!is_error_) {
    return "(nonerror)";
  }

  ostringstream os;
  for (const auto& sf : stack_) {
    os << sf.message << " (" << sf.filename << ":" << sf.function << ":" << sf.line << ")" << endl;
  }

  return os.str();
}

} // namespace error
