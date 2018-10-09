#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>


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
        std::vector<std::string> env_vars = {"TMPDIR", "TMP", "TEMP", "TEMPDIR"};
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
