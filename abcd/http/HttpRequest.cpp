/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "HttpRequest.hpp"

namespace abcd {

#define TIMEOUT 10

extern std::string gCertPath;

static size_t
curlDataCallback(void *data, size_t memberSize, size_t numMembers, void *userData)
{
    auto size = numMembers * memberSize;

    auto string = static_cast<std::string *>(userData);
    string->append(static_cast<char *>(data), size);

    return size;
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
    if (curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &result.body))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to set callback data");
    if (curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, curlDataCallback))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to set callback");
    if (curl_easy_setopt(handle_, CURLOPT_URL, url.c_str()))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to set URL");
    if (headers_)
    {
        if (curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers_))
            return ABC_ERROR(ABC_CC_Error, "cURL failed to set headers");
    }

    // Make the request:
    if (curl_easy_perform(handle_))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to make HTTP request");

    // Check the result:
    if (curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &result.code))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to get response code");
    if (result.code < 200 || 300 <= result.code)
        return ABC_ERROR(ABC_CC_Error, "Bad HTTP response code " +
            std::to_string(result.code));

    return Status();
}

Status
HttpRequest::post(HttpReply &result, const std::string &url,
    const std::string body)
{
    if (!status_)
        return status_;
    if (curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, body.size()) ||
        curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, body.c_str()))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to set POST body");
    return get(result, url);
}

Status
HttpRequest::put(HttpReply &result, const std::string &url,
    const std::string body)
{
    if (!status_)
        return status_;
    if (curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PUT"))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to set PUT mode");
    return post(result, url);
}

Status
HttpRequest::init()
{
    handle_ = curl_easy_init();
    if (!handle_)
        return ABC_ERROR(ABC_CC_Error, "cURL failed create handle");

    // Basic options:
    if (curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to ignore signals");
    if (curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT, TIMEOUT))
        return ABC_ERROR(ABC_CC_Error, "cURL failed to set timeout");
    if (!gCertPath.empty())
    {
        if (curl_easy_setopt(handle_, CURLOPT_CAINFO, gCertPath.c_str()))
            return ABC_ERROR(ABC_CC_Error, "cURL failed to set ca-certificates.crt");
    }

    return Status();
}

} // namespace abcd
