/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_HTTP_HTTP_AIRBITZ_HPP
#define ABCD_HTTP_HTTP_AIRBITZ_HPP

#include "../http/HttpRequest.hpp"

namespace abcd {

/**
 * An HttpRequest with special features for talking to the Airbitz servers.
 * Enables certificate pinning, auth token, and "Content-Type" header.
 */
class AirbitzRequest:
    public HttpRequest
{
public:
    AirbitzRequest();
};

} // namespace abcd

#endif
