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
#include <sstream>


namespace utils {

// From https://stackoverflow.com/a/25386444/729729
// Can be used like `s = Stringer("lucky ", 42, '!');
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
#define SYNCHRONIZE(m) std::unique_lock<std::recursive_mutex> synchronize_lock(m)

}

#endif //PSICASHLIB_UTILS_H
