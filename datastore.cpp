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
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace psicash;
using namespace error;


Datastore::Datastore()
        : initialized_(false), json_(json::object()), paused_(false) {
}

static string FilePath(const string& file_root, const string& suffix) {
    return file_root + "/psicashdatastore" + suffix;
}

Error Datastore::Init(const string& file_root, const string& suffix) {
    SYNCHRONIZE(mutex_);
    file_path_ = FilePath(file_root, suffix);
    if (auto err = FileLoad(file_path_)) {
        return PassError(err);
    }
    initialized_ = true;
    return error::nullerr;
}

#define MUST_BE_INITIALIZED     if (!initialized_) { return MakeCriticalError("must only be called on an initialized datastore"); }

Error Datastore::Clear(const string& file_path) {
    SYNCHRONIZE(mutex_);
    json_ = json::object();
    return PassError(FileStore(file_path));
}

Error Datastore::Clear(const string& file_root, const string& suffix) {
    return PassError(Clear(FilePath(file_root, suffix)));
}

Error Datastore::Clear() {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    return PassError(Clear(file_path_));
}

void Datastore::PauseWrites() {
    SYNCHRONIZE(mutex_);
    paused_ = true;
}

Error Datastore::UnpauseWrites() {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    if (!paused_) {
        return nullerr;
    }
    paused_ = false;
    return FileStore(file_path_);
}

Error Datastore::Set(const json& in) {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    json_.update(in);
    return PassError(FileStore(file_path_));
}

Error Datastore::FileLoad(const string& file_path) {
    SYNCHRONIZE(mutex_);

    json_ = json::object();

    ifstream f;
    f.open(file_path, ios::binary);

    // Figuring out the cause of an open-file problem (i.e., file doesn't exist vs. filesystem is
    // broken) is annoying difficult to do robustly and in a cross-platform manner.
    // It seems like these state achieve approximately what we want.
    // For details see: https://en.cppreference.com/w/cpp/io/ios_base/iostate
    if (f.fail()) {
        // File probably doesn't exist. Check that we can write here.
        return WrapError(FileStore(file_path), "f.fail and FileStore failed");
    } else if (!f.good()) {
        return MakeCriticalError(utils::Stringer("not f.good; errno=", errno));
    }

    try {
        f >> json_;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json load failed: ", e.what(), "; id:", e.id));
    }

    return nullerr;
}

Error Datastore::FileStore(const string& file_path) {
    SYNCHRONIZE(mutex_);

    if (paused_) {
        return nullerr;
    }

    ofstream f;
    f.open(file_path, ios::trunc | ios::binary);
    if (!f.is_open()) {
        return MakeCriticalError(utils::Stringer("not f.is_open; errno=", errno));
    }

    try {
        f << json_;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json dump failed: ", e.what(), "; id:", e.id));
    }

    return nullerr;
}
