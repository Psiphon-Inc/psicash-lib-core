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
