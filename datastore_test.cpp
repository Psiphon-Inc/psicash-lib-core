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

#include <cstdlib>
#include <ctime>

#include "gtest/gtest.h"
#include "test_helpers.hpp"
#include "datastore.hpp"

using namespace std;
using namespace psicash;
using json = nlohmann::json;

class TestDatastore : public ::testing::Test, public TempDir
{
  public:
    TestDatastore() {}
};

TEST_F(TestDatastore, InitSimple)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);
}

TEST_F(TestDatastore, InitCorrupt)
{
    auto temp_dir = GetTempDir();
    WriteBadData(temp_dir.c_str());

    Datastore ds;
    auto err = ds.Init(temp_dir.c_str());
    ASSERT_TRUE(err);
    ASSERT_GT(err.ToString().length(), 0);
}

TEST_F(TestDatastore, InitBadDir)
{
    auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
    Datastore ds1;
    auto err = ds1.Init(bad_dir.c_str());
    ASSERT_TRUE(err);

    bad_dir = "/";
    Datastore ds2;
    err = ds2.Init(bad_dir.c_str());
    ASSERT_TRUE(err);
}

TEST_F(TestDatastore, CheckPersistence)
{
    // We will create an instance, destroy it, create a new one with the same data directory, and
    // check that it contains our previous data.

    auto temp_dir = GetTempDir();

    auto ds = new Datastore();
    auto err = ds->Init(temp_dir.c_str());
    ASSERT_FALSE(err);

    string want = "v";
    err = ds->Set({{"k", want}});
    ASSERT_FALSE(err);

    auto got = ds->Get<string>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want);

    // Destroy/close the datastore
    delete ds;

    // Create a new one and check that it has the same data.
    ds = new Datastore();
    err = ds->Init(temp_dir.c_str());
    ASSERT_FALSE(err);

    got = ds->Get<string>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want);

    delete ds;
}

TEST_F(TestDatastore, WritePause)
{
    auto temp_dir = GetTempDir();

    auto ds = new Datastore();
    auto err = ds->Init(temp_dir.c_str());
    ASSERT_FALSE(err);

    // This should persist
    string pause_want1 = "pause_want1";
    err = ds->Set({{pause_want1, pause_want1}});
    ASSERT_FALSE(err);

    // This should persist
    ds->PauseWrites();
    string pause_want2 = "pause_want2";
    err = ds->Set({{pause_want2, pause_want2}});
    ASSERT_FALSE(err);
    err = ds->UnpauseWrites();
    ASSERT_FALSE(err);

    // This should NOT persist, since we'll close before unpausing
    ds->PauseWrites();
    string pause_want3 = "pause_want3";
    err = ds->Set({{pause_want3, pause_want3}});
    ASSERT_FALSE(err);

    // Close
    delete ds;

    // Reopen
    ds = new Datastore();
    err = ds->Init(temp_dir.c_str());
    ASSERT_FALSE(err);

    auto got = ds->Get<string>(pause_want1.c_str());
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, pause_want1);

    got = ds->Get<string>(pause_want2.c_str());
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, pause_want2);

    got = ds->Get<string>(pause_want3.c_str());
    ASSERT_FALSE(got);

    delete ds;
}

TEST_F(TestDatastore, SetSimple)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    string want = "v";
    err = ds.Set({{"k", want}});
    ASSERT_FALSE(err);

    auto got = ds.Get<string>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want);
}

TEST_F(TestDatastore, SetMulti)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    const char *key1 = "key1", *key2 = "key2";
    string want1 = "want1", want2 = "want2";
    err = ds.Set({{key1, want1}, {key2, want2}});
    ASSERT_FALSE(err);

    auto got = ds.Get<string>(key1);
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want1);

    got = ds.Get<string>(key2);
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want2);
}

TEST_F(TestDatastore, SetDeep)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    const char *key1 = "key1", *key2 = "key2";
    string want = "want";
    err = ds.Set({{key1, {{key2, want}}}});
    ASSERT_FALSE(err);

    auto gotShallow = ds.Get<json>(key1);
    ASSERT_TRUE(gotShallow);

    string gotDeep = gotShallow->at(key2).get<string>();
    ASSERT_EQ(gotDeep, want);
}

TEST_F(TestDatastore, SetAndClear)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    map<string, string> want = {{"a", "a"}, {"b", "b"}};
    err = ds.Set({{"k", want}});
    ASSERT_FALSE(err);

    auto got = ds.Get<map<string, string>>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(got->size(), want.size());

    want.clear();
    err = ds.Set({{"k", want}});
    ASSERT_FALSE(err);

    // This used to fail when Datastore was using json.merge_patch instead of json.update.
    got = ds.Get<map<string, string>>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(got->size(), 0);
}

TEST_F(TestDatastore, SetTypes)
{
    // Test some types other than just string

    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    // Start with string
    string wantString = "v";
    const char *wantStringKey = "wantStringKey";
    err = ds.Set({{wantStringKey, wantString}});
    ASSERT_FALSE(err);

    auto gotString = ds.Get<string>(wantStringKey);
    ASSERT_TRUE(gotString);
    ASSERT_EQ(*gotString, wantString);

    // bool
    bool wantBool = true;
    const char *wantBoolKey = "wantBoolKey";
    err = ds.Set({{wantBoolKey, wantBool}});
    ASSERT_FALSE(err);

    auto gotBool = ds.Get<bool>(wantBoolKey);
    ASSERT_TRUE(gotBool);
    ASSERT_EQ(*gotBool, wantBool);

    // int
    int wantInt = 5273482;
    const char *wantIntKey = "wantIntKey";
    err = ds.Set({{wantIntKey, wantInt}});
    ASSERT_FALSE(err);

    auto gotInt = ds.Get<int>(wantIntKey);
    ASSERT_TRUE(gotInt);
    ASSERT_EQ(*gotInt, wantInt);
}

TEST_F(TestDatastore, TypeMismatch)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    // string
    string wantString = "v";
    const char *wantStringKey = "wantStringKey";
    err = ds.Set({{wantStringKey, wantString}});
    ASSERT_FALSE(err);

    auto gotString = ds.Get<string>(wantStringKey);
    ASSERT_TRUE(gotString);
    ASSERT_EQ(*gotString, wantString);

    // bool
    bool wantBool = true;
    const char *wantBoolKey = "wantBoolKey";
    err = ds.Set({{wantBoolKey, wantBool}});
    ASSERT_FALSE(err);

    auto gotBool = ds.Get<bool>(wantBoolKey);
    ASSERT_TRUE(gotBool);
    ASSERT_EQ(*gotBool, wantBool);

    // int
    int wantInt = 5273482;
    const char *wantIntKey = "wantIntKey";
    err = ds.Set({{wantIntKey, wantInt}});
    ASSERT_FALSE(err);

    auto gotInt = ds.Get<int>(wantIntKey);
    ASSERT_TRUE(gotInt);
    ASSERT_EQ(*gotInt, wantInt);

    // It's an error to set one type and then try to get another
    auto got_fail_1 = ds.Get<bool>(wantStringKey);
    ASSERT_FALSE(got_fail_1);
    ASSERT_EQ(got_fail_1.error(), psicash::Datastore::kTypeMismatch);

    auto got_fail_2 = ds.Get<string>(wantIntKey);
    ASSERT_FALSE(got_fail_2);
    ASSERT_EQ(got_fail_2.error(), psicash::Datastore::kTypeMismatch);

    auto got_fail_3 = ds.Get<int>(wantStringKey);
    ASSERT_FALSE(got_fail_3);
    ASSERT_EQ(got_fail_3.error(), psicash::Datastore::kTypeMismatch);

    auto got_fail_4 = ds.Get<int>(wantBoolKey);
    //ASSERT_FALSE(got_fail_4); // NOTE: This doesn't actually fail. There must be a successful implicit conversion.

    // It's not an error to set one type to a key and then replace it with another type
    err = ds.Set({{wantStringKey, wantBool}});
    ASSERT_FALSE(err);
}

TEST_F(TestDatastore, getSimple)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    string want = "v";
    err = ds.Set({{"k", want}});
    ASSERT_FALSE(err);

    auto got = ds.Get<string>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want);
}

TEST_F(TestDatastore, getNotFound)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    string want = "v";
    err = ds.Set({{"k", want}});
    ASSERT_FALSE(err);

    auto got = ds.Get<string>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want);

    // Bad key
    auto nope = ds.Get<string>("nope");
    ASSERT_FALSE(nope);
    ASSERT_EQ(nope.error(), psicash::Datastore::kNotFound);
}

TEST_F(TestDatastore, clear)
{
    Datastore ds;
    auto err = ds.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    string want = "v";
    err = ds.Set({{"k", want}});
    ASSERT_FALSE(err);

    auto got = ds.Get<string>("k");
    ASSERT_TRUE(got);
    ASSERT_EQ(*got, want);

    ds.Clear();

    // There should be nothing in the datastore.
    got = ds.Get<string>("k");
    ASSERT_FALSE(got);
    ASSERT_EQ(got.error(), psicash::Datastore::kNotFound);
}
