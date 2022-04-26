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
#include <functional>
#include "datastore.hpp"
#include "utils.hpp"
#include "base64.hpp"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

namespace psicash {

using namespace std;
using namespace error;

static string FilePath(const string& file_root, const string& suffix);
static Result<json> LoadDatastore(const string& file_path);
static Error SaveDatastore(const string& file_path, const json& json);

Datastore::Datastore()
        : initialized_(false), explicit_lock_(mutex_, std::defer_lock),
          transaction_depth_(0), transaction_dirty_(false), json_(json::object()) {
}

Error Datastore::Init(const string& file_root, const string& suffix) {
    SYNCHRONIZE(mutex_);
    file_path_ = FilePath(file_root, suffix);
    auto res = LoadDatastore(file_path_);
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
    if (auto err = SaveDatastore(file_path, new_value)) {
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
        return PassError(SaveDatastore(file_path_, json_));
    }

    // We're rolling back -- revert to what's on disk
    auto res = LoadDatastore(file_path_);
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

Error Datastore::Set(const json::json_pointer& p, json v, bool write_store/*=true*/) {
    SYNCHRONIZE(mutex_);
    MUST_BE_INITIALIZED;

    // We will use the transaction mechanism to do the writing. It will also help prevent
    // changes to the stored value between the time we check it and the time we set it.
    if (write_store) {
        BeginTransaction();
    }

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

    if (write_store) {
        return PassError(EndTransaction(true));
    }
    return nullerr;
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

Additionally, two identical files are written: the "main" and "backup" datastore files.
These files contain the JSON an a checksum/hash of the JSON data. Then enables us to
detect and recover from file corruption.
(Unless both files are corrupted. But the the probability of that should be very low. And
if the storage device is so broken that multiple files are simultaneously getting corrupted,
then there's very little we can do.)
*/

static constexpr auto TEMP_EXT = ".temp";
static constexpr auto COMMIT_EXT = ".commit";
static constexpr auto BACKUP_EXT = ".2";
// Note that the "main" datastore file doesn't get a special extension for backwards
// compatiblity/migration reasons.

// Write the contents of a single datastore file. The checksum will be written after the
// contents, separated by an empty line. The contents must not contain an empty line.
static Error WriteFileContents(const string& file_path, const string& contents, const string& checksum) {
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
        f << contents << "\n\n" << checksum;
    }
    catch (std::exception& e) {
        return MakeCriticalError(utils::Stringer("file write failed: ", e.what()));
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

// Create a checksum for the given string (i.e., stringified JSON). This checksum is not
// portable between platforms. (Because the C++ hash implementation is likely different
// and because there's no endian-ness check. Local consistency is all that matters.)
static string ChecksumString(const string& s) {
    size_t hash = std::hash<std::string>{}(s);
    const size_t hash_size = sizeof(hash);

    // Convert the size_t value into a vector of bytes
    vector<uint8_t> checksum(hash_size);
    for (size_t i = 0; i < hash_size; i++) {
        checksum[hash_size-1-i] = (hash >> (i * 8));
    }

    return base64::B64Encode(checksum);
}

// Write the datastore to disk
static Error SaveDatastore(const string& file_path, const json& json) {
    string json_string;
    try {
        json_string = json.dump(-1, ' ',  // most compact representation
                                true);    // ensure ASCII
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("json dump failed: ", e.what(), "; id:", e.id));
    }

    // Calculate the datstore checksum
    auto checksum = ChecksumString(json_string);

    // Write the main datastore file
    auto err = WriteFileContents(file_path, json_string, checksum);
    if (err) {
        return WrapError(err, "failed to write main datastore file");
    }

    // Write the backup datastore file
    err = WriteFileContents(file_path + BACKUP_EXT, json_string, checksum);
    if (err) {
        return WrapError(err, "failed to write backup datastore file");
    }

    return nullerr;
}

struct DatastoreFileContents {
    string contents_;
    bool checksum_absent_;
};

// Read the contents of a single datastore file. Returns an error if the checksum doesn't
// match (but not if it's absent) or if the file contents are empty.
static Result<DatastoreFileContents> ReadFileContents(const string& file_path) {
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
        // Check that we can write here -- and initialize -- by storing an empty object.
        if (auto err = WriteFileContents(file_path, "{}", "")) {
            return WrapError(err, "file doesn't exist and FileStore failed");
        }

        // We'll continue on with the rest of the logic, which will read the new stub file.
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

    // When there's a checksum, it should be after the strigified JSON, separated by a
    // blank line. If there is no checksum (such as when migrating from a pre-checksum
    // datastore), then there must be no empty line before the end.
    vector<string> json_lines;
    string checksum_line;
    bool capture_checksum = false;
    try {
        for (string line; std::getline(f, line); ) {
            utils::Trim(line);

            if (line.length() == 0) {
                capture_checksum = true;
            }
            else if (capture_checksum) {
                checksum_line = line;
                break;
            }
            else {
                json_lines.push_back(line);
            }
        }
    }
    catch (std::exception& e) {
        return MakeCriticalError(utils::Stringer("file read failed: ", e.what()));
    }

    DatastoreFileContents res;
    res.contents_ = utils::Join(json_lines, "");

    if (res.contents_.empty()) {
        return MakeCriticalError("datastore file empty");
    }

    res.checksum_absent_ = true;
    if (!checksum_line.empty()) {
        res.checksum_absent_ = false;
        string actual_checksum = ChecksumString(res.contents_);
        if (checksum_line != actual_checksum) {
            return MakeCriticalError("datastore file checksum mismatch");
        }
    }

    return res;
}

// Load the datastore from disk
static Result<json> LoadDatastore(const string& file_path) {
    // Read the main datastore file
    auto file_contents_1 = ReadFileContents(file_path);
    // Read the backup datastore file
    auto file_contents_2 = ReadFileContents(file_path + BACKUP_EXT);

    // We won't use a file with a bad checksum, but we will use one with no checksum. This
    // allows us to cleanly migrate from pre-checksum datastores, and to test with edited
    // datastores. But we will still prefer a good checksum to a missing one.
    // When migrating a pre-checksum datastore, there will be no checksum in the first
    // file and only the stub empty JSON object (and no checksum) in the second file. In
    // order to ensure the older data is successfully migrated, we need to prefer the
    // first file.

    string json_string;
    if (!file_contents_1 && !file_contents_2) {
        return PassError(file_contents_1.error());
    }
    else if (!file_contents_1 || (file_contents_2 && !file_contents_2->checksum_absent_)) {
        // Either file_contents_1 is in an error state or file_contents_2 has a good checksum
        json_string = std::move(file_contents_2->contents_);
    }
    else {
        // If the checksum is absent, we prefer the main datastore file
        json_string = std::move(file_contents_1->contents_);
    }

    // At this point we know we have a non-empty json_string
    try {
        return json::parse(json_string);
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
    }
}

} // namespace psicash
