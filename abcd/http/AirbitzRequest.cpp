/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "AirbitzRequest.hpp"
#include "Pinning.hpp"
#include "../config.h"
#include "../util/Util.hpp"
#include <openssl/ssl.h>

namespace abcd {

static CURLcode
curlSslCallback(CURL *curl, void *ssl_ctx, void *userptr)
{
    SSL_CTX_set_verify((SSL_CTX *)ssl_ctx,
        SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, ABC_PinCertCallback);
    return CURLE_OK;
}

AirbitzRequest::AirbitzRequest()
{
    if (!status_)
        return;

    if (curl_easy_setopt(handle_, CURLOPT_SSL_CTX_FUNCTION, curlSslCallback))
        status_ = ABC_ERROR(ABC_CC_Error, "cURL failed to set SSL pinning");
    header("Content-Type", "application/json");
    header("Authorization", API_KEY_HEADER + 15);
}

/**
 * Makes a URL post request and returns results in a string.
 * @param szURL         The request URL.
 * @param szPostData    The data to be posted in the request
 * @param pszResults    The location to store the allocated string with results.
 *                      The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLPostString(const char *szURL,
                          const char *szPostData,
                          char **pszResults,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;

    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(szPostData);
    ABC_CHECK_NULL(pszResults);

    ABC_CHECK_NEW(AirbitzRequest().post(reply, szURL, szPostData), pError);

    // assign the results
    ABC_STRDUP(*pszResults, reply.body.c_str());

exit:
    return cc;
}

} // namespace abcd
