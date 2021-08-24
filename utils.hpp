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

#ifndef PSICASHLIB_UTILS_H
#define PSICASHLIB_UTILS_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iterator>
#include "error.hpp"


namespace utils {

// From https://stackoverflow.com/a/25386444/729729
/// Can be used like `s = Stringer("lucky ", 42, '!');
template<typename T>
std::string Stringer(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}
template<typename T, typename ... Args>
std::string Stringer(const T& value, const Args& ... args) {
    return Stringer(value) + Stringer(args...);
}

// From https://stackoverflow.com/a/43894724/729729
/// Synchronizes access to a block, using the given mutex. Used like:
/// SYNCHRONIZE_BLOCK(mutex_) {
///     ...do stuff
/// }
#define SYNCHRONIZE_BLOCK(m) for(std::unique_lock<std::recursive_mutex> lk(m); lk; lk.unlock())
/// Synchronize the current scope using the given mutex.
#define SYNCHRONIZE(m) std::lock_guard<std::recursive_mutex> synchronize_lock(m)

/// Tests if the given filepath+name exists.
bool FileExists(const std::string& filename);

/// Gets the size of the file at the given path.
psicash::error::Error FileSize(const std::string& path, uint64_t& o_size);

/// Generates a large random ID.
std::string RandomID();

/// Finds the value of the header with the given key in `headers` (case-insensitive).
/// Returns the value if found, or empty string if not found.
/// If there are multiple header values for the key, the first one is returned.
std::string FindHeaderValue(const std::map<std::string, std::vector<std::string>>& headers, const std::string& key);

// From https://stackoverflow.com/a/5289170/729729
/// note: delimiter cannot contain NUL characters
template <typename Range, typename Value = typename Range::value_type>
std::string Join(Range const& elements, const char *const delimiter) {
    std::ostringstream os;
    auto b = begin(elements), e = end(elements);

    if (b != e) {
        std::copy(b, prev(e), std::ostream_iterator<Value>(os, delimiter));
        b = prev(e);
    }
    if (b != e) {
        os << *b;
    }

    return os.str();
}

// From https://stackoverflow.com/a/5289170/729729
/// note: imput is assumed to not contain NUL characters
template <typename Input, typename Output, typename Value = typename Output::value_type>
void Split(char delimiter, Output &output, Input const& input) {
    using namespace std;
    for (auto cur = begin(input), beg = cur; ; ++cur) {
        if (cur == end(input) || *cur == delimiter || !*cur) {
            output.insert(output.end(), Value(beg, cur));
            if (cur == end(input) || !*cur)
                break;
            else
                beg = next(cur);
        }
    }
}

}

#endif //PSICASHLIB_UTILS_H
