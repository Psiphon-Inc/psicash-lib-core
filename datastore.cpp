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

#include <iostream>
#include <fstream>

#include "datastore.hpp"
#include "utils.hpp"

#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace psicash;
using namespace error;


Datastore::Datastore()
        : json_(json::object()) {
}

Error Datastore::Init(const char* file_root) {
    SYNCHRONIZE(mutex_);
    file_path_ = string(file_root) + "/datastore";
    return PassError(FileLoad());
}

void Datastore::Clear() {
    SYNCHRONIZE(mutex_);
    json_ = json::object();
    FileStore();
}

void Datastore::PauseWrites() {
    SYNCHRONIZE(mutex_);
    paused_ = true;
}

error::Error Datastore::UnpauseWrites() {
    SYNCHRONIZE(mutex_);
    if (!paused_) {
        return nullerr;
    }
    paused_ = false;
    return FileStore();
}

Error Datastore::Set(const json& in) {
    SYNCHRONIZE(mutex_);
    json_.update(in);
    return PassError(FileStore());
}

Error Datastore::FileLoad() {
    SYNCHRONIZE(mutex_);

    json_ = json::object();

    ifstream f;
    f.open(file_path_, ios::binary);

    // Figuring out the cause of an open-file problem (i.e., file doesn't exist vs. filesystem is
    // broken) is annoying difficult to do robustly and in a cross-platform manner.
    // It seems like these state achieve approximately what we want.
    // For details see: https://en.cppreference.com/w/cpp/io/ios_base/iostate
    if (f.fail()) {
        // File probably doesn't exist. Check that we can write here.
        return WrapError(FileStore(), "f.fail and FileStore failed");
    } else if (!f.good()) {
        return MakeCriticalError(utils::Stringer("not f.good; errno=", errno).c_str());
    }

    try {
        f >> json_;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json load failed: ", e.what(), "; id:", e.id).c_str());
    }

    return nullerr;
}

Error Datastore::FileStore() {
    SYNCHRONIZE(mutex_);

    if (paused_) {
        return nullerr;
    }

    ofstream f;
    f.open(file_path_, ios::trunc | ios::binary);
    if (!f.is_open()) {
        return MakeCriticalError(utils::Stringer("not f.is_open; errno=", errno).c_str());
    }

    try {
        f << json_;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json dump failed: ", e.what(), "; id:", e.id).c_str());
    }

    return nullerr;
}
