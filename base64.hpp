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

namespace base64 {

typedef unsigned char BYTE;

std::string B64Encode(const std::string& buf);
std::string B64Encode(const std::vector<BYTE>& buf);
std::string B64Encode(const BYTE* buf, unsigned int bufLen);

std::vector<BYTE> B64Decode(const std::string& b64encoded);

} // namespace base64

#endif //PSICASHLIB_BASE64_H
