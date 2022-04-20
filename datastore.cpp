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

// Create a checksum for the given JSON. This checksum is not portable between platforms.
// (Because the C++ hash implementation is likely different and because there's no
// endian-ness check. Local consistency is all that matters.)
static Result<vector<uint8_t>> ChecksumJSON(const json& json) {
    // nlohmann::json has its own std::hash specialization
    size_t hash = std::hash<nlohmann::json>{}(json);
    const size_t hash_size = sizeof(hash);

    // Convert the size_t value into a vector of bytes
    vector<uint8_t> res(hash_size);
    for (int i = 0; i < hash_size; i++) {
        res[hash_size-1-i] = (hash >> (i * 8));
    }

    return res;
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

// Write the contents of a single datastore file
static Error WriteJSONFileContents(const string& file_path, const json& json, const string& checksum) {
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

    // Write the JSON data
    try {
        f << json;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json dump failed: ", e.what(), "; id:", e.id));
    }

    // Write the checksum
    f << "\n" << checksum << "\n";

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

// Write the datastore to disk
static Error SaveDatastore(const string& file_path, const json& json) {
    // Calculate the datstore checksum
    auto checksum = ChecksumJSON(json);
    if (!checksum) {
        return PassError(checksum.error());
    }
    auto checksum_string = base64::B64Encode(*checksum);

    // Write the main datastore file
    auto err = WriteJSONFileContents(file_path, json, checksum_string);
    if (err) {
        return WrapError(err, "failed to write main datastore file");
    }

    // Write the backup datastore file
    err = WriteJSONFileContents(file_path + BACKUP_EXT, json, checksum_string);
    if (err) {
        return WrapError(err, "failed to write backup datastore file");
    }

    return nullerr;
}

struct DatastoreFileContents {
    json json_;
    bool checksum_match_;
};

// Read the contents of a single datastore file
static Result<DatastoreFileContents> ReadJSONFileContents(const string& file_path) {
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
        if (auto err = WriteJSONFileContents(file_path, empty_object, "")) {
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

    DatastoreFileContents res;
    res.checksum_match_ = false;

    // Read the JSON
    try {
        f >> res.json_;
    }
    catch (json::exception& e) {
        return MakeCriticalError(utils::Stringer("json load failed: ", e.what(), "; id:", e.id));
    }

    // Reading the JSON from the file would have stopped at the end of the object, so the
    // file pointer is now at the beginning of our checksum (give or take some
    // whitespace). If there is no checksum after the JSON (such as when migrating from a
    // pre-checksum datastore), file_checksum_string and file_checksum will be empty.
    // Later logic will handle that. (Note that this accidentally enables client version
    // downgrading as well. The old code will read the JSON and then stop.)
    vector<uint8_t> file_checksum;
    // Under the hood this uses std::basic_istream::rdbuf, which "may throw implementation-defined exceptions".
    // We don't want datastore access problems to crash the app, so we'll be careful about reading it.
    try {
        std::string file_checksum_string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        utils::Trim(file_checksum_string);
        file_checksum = base64::B64Decode(file_checksum_string);
    }
    catch (std::exception& e) {
        return MakeCriticalError(utils::Stringer("checksum read failed: ", e.what()));
    }

    auto json_checksum = ChecksumJSON(res.json_);
    if (json_checksum && json_checksum->size() == file_checksum.size()) {
        // Compare the bytes of the checksums
        bool mismatch = false;
        for (size_t i = 0; i < file_checksum.size(); i++) {
            if (file_checksum[i] != json_checksum->at(i)) {
                mismatch = true;
                break;
            }
        }
        res.checksum_match_ = !mismatch;
    }

    return res;
}

// Load the datastore from disk
static Result<json> LoadDatastore(const string& file_path) {
    // Read the main datastore file
    auto file_contents_1 = ReadJSONFileContents(file_path);
    // Read the backup datastore file
    auto file_contents_2 = ReadJSONFileContents(file_path + BACKUP_EXT);

    // If we failed to read either file, then we have an unavoidable error
    if (!file_contents_1 && !file_contents_2) {
        return PassError(file_contents_1.error());
    }

    // If one file could be read and the other couldn't, use the one that could.
    // Don't bother checking if the checksum matched. We'll hope that any corruption is
    // minor and recoverable.
    if (file_contents_1 && !file_contents_2) {
        return file_contents_1->json_;
    }
    else if (!file_contents_1 && file_contents_2) {
        return file_contents_2->json_;
    }

    // When migrating a pre-checksum datastore, there will be no checksum in the first
    // file and only the stub empty JSON object (and no checksum) in the second file. In
    // order to ensure the older data is successfully migrated, we need to fall back to
    // the first file.

    // Use file_contents_2 if its checksum matches.
    if (file_contents_2->checksum_match_) {
        return file_contents_2->json_;
    }
    // Otherwise use the contents of file_contents_1, regardless of checksum.
    return file_contents_1->json_;
}

} // namespace psicash
