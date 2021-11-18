#ifndef PSICASHLIB_TEST_HELPERS_H
#define PSICASHLIB_TEST_HELPERS_H

#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "userdata.hpp"

class TempDir
{
  public:
    TempDir() {}

  protected:
    int RandInt()
    {
        static bool rand_seeded = false;
        if (!rand_seeded)
        {
            std::srand(std::time(nullptr));
            rand_seeded = true;
        }
        return std::rand();
    }

    std::string GetTempDir()
    {
        // The first envvar is set by test.sh
        std::vector<std::string> env_vars = {"TEST_TEMP_DIR", "TMPDIR", "TMP", "TEMP", "TEMPDIR"};
        const char *tmp = nullptr;
        for (auto var : env_vars)
        {
            tmp = getenv(var.c_str());
            if (tmp)
            {
                break;
            }
        }

        if (!tmp)
        {
            tmp = "/tmp";
        }

        std::string res = tmp;
        res += "/" + std::to_string(RandInt());

#ifdef _MSC_VER
        auto rmrf = "rmdir /S /Q \"" + res + "\" > nul 2>&1";
        auto mkdirp = "mkdir \"" + res + "\"";
#else
        auto rmrf = "rm -rf " + res;
        auto mkdirp = "mkdir -p " + res;
#endif
        system(rmrf.c_str());
        system(mkdirp.c_str());

        return res;
    }

    std::string GetSuffix(bool dev) {
        return dev ? ".dev" : ".prod";
    }

    std::string DatastoreFilepath(const std::string& datastore_root, const char* suffix) {
        return datastore_root + "/psicashdatastore" + suffix;
    }

    std::string DatastoreFilepath(const std::string& datastore_root, const std::string& suffix) {
        return DatastoreFilepath(datastore_root, suffix.c_str());
    }

    std::string DatastoreFilepath(const std::string& datastore_root, bool dev) {
        return DatastoreFilepath(datastore_root, GetSuffix(dev));
    }

    bool Write(const std::string& datastore_root, bool dev, const std::string& s) {
        auto ds_file = DatastoreFilepath(datastore_root, dev);
        std::ofstream f;
        f.open(ds_file, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!f.is_open()) {
            return false;
        }
        f << s;
        f.close();
        return true;
    }

    bool WriteBadData(const std::string& datastore_root, bool dev)
    {
        auto ds_file = DatastoreFilepath(datastore_root, dev);
        auto make_bad_file = "echo nonsense > " + ds_file;
        return Write(datastore_root, dev, "this is bad data");
    }
};

int exec(const char* cmd, std::string& output);

// From https://stackoverflow.com/a/17976541
inline std::string trim(const std::string &s)
{
   auto wsfront=std::find_if_not(s.begin(),s.end(),[](int c){return std::isspace(c);});
   auto wsback=std::find_if_not(s.rbegin(),s.rend(),[](int c){return std::isspace(c);}).base();
   return (wsback<=wsfront ? std::string() : std::string(wsfront,wsback));
}

// Checks that two vectors have the same set of values, regardless of order.
template<typename T, typename U>
::testing::AssertionResult VectorSetsMatch(const std::vector<T>& expected, const std::vector<U>& actual, std::function<T(const U&)> trans) {
    if (expected.size() != actual.size()) {
            return ::testing::AssertionFailure() << " actual size (" << actual.size() << ") "
                << "not equal to expected size (" << expected.size() << ")";
    }

    // Using a copy of expected, so we can remove as we find items, to ensure that the full
    // sets match.
    std::vector<std::reference_wrapper<const T>> expected_copy;
    for (const auto& e : expected) {
        expected_copy.push_back(std::cref(e));
    }

    for (size_t i(0); i < actual.size(); ++i) {
        auto transformed_actual = trans(actual[i]);

        auto expected_found = std::find_if(
            expected_copy.begin(), expected_copy.end(),
            [&](const std::reference_wrapper<const T> &e) { return e.get() == transformed_actual; });

        if (expected_found == expected_copy.end()) {
            // If we ever use this helper function with a type of transformed_actual that
            // does not support `<<`, then we'll need to re-think this output.
            return ::testing::AssertionFailure() << " actual[" << i << "] (" << transformed_actual << ")"
                << "not found in expected";
        }

        expected_copy.erase(expected_found);
    }

    return ::testing::AssertionSuccess();
}

bool AuthTokenSetsEqual(const psicash::AuthTokens& at1, const psicash::AuthTokens& at2);

#endif // PSICASHLIB_TEST_HELPERS_H
