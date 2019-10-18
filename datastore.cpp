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
#include <cstdio>
#include "datastore.hpp"
#include "utils.hpp"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace psicash;
using namespace error;

static string FilePath(const string& file_root, const string& suffix);
static Result<json> FileLoad(const string& file_path);
static Error FileStore(bool paused, const string& file_path, const json& json);

Datastore::Datastore()
        : initialized_(false), json_(json::object()), paused_(false) {
}

Error Datastore::Init(const string& file_root, const string& suffix) {
    SYNCHRONIZE(mutex_);
    file_path_ = FilePath(file_root, suffix);
    auto res = FileLoad(file_path_);
    if (!res) {
        return PassError(res.error());
    }
    json_ = *res;
    initialized_ = true;
    return error::nullerr;
}

#define MUST_BE_INITIALIZED     if (!initialized_) { return MakeCriticalError("must only be called on an initialized datastore"); }

Error Datastore::Clear(const string& file_path) {
    SYNCHRONIZE(mutex_);
    paused_ = false;
    auto empty_json = json::object();
    if (auto err = FileStore(paused_, file_path, empty_json)) {
        return PassError(err);
    }
    json_ = empty_json;
    return error::nullerr;
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
    return PassError(FileStore(paused_, file_path_, json_));
}

Error Datastore::Set(const json& in) {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    json_.update(in);
    return PassError(FileStore(paused_, file_path_, json_));
}

static string FilePath(const string& file_root, const string& suffix) {
    return file_root + "/psicashdatastore" + suffix;
}

/*
More-robust file saving will be achieved like this...

When writing to file:
1. Write data to a new file `file_path.temp` (overwrite if exists)
2. Delete `file_path.commit`, if it exists (this should not happen, as the last read should have removed it)
3. Rename new file to `file_path.commit`
4. Delete existing `file_path` file
5. Rename `file_path.commit` to `file_path`

When reading from file:
1. Check if `file_path.commit` exists
  a. If so, delete `file_path`, if it exists
  b. Rename `file_path.commit` to `file_path`
2. Read `file_path`
*/

static constexpr auto TEMP_EXT = ".temp";
static constexpr auto COMMIT_EXT = ".commit";

static Result<json> FileLoad(const string& file_path) {
    const auto commit_file_path = file_path + COMMIT_EXT;

    // Do we have an existing commit file to promote?
    if (utils::FileExists(commit_file_path)) {
        int err;
        if (utils::FileExists(file_path) && (err = std::remove(file_path.c_str())) != 0) {
            return MakeCriticalError(utils::Stringer("removing file_path failed; err=", err, "; errno=", errno));
        }
        if ((err = std::rename(commit_file_path.c_str(), file_path.c_str())) != 0) {
            return MakeCriticalError(utils::Stringer("renaming commit_file_path to file_path failed; err=", err, "; errno=", errno));
        }
    }

    if (!utils::FileExists(file_path)) {
        // Check that we can write here by trying to store.
        auto empty_object = json::object();
        if (auto err = FileStore(false, file_path, empty_object)) {
            return WrapError(err, "file doesn't exist and FileStore failed");
        }

        // We'll continue on with the rest of the logic, which will read the new empty file.
    }

    ifstream f;
    f.open(file_path, ios::in | ios::binary);
    if (!f) {
        return MakeCriticalError(utils::Stringer("file open failed; errno=", errno));
    }

    json json;
    try {
        f >> json;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json load failed: ", e.what(), "; id:", e.id));
    }

    return json;
}

static Error FileStore(bool paused, const string& file_path, const json& json) {
    if (paused) {
        return nullerr;
    }

    const auto temp_file_path = file_path + TEMP_EXT;
    const auto commit_file_path = file_path + COMMIT_EXT;

    /*
    Write to the temp file
    */

    ofstream f;
    f.open(temp_file_path, ios::out | ios::trunc | ios::binary);
    if (!f.is_open()) {
        return MakeCriticalError(utils::Stringer("temp_file_path not f.is_open; errno=", errno));
    }

    try {
        f << json;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json dump failed: ", e.what(), "; id:", e.id));
    }

    f.close();

    /*
    Rename temp to commit
    */

    int err;
    if (utils::FileExists(commit_file_path) && (err = std::remove(commit_file_path.c_str())) != 0) {
        return MakeCriticalError(utils::Stringer("removing commit_file_path failed; err=", err, "; errno=", errno));
    }

    if ((err = std::rename(temp_file_path.c_str(), commit_file_path.c_str())) != 0) {
        return MakeCriticalError(utils::Stringer("renaming temp_file_path to commit_file_path failed; err=", err, "; errno=", errno));
    }

    /*
    Rename commit to datastore
    */

    if (utils::FileExists(file_path) && (err = std::remove(file_path.c_str())) != 0) {
        return MakeCriticalError(utils::Stringer("removing file_path failed; err=", err, "; errno=", errno));
    }

    if ((err = std::rename(commit_file_path.c_str(), file_path.c_str())) != 0) {
        return MakeCriticalError(utils::Stringer("renaming commit_file_path to file_path failed; err=", err, "; errno=", errno));
    }

    return nullerr;
}
