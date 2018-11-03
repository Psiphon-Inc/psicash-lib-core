#ifndef PSICASHLIB_TEST_HELPERS_H
#define PSICASHLIB_TEST_HELPERS_H

#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>

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

    void WriteBadData(const char *datastoreRoot)
    {
        auto ds_file = std::string(datastoreRoot) + "/datastore";
        auto make_bad_file = "echo nonsense > " + ds_file;
        system(make_bad_file.c_str());
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

#endif // PSICASHLIB_TEST_HELPERS_H
