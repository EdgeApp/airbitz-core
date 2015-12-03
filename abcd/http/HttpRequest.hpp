/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_HTTP_HTTP_REQUEST_HPP
#define ABCD_HTTP_HTTP_REQUEST_HPP

#include "../util/Status.hpp"
#include <curl/curl.h>

namespace abcd {

struct HttpReply
{
    /** The HTTP status code. */
    int code;
    /** The returned message body. */
    std::string body;

    /**
     * Verifies that the response code is in the 200 range.
     */
    Status
    codeOk();
};

/**
 * A class for building up and making HTTP requests.
 */
class HttpRequest
{
public:
    ~HttpRequest();
    HttpRequest();

    /**
     * Enables verbose debugging on the HTTP request.
     */
    HttpRequest &
    debug();

    /**
     * Adds a header to the HTTP request.
     */
    HttpRequest &
    header(const std::string &key, const std::string &value);

    /**
     * Performs an HTTP GET operation.
     */
    Status
    get(HttpReply &result, const std::string &url);

    /**
     * Performs an HTTP POST operation.
     */
    Status
    post(HttpReply &result, const std::string &url,
         const std::string body="");

    /**
     * Performs an HTTP PUT operation.
     */
    Status
    put(HttpReply &result, const std::string &url,
        const std::string body="");

protected:
    Status status_;
    CURL *handle_;

private:
    struct curl_slist *headers_;

    Status init();
};

} // namespace abcd

#endif
