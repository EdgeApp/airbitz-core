/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Http.hpp"
#include <curl/curl.h>

namespace abcd {

/**
 * Manages the cURL library global memory lifetime.
 */
struct HttpSingleton
{
    ~HttpSingleton();
    HttpSingleton();

    Status status;
};

// Global variables:
static HttpSingleton gSingleton;
std::string gCertPath;

HttpSingleton::~HttpSingleton()
{
    curl_global_cleanup();
}

HttpSingleton::HttpSingleton()
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT))
        status = ABC_ERROR(ABC_CC_Error, "Cannot initialize cURL");
}

Status
httpInit(const std::string &certPath)
{
    gCertPath = certPath;
    return gSingleton.status;
}

} // namespace abcd
