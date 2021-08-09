#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include "test_helpers.hpp"

// Adapted from https://stackoverflow.com/a/478960
int exec(const char* cmd, std::string& output) {
    output.clear();
    std::array<char, 128> buffer;
    bool closed = false;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), [&closed](FILE* p){if (!closed) pclose(p);});
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            output += buffer.data();
    }
    int result = pclose(pipe.get());
    closed = true;
    // pclose will be called again as the shared_ptr deleter, but that's okay
    return result;
}

bool AuthTokenSetsEqual(const psicash::AuthTokens& at1, const psicash::AuthTokens& at2) {
    if (at1.size() != at2.size()) {
        return false;
    }

    for (const auto& map_it1 : at1) {
        const auto& val_it2 = at2.find(map_it1.first);
        if (val_it2 == at2.end()) {
            return false;
        }

        if (val_it2->second.id != map_it1.second.id) {
            return false;
        }

        if (val_it2->second.server_time_expiry.has_value() != map_it1.second.server_time_expiry.has_value()) {
            return false;
        }

        if (val_it2->second.server_time_expiry.has_value()
            && !(*(val_it2->second.server_time_expiry) == *(map_it1.second.server_time_expiry))) {
            return false;
        }
    }

    return true;
}
