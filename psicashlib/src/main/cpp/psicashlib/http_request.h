#ifndef PSICASHLIB_HTTP_REQUEST_H
#define PSICASHLIB_HTTP_REQUEST_H

#include <string>
#include <map>

#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"

std::string buildRequestParams(std::string path, std::string method, std::map<std::string, std::string> queryParams, bool includeAuthTokens);

struct HTTPResult
{
    int status;
    std::string body;
    std::string error;

    HTTPResult() : status(-1) {}
};

#endif //PSICASHLIB_HTTP_REQUEST_H
