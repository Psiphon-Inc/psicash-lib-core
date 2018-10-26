#include <string>
#include "jnitest.h"
#include "http_status_codes.h"

using namespace std;

error::Error PsiCashTest::TestReward(const string& transaction_class, const string& distinguisher) {
    auto result = MakeHTTPRequestWithRetry(
            "POST", "/transaction", true,
            {{"class", transaction_class},
             {"distinguisher", distinguisher}});
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    } else if (result->status != kHTTPStatusOK) {
        return MakeError(utils::Stringer("reward request failed: ", result->status, "; ",
                                         result->error, "; ", result->body));
    }

    return error::nullerr;
}
