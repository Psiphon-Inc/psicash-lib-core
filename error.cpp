/*
 * Copyright (c) 2018, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <sstream>
#include <memory>
#include "error.hpp"

using namespace std;

namespace error {

Error::Error()
        : is_error_(false), critical_(false) {
}

Error::Error(const Error& src)
        : is_error_(src.is_error_), critical_(src.critical_), stack_(src.stack_) {
}

Error::Error(bool critical, const std::string& message, const std::string& filename,
             const std::string& function, int line)
        : is_error_(true), critical_(critical) {
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

string Error::ToString() const {
    if (!is_error_) {
        return "(nonerror)";
    }

    bool first = true;
    ostringstream os;

    if (Critical()) {
        os << "CRITICAL: ";
    }

    for (const auto& sf : stack_) {
        if (!first) {
            os << endl;
        }
        first = false;

        os << sf.message << " (" << sf.filename << ":" << sf.function << ":" << sf.line << ")";
    }

    return os.str();
}

std::ostream& operator<<(std::ostream& os, const Error& err) {
    os << err.ToString();
    return os;
}

} // namespace error
