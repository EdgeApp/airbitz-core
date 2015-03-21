/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Broadcast.hpp"
#include "Testnet.hpp"
#include "../config.h"
#include "../crypto/Encoding.hpp"
#include "../json/JsonObject.hpp"
#include "../util/URL.hpp"
#include <curl/curl.h>

namespace abcd {

static size_t
curlWriteData(void *data, size_t memberSize, size_t numMembers, void *userData)
{
    auto size = numMembers * memberSize;

    auto string = static_cast<std::string *>(userData);
    string->append(static_cast<char *>(data), size);

    return size;
}

struct ChainPost: public JsonObject
{
    ABC_JSON_STRING(hex, "hex", nullptr);
};

static Status
chainPostTx(DataSlice tx)
{
    const char *url = isTestnet() ?
        "https://api.chain.com/v1/testnet3/transactions":
        "https://api.chain.com/v1/bitcoin/transactions";

    ChainPost object;
    object.hexSet(base16Encode(tx).c_str());
    std::string body;
    ABC_CHECK(object.encode(body));

    ABC_DebugLog("URL: %s\n", url);
    ABC_DebugLog("UserPwd: %s\n", CHAIN_API_USERPWD);
    ABC_DebugLog("Body: %s\n", body.c_str());

    std::string resBuffer;
    AutoFree<CURL, curl_easy_cleanup> curlHandle;
    ABC_CHECK_OLD(ABC_URLCurlHandleInit(&curlHandle.get(), &error));
    if (curl_easy_setopt(curlHandle, CURLOPT_URL, url))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set URL\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_USERPWD, CHAIN_API_USERPWD))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set User:Password\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "PUT"))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set Put\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, body.c_str()))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set post fields\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &resBuffer))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set data\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, curlWriteData))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set callback\n");
    if (curl_easy_perform(curlHandle))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to perform\n");

    long resCode;
    if (curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to retrieve response info\n");

    ABC_DebugLog("Chain Response Code: %d\n", resCode);
    ABC_DebugLog("%.100s\n", resBuffer.c_str());
    if (resCode < 200 || 299 < resCode)
        return ABC_ERROR(ABC_CC_Error, "Error when sending tx to chain");

    return Status();
}

static Status
blockhainPostTx(DataSlice tx)
{
    const char *url = "https://blockchain.info/pushtx";

    std::string body = "tx=" + base16Encode(tx);

    ABC_DebugLog("%s\n", body.c_str());

    std::string resBuffer;
    AutoFree<CURL, curl_easy_cleanup> curlHandle;
    ABC_CHECK_OLD(ABC_URLCurlHandleInit(&curlHandle.get(), &error));
    if (curl_easy_setopt(curlHandle, CURLOPT_URL, url))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set URL\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, body.c_str()))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set post fields\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &resBuffer))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set data\n");
    if (curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, curlWriteData))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set callback\n");
    if (curl_easy_perform(curlHandle))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to perform\n");

    long resCode;
    if (curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to retrieve response info\n");

    ABC_DebugLog("Blockchain Response Code: %d\n", resCode);
    ABC_DebugLog("%.100s\n", resBuffer.c_str());
    if (resCode < 200 || 299 < resCode)
        return ABC_ERROR(ABC_CC_Error, "Error when sending tx to blockchain");

    return Status();
}

Status
broadcastTx(DataSlice rawTx)
{
    Status out = chainPostTx(rawTx);

    // Only try Blockchain when not on testnet:
    if (!isTestnet())
    {
        if (out)
            blockhainPostTx(rawTx);
        else
            out = blockhainPostTx(rawTx);
    }

    return out;
}

} // namespace abcd
