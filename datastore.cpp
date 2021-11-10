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
#include <mutex>
#include "datastore.hpp"
#include "utils.hpp"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

namespace psicash {

using namespace std;
using namespace error;

static string FilePath(const string& file_root, const string& suffix);
static Result<json> FileLoad(const string& file_path);
static Error FileStore(const string& file_path, const json& json);

Datastore::Datastore()
        : initialized_(false), explicit_lock_(mutex_, std::defer_lock),
          transaction_depth_(0), transaction_dirty_(false), json_(json::object()) {
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

Error Datastore::Reset(const string& file_path, json new_value) {
    SYNCHRONIZE(mutex_);
    transaction_depth_ = 0;
    transaction_dirty_ = false;
    if (auto err = FileStore(file_path, new_value)) {
        return PassError(err);
    }
    json_ = new_value;
    return error::nullerr;
}

Error Datastore::Reset(const string& file_root, const string& suffix, json new_value) {
    return PassError(Reset(FilePath(file_root, suffix), new_value));
}

Error Datastore::Reset(json new_value) {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    return PassError(Reset(file_path_, new_value));
}

void Datastore::BeginTransaction() {
    // We only acquire a non-local lock if we're starting an outermost transaction.
    SYNCHRONIZE(mutex_);
    // We got a local lock, so we know there's no transaction in progress in any other thread.
    if (transaction_depth_ == 0) {
        transaction_dirty_ = false;
        explicit_lock_.lock();
    }
    transaction_depth_++;
}

Error Datastore::EndTransaction(bool commit) {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    if (transaction_depth_ <= 0) {
        assert(false);
        return nullerr;
    }

    transaction_depth_--;

    if (transaction_depth_ > 0) {
        // This was an inner transaction and there's nothing more to do.
        return nullerr;
    }

    // We need to release the explicit lock on exit from this function, no matter what.
    // We will "adopt" the lock into this lock_guard to ensure the unlock happens when it goes out of scope.
    std::lock_guard<std::unique_lock<std::recursive_mutex>> lock_releaser(explicit_lock_, std::adopt_lock);

    if (!transaction_dirty_) {
        // No actual substantive changes were made during this transaction, so we will avoid
        // writing to disk. Committing and rolling back are no-ops if there are no changes.
        return nullerr;
    }

    if (commit) {
        return PassError(FileStore(file_path_, json_));
    }

    // We're rolling back -- revert to what's on disk
    auto res = FileLoad(file_path_);
    if (!res) {
        return PassError(res.error());
    }
    json_ = *res;
    return nullerr;
}

error::Result<nlohmann::json> Datastore::Get() const {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;
    return json_;
}

Error Datastore::Set(const json::json_pointer& p, json v) {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;

    // We will use the transaction mechanism to do the writing. It will also help prevent
    // changes to the stored value between the time we check it and the time we set it.
    BeginTransaction();

    // Avoid modifying the datastore if the value is the same as what's already there.
    bool changed = true;
    try {
        changed = (json_.at(p) != v);
    }
    catch (json::out_of_range&) {
        // The key doesn't exist, so continue to set it.
    }

    json_[p] = v;
    transaction_dirty_ = changed;

    return PassError(EndTransaction(true));
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
        if (auto err = FileStore(file_path, empty_object)) {
            return WrapError(err, "file doesn't exist and FileStore failed");
        }

        // We'll continue on with the rest of the logic, which will read the new empty file.
    }

    uint64_t file_size = 0;
    if (auto err = utils::FileSize(file_path, file_size)) {
        return WrapError(err, utils::Stringer("unable to get file size; errno=", errno));
    }
    if (file_size == 0) {
        return MakeCriticalError("file size is zero");
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

static Error FileStore(const string& file_path, const json& json) {
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

    if (f.fail()) {
        return MakeCriticalError(utils::Stringer("temp_file_path close failed; errno=", errno));
    }

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

} // namespace psicash
