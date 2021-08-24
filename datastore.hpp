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

#ifndef PSICASHLIB_DATASTORE_H
#define PSICASHLIB_DATASTORE_H

#include <string>
#include <mutex>
#include "error.hpp"
#include "utils.hpp"
#include "vendor/nonstd/expected.hpp"
#include "vendor/nlohmann/json.hpp"

namespace psicash {

/// Extremely simplistic key-value store.
/// Datastore operations are threadsafe.
class Datastore {
    using json = nlohmann::json;

public:
    enum class DatastoreGetError {
        kNotFound = 1,
        kTypeMismatch,
        kDatastoreUninitialized
    };

public:
    Datastore();

    /// Must be called exactly once.
    /// The fileRoot directory must already exist.
    /// suffix should be used to disambiguate different datastores. Optional (can be null).
    /// Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
    error::Error Init(const std::string& file_root, const std::string& suffix);

    /// Resets the in-memory structure and the persistent file, setting it to `new_value`
    /// (which may be an empty object).
    /// Calling this does not change the initialized state. If the datastore was already
    /// initialized with a different file_root+suffix, then the result is undefined.
    error::Error Reset(const std::string& file_root, const std::string& suffix, json new_value);

    /// Reset the in-memory structure and the persistent file, setting it to `new_value`
    /// (which may be an empty object).
    /// Calling this does not change the initialized state.
    /// Init() must have already been called, successfully.
    error::Error Reset(json new_value);

    /// Locks the read/write mutex and stops writing of updates to disk until
    /// EndTransaction is called. Transactions are re-enterable, but not nested.
    /// NOTE: Failing to call EndTransaction will result in undefined behaviour.
    void BeginTransaction();
    /// Ends an ongoing transaction writing. If commit is true, it writes the changes
    /// immediately; if false it discards the changes.
    /// Committing or rolling back inner transactions does nothing. Any errors during
    /// inner transactions that require the outermost transaction to be rolled back must
    /// be handled by the caller.
    error::Error EndTransaction(bool commit);

    /// Returns the value, or an error indicating the failure reason.
    template<typename T>
    nonstd::expected<T, DatastoreGetError> Get(const json::json_pointer& p) const {
        try {
            // Not returning inside the synchronize block to avoid compiler warning about
            // "control reached end of non-void function without returning a value".
            T val;
            SYNCHRONIZE_BLOCK(mutex_) {
                // Not using MUST_BE_INITIALIZED so we don't need it in the header.
                if (!initialized_) {
                    return nonstd::make_unexpected(DatastoreGetError::kDatastoreUninitialized);
                }

                if (p.empty() || !json_.contains(p)) {
                    return nonstd::make_unexpected(DatastoreGetError::kNotFound);
                }

                val = json_.at(p).get<T>();
            }
            return val;
        }
        catch (json::type_error&) {
            return nonstd::make_unexpected(DatastoreGetError::kTypeMismatch);
        }
        catch (json::out_of_range&) {
            // This should be avoided by the explicit check above. But we'll be safe.
            return nonstd::make_unexpected(DatastoreGetError::kNotFound);
        }
    }

    error::Result<nlohmann::json> Get() const;

    // Sets the value v in the datastore at path p.
    /// NOTE: Set is not atomic. If the file operation fails, the intermediate object will still be
    /// updated. We may want this to be otherwise in the future, but for now I think that it's preferable.
    /// Returns false if the file operation failed.
    error::Error Set(const json::json_pointer& p, json v);

protected:
    /// Helper for the public Reset methods
    error::Error Reset(const std::string& file_path, json new_value);

private:
    bool initialized_;

    mutable std::recursive_mutex mutex_;
    std::unique_lock<std::recursive_mutex> explicit_lock_;
    int transaction_depth_;

    std::string file_path_;
    json json_;
};

} // namespace psicash

#endif //PSICASHLIB_DATASTORE_H
