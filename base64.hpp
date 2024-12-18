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

#ifndef PSICASHLIB_BASE64_H
#define PSICASHLIB_BASE64_H

#include <vector>
#include <string>
#include <cstdint>

namespace base64 {

std::string B64Encode(const std::string& buf);
std::string B64Encode(const std::vector<uint8_t>& buf);
std::string B64Encode(const uint8_t* buf, unsigned int bufLen);

std::vector<uint8_t> B64Decode(const std::string& b64encoded);

std::string TrimPadding(const std::string& s);

} // namespace base64

#endif //PSICASHLIB_BASE64_H
