#include <string>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;

#define API_SERVER_SCHEME "https"
#define API_SERVER_HOSTNAME "dev-api.psi.cash"
#define API_SERVER_PORT 443
#define API_SERVER_VERSION "v1"
#define PSICASH_USER_AGENT "Psiphon-PsiCash-iOS" // TODO: update
#define PSICASH_AUTH_HEADER "X-PsiCash-Auth"

string buildRequestParams(string path, string method, map<string, string> queryParams, bool includeAuthTokens)
{
    // TODO: request metadata

    json headers;
    headers["User-Agent"] = PSICASH_USER_AGENT;
    if (includeAuthTokens)
    {
        headers[PSICASH_AUTH_HEADER] = "badtokens";
    }

    json j = {
        {"scheme", API_SERVER_SCHEME},
        {"hostname", API_SERVER_HOSTNAME},
        {"port", API_SERVER_PORT},
        {"method", method},
        {"path", string("/") + API_SERVER_VERSION + "/" + path},
        {"query", queryParams},
        {"headers", headers},
    };

    return j.dump(); // TODO: catch exception
}
