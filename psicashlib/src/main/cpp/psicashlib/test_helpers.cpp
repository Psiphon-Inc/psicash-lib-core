#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include "test_helpers.h"

bool InRange(int64_t target, int64_t low, int64_t high) {
    return (low <= target) && (target <= high);
}

bool IsNear(int64_t target, int64_t comparator, int64_t wiggle) {
    return InRange(target, comparator-wiggle, comparator+wiggle);
}

// From https://stackoverflow.com/a/478960
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}
