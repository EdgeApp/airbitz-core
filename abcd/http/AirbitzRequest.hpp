/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_HTTP_HTTP_AIRBITZ_HPP
#define ABCD_HTTP_HTTP_AIRBITZ_HPP

#include "HttpRequest.hpp"

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

// Legacy wrapper:
tABC_CC ABC_URLPostString(const char *szURL,
                          const char *szPostData,
                          char **pszResults,
                          tABC_Error *pError);

} // namespace abcd

#endif
