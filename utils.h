#ifndef PSICASHLIB_UTILS_H
#define PSICASHLIB_UTILS_H

#include <string>
#include <sstream>


namespace utils {

// From https://stackoverflow.com/a/25386444/729729
// Can be used like `s = Stringer("lucky ", 42, '!');
template<typename T>
std::string Stringer(const T& value)
{
  std::ostringstream oss;
  oss << value;
  return oss.str();
}
template<typename T, typename ... Args >
std::string Stringer(const T& value, const Args& ... args)
{
  return Stringer(value) + Stringer(args...);
}

}

#endif //PSICASHLIB_UTILS_H
