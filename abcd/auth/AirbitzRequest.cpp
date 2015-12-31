/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "AirbitzRequest.hpp"
#include "Pinning.hpp"
#include "../Context.hpp"
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
    header("Authorization", "Token " + gContext->apiKeyHeader());
}

} // namespace abcd
