/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Broadcast.hpp"
#include "Testnet.hpp"
#include "../config.h"
#include "../util/Crypto.hpp"
#include "../util/Json.hpp"
#include "../util/URL.hpp"
#include "../util/Util.hpp"
#include <curl/curl.h>

namespace abcd {

static size_t
ABC_BridgeCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData)
{
    std::string *pCurlBuffer = (std::string *) pUserData;
    unsigned int dataAvailLength = (unsigned int) numMembers * (unsigned int) memberSize;
    size_t amountWritten = 0;

    if (pCurlBuffer)
    {
        pCurlBuffer->append((char *) pBuffer);
        amountWritten = dataAvailLength;
    }

    return amountWritten;
}

static tABC_CC
ABC_BridgeChainPostTx(DataSlice tx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;
    std::string url, resBuffer;
    json_t *pJSON_Root = NULL;
    char *szPut = NULL;

    AutoString encoded;
    ABC_CHECK_RET(ABC_CryptoHexEncode(toU08Buf(tx), &encoded.get(), pError));

    if (isTestnet())
    {
        url.append("https://api.chain.com/v1/testnet3/transactions");
    }
    else
    {
        url.append("https://api.chain.com/v1/bitcoin/transactions");
    }

    pJSON_Root = json_pack("{ss}", "hex", encoded.get());
    szPut = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    ABC_DebugLog("URL: %s\n", url.c_str());
    ABC_DebugLog("UserPwd: %s\n", CHAIN_API_USERPWD);
    ABC_DebugLog("Body: %s\n", szPut);

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, url.c_str())) == 0,
        ABC_CC_Error, "Curl failed to set URL\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_USERPWD, CHAIN_API_USERPWD)) == 0,
        ABC_CC_Error, "Curl failed to set User:Password\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT")) == 0,
        ABC_CC_Error, "Curl failed to set Put\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, szPut)) == 0,
        ABC_CC_Error, "Curl failed to set post fields\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &resBuffer)) == 0,
        ABC_CC_Error, "Curl failed to set data\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, ABC_BridgeCurlWriteData)) == 0,
        ABC_CC_Error, "Curl failed to set callback\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_perform(pCurlHandle)) == 0,
        ABC_CC_Error, "Curl failed to perform\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_getinfo(pCurlHandle, CURLINFO_RESPONSE_CODE, &resCode)) == 0,
        ABC_CC_Error, "Curl failed to retrieve response info\n");

    ABC_DebugLog("Chain Response Code: %d\n", resCode);
    ABC_DebugLog("%.100s\n", resBuffer.c_str());
    ABC_CHECK_ASSERT(resCode == 201 || resCode == 200,
            ABC_CC_Error, "Error when sending tx to chain");
exit:
    if (pCurlHandle != NULL)
    {
        curl_easy_cleanup(pCurlHandle);
    }
    json_decref(pJSON_Root);
    ABC_FREE_STR(szPut);

    return cc;
}

static tABC_CC
ABC_BridgeBlockhainPostTx(DataSlice tx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;
    std::string url, body, resBuffer;

    AutoString encoded;
    ABC_CHECK_RET(ABC_CryptoHexEncode(toU08Buf(tx), &encoded.get(), pError));

    url.append("https://blockchain.info/pushtx");
    body.append("tx=");
    body.append(encoded.get());

    ABC_DebugLog("%s\n", body.c_str());

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, url.c_str())) == 0,
        ABC_CC_Error, "Curl failed to set URL\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, body.c_str())) == 0,
        ABC_CC_Error, "Curl failed to set post fields\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &resBuffer)) == 0,
        ABC_CC_Error, "Curl failed to set data\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, ABC_BridgeCurlWriteData)) == 0,
        ABC_CC_Error, "Curl failed to set callback\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_perform(pCurlHandle)) == 0,
        ABC_CC_Error, "Curl failed to perform\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_getinfo(pCurlHandle, CURLINFO_RESPONSE_CODE, &resCode)) == 0,
        ABC_CC_Error, "Curl failed to retrieve response info\n");

    ABC_DebugLog("Blockchain Response Code: %d\n", resCode);
    ABC_DebugLog("%.100s\n", resBuffer.c_str());
    ABC_CHECK_ASSERT(resCode == 200, ABC_CC_Error, "Error when sending tx to blockchain");
exit:
    if (pCurlHandle != NULL)
        curl_easy_cleanup(pCurlHandle);

    return cc;
}


Status
broadcastTx(DataSlice rawTx)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;

    cc = ABC_BridgeChainPostTx(rawTx, &error);

    // Only try Blockchain when not on testnet:
    if (!isTestnet())
    {
        if (cc == ABC_CC_Ok)
            ABC_BridgeBlockhainPostTx(rawTx, &error);
        else
            cc = ABC_BridgeBlockhainPostTx(rawTx, &error);
    }

    if (ABC_CC_Ok != cc)
        return Status::fromError(error);

    return Status();
}

} // namespace abcd
