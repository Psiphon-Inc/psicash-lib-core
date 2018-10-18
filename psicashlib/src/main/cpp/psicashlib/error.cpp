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

Error::Error(const std::string& message, const std::string& filename,
             const std::string& function, int line)
        : is_error_(true) {
    Wrap(message, filename, function, line);
}

Error& Error::Wrap(const std::string& message, const std::string& filename,
                   const std::string& function, int line) {
    if (!is_error_) {
        // This is a non-error, so there's nothing to wrap.
        return *this;
    }

    // We don't want the full absolute file path.
    string f = filename;
    auto last_slash = f.find_last_of("/\\");
    if (last_slash != string::npos) {
        f = f.substr(last_slash + 1);
    }

    stack_.push_back({message, f, function, line});
    return *this;
}

Error& Error::Wrap(const std::string& filename, const std::string& function, int line) {
    return Wrap("", filename, function, line);
}

Error::operator bool() const {
    return is_error_;
}

string Error::ToString() const {
    if (!is_error_) {
        return "(nonerror)";
    }

    bool first = true;
    ostringstream os;
    for (const auto& sf : stack_) {
        if (!first) {
            os << endl;
        }
        first = false;

        os << sf.message << " (" << sf.filename << ":" << sf.function << ":" << sf.line << ")";
    }

    return os.str();
}

std::ostream& operator<<(std::ostream& os, const Error& err)
{
    os << err.ToString();
    return os;
}

} // namespace error
