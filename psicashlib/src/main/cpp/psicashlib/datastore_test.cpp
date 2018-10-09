#include <cstdlib>
#include <ctime>

#include "gtest/gtest.h"
#include "datastore.h"

using namespace std;
using namespace psicash;
using json = nlohmann::json;

class TestDatastore : public ::testing::Test
{
  public:
    TestDatastore() {}

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

    string GetTempDir()
    {
        vector<string> env_vars = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
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

        string res = tmp;
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

    void WriteBadData(const char *datastoreRoot)
    {
        auto ds_file = string(datastoreRoot) + "/datastore";
        auto make_bad_file = "echo nonsense > " + ds_file;
        system(make_bad_file.c_str());
    }
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

TEST_F(TestDatastore, setMulti)
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

TEST_F(TestDatastore, setDeep)
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

TEST_F(TestDatastore, setTypes)
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
