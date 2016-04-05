/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "HttpRequest.hpp"
#include "../Context.hpp"
#include "../util/Debug.hpp"

namespace abcd {

#define TIMEOUT 10

static Status curlOk(CURLcode code)
{
    if (code)
    {
        std::string message("cURL error: ");
        if (curl_easy_strerror(code))
            message += curl_easy_strerror(code);
        else
            message += std::to_string(code);
        return ABC_ERROR(ABC_CC_SysError, message);
    }
    return Status();
}

#define ABC_CHECK_CURL(code) ABC_CHECK(curlOk(code))

static int
curlDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size,
                  void *userp)
{
    std::string payload(data, size);
    switch (type)
    {
    case CURLINFO_HEADER_OUT:
        ABC_DebugLog("cURL header out: %s", payload.c_str());
        return 0;
    case CURLINFO_DATA_OUT:
        ABC_DebugLog("cURL data out: %s", payload.c_str());
        return 0;
    case CURLINFO_HEADER_IN:
        ABC_DebugLog("cURL header in: %s", payload.c_str());
        return 0;
    case CURLINFO_DATA_IN:
        ABC_DebugLog("cURL data in: %s", payload.c_str());
        return 0;
    default:
        return 0;
    }
}

static size_t
curlDataCallback(void *data, size_t memberSize, size_t numMembers,
                 void *userData)
{
    auto size = numMembers * memberSize;

    auto string = static_cast<std::string *>(userData);
    string->append(static_cast<char *>(data), size);

    return size;
}

Status
HttpReply::codeOk()
{
    if (code < 200 || 300 <= code)
        return ABC_ERROR(ABC_CC_Error, "Bad HTTP status code " +
                         std::to_string(code));
    return Status();
}

HttpRequest::~HttpRequest()
{
    if (handle_) curl_easy_cleanup(handle_);
    if (headers_) curl_slist_free_all(headers_);
}

HttpRequest::HttpRequest():
    handle_(nullptr),
    headers_(nullptr)
{
    status_ = init();
}

HttpRequest &
HttpRequest::debug()
{
    if (status_)
        status_ = curlOk(curl_easy_setopt(handle_, CURLOPT_DEBUGFUNCTION,
                                          curlDebugCallback));
    if (status_)
        status_ = curlOk(curl_easy_setopt(handle_, CURLOPT_VERBOSE, 1L));
    return *this;
}

HttpRequest &
HttpRequest::header(const std::string &key, const std::string &value)
{
    if (!status_)
        return *this;

    std::string header = key + ": " + value;
    auto slist = curl_slist_append(headers_, header.c_str());
    if (!slist)
        status_ = ABC_ERROR(ABC_CC_Error, "cURL slist error");
    else
        headers_ = slist;

    return *this;
}

Status
HttpRequest::get(HttpReply &result, const std::string &url)
{
    if (!status_)
        return status_;

    // Final options:
    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &result.body));
    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION,
                                    curlDataCallback));
    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_URL, url.c_str()));
    if (headers_)
        ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers_));

    // Make the request:
    ABC_CHECK_CURL(curl_easy_perform(handle_));
    ABC_CHECK_CURL(curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE,
                                     &result.code));
    if (result.codeOk())
        ABC_DebugLog("%s (%d)", url.c_str(), result.code);
    else
        ABC_DebugLog("%s (%d)\n%s", url.c_str(), result.code,
                     result.body.c_str());

    return Status();
}

Status
HttpRequest::post(HttpReply &result, const std::string &url,
                  const std::string body)
{
    if (!status_)
        return status_;

    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, body.size()));
    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, body.c_str()));
    return get(result, url);
}

Status
HttpRequest::put(HttpReply &result, const std::string &url,
                 const std::string body)
{
    if (!status_)
        return status_;

    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PUT"));
    return post(result, url);
}

Status
HttpRequest::init()
{
    handle_ = curl_easy_init();
    if (!handle_)
        return ABC_ERROR(ABC_CC_Error, "cURL failed create handle");

    // Basic options:
    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1));
    ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT, TIMEOUT));

    const auto certPath = gContext->paths.certPath();
    if (!certPath.empty())
        ABC_CHECK_CURL(curl_easy_setopt(handle_, CURLOPT_CAINFO,
                                        certPath.c_str()));

    return Status();
}

} // namespace abcd
