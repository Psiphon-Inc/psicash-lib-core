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
#include "vendor/nonstd/expected.hpp"
#include "vendor/nlohmann/json.hpp"

namespace psicash {

/// Extremely simplistic key-value store.
/// Datastore operations are threadsafe.
class Datastore {
    using json = nlohmann::json;

public:
    enum DatastoreGetError {
        kNotFound = 1,
        kTypeMismatch
    };

public:
    Datastore();

    /// Must be called exactly once.
    /// The fileRoot directory must already exist.
    /// Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
    error::Error Init(const char* file_root);

    /// Clears the in-memory structure and the persistent file.
    /// Primarily intended for debugging purposes.
    void Clear();

    /// Stops writing of updates to disk until UnpauseWrites is called.
    void PauseWrites();
    /// Unpauses writing and causes an immediate write.
    error::Error UnpauseWrites();

    /// Returns the value, or an error indicating the failure reason.
    template<typename T>
    nonstd::expected<T, DatastoreGetError> Get(const char* key) const {
        try {
            if (json_.find(key) == json_.end()) {
                return nonstd::make_unexpected(kNotFound);
            }

            return json_[key].get<T>();
        }
        catch (json::type_error& e) {
            return nonstd::make_unexpected(kTypeMismatch);
        }
        catch (json::out_of_range& e) {
            // This should be avoided by the explicit check above. But we'll be safe.
            return nonstd::make_unexpected(kNotFound);
        }
    }

    /// To set a single key-value: `set({{"k1", "v1"}})`.
    /// To set multiple key-values: `set({{"k1", "v1"}, {"k2", "v2"}})`.
    /// NOTE: If you use too few curly braces, you'll accidentally create arrays instead of objects.
    /// NOTE: Set is not atomic. If the file operation fails, the intermediate object will still be
    /// updated. We may want this to be otherwise in the future, but for now I think that it's preferable.
    /// Returns false if the file operation failed.
    error::Error Set(const json& in);

protected:
    error::Error FileLoad();
    error::Error FileStore();

private:
    std::recursive_mutex mutex_;
    std::string file_path_;
    json json_;
    bool paused_;
};

} // namespace psicash

#endif //PSICASHLIB_DATASTORE_H
