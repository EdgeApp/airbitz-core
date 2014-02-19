/**
 * @file
 * AirBitz URL functions.
 *
 * This file contains all of the functions associated with sending and receiving
 * data to and from servers.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "ABC.h"
#include "ABC_FileIO.h"
#include "ABC_Util.h"
#include "ABC_Debug.h"
#include "ABC_URL.h"

static bool gbInitialized = false;

static int test(); // temp
static size_t curl_write_data(void *buffer, size_t size, size_t nmemb, void *userp); // temp

// temp
typedef struct sCurlBuffer
{
    unsigned char *pBuf;
    unsigned int curSize;
    unsigned int curUsed;
} tCurlBuffer;



// initialize the URL system
tABC_CC ABC_URLInitialize(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "The URL has already been initalized");

    // initialize curl
    CURLcode curlCode;
    if ((curlCode = curl_global_init(CURL_GLOBAL_ALL)) != 0)
    {
        ABC_DebugLog("Curl init failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl init failed");
    }

    gbInitialized = true;

exit:

    return cc;
}

// shut down the URL system
void ABC_URLTerminate()
{
    if (gbInitialized == true)
    {
        // cleanup curl
        curl_global_cleanup();

        gbInitialized = false;
    }
}

// makes a URL request with the given arguments
// Data is returned in pData, caller is responsible for free'ing
tABC_CC ABC_URLRequest(const char *szURL, const char *szPostData, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The URL system has not been initalized");

    // TODO: write the real request here
    
    test(); // temp

exit:
    
    return cc;
}

// temp
static int test()
{
    //char *szWebPage = "http://www.guimp.com/";
    //char *szWebPage = "http://www.joypiter.com/";
    char *szWebPage = "http://httpbin.org/post";
    //char *szWebPage = "https://www.google.com";
    tCurlBuffer curlBuffer;
    memset(&curlBuffer, 0, sizeof(tCurlBuffer));
    CURLcode curlCode;

    CURL *pCurlHandle = curl_easy_init();
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szWebPage)) != 0)
    {
        fprintf(stderr, "curl easy setopt failed: %d\n", curlCode);
        return -1;
    }

    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, curl_write_data)) != 0)
    {
        fprintf(stderr, "curl easy setopt failed: %d\n", curlCode);
        return -1;
    }


    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &curlBuffer)) != 0)
    {
        fprintf(stderr, "curl easy setopt failed: %d\n", curlCode);
        return -1;
    }

#if 1
    //char szDataPost[1024];
    //sprintf(szDataPost, "data1={ \"name\" : \"test_name\" }&password=test_password");
    char *szDataPost="data1={\n \"name\" : \"test_name\"\n }&password=test_password";
    printf("posting: %s\n", szDataPost);
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, szDataPost)) != 0)
    {
        fprintf(stderr, "curl easy setopt failed: %d\n", curlCode);
        return -1;
    }
#endif

    if ((curlCode = curl_easy_perform(pCurlHandle)) != 0)
    {
        fprintf(stderr, "curl easy perform failed: %d\n", curlCode);
        return -1;
    }

    curl_easy_cleanup(pCurlHandle);

    if ((curlBuffer.curUsed) && (curlBuffer.pBuf))
    {
        //hexDump("Curl data final", curlBuffer.pBuf, curlBuffer.curUsed);
        //printf("CurSize: %d, CurUsed: %d\n", curlBuffer.curSize, curlBuffer.curUsed);
        // add a null
        curlBuffer.pBuf = realloc(curlBuffer.pBuf, curlBuffer.curUsed + 1);
        curlBuffer.curSize++;
        unsigned char *pLastByte = (curlBuffer.pBuf + curlBuffer.curSize) - 1;
        *pLastByte = 0;
        curlBuffer.curUsed++;
        //hexDump("Curl data final", curlBuffer.pBuf, curlBuffer.curUsed);

        // print what we go
        printf("Curl results:\n%s\n", curlBuffer.pBuf);
    }

    return 0;
}

// temp
static size_t curl_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    tCurlBuffer *pCurlBuffer = (tCurlBuffer *)userp;
    unsigned int dataAvailLength = (unsigned int) nmemb * (unsigned int) size;
    size_t amountWritten = 0;

    //hexDump("Curl data", buffer, size * nmemb);

    if (pCurlBuffer)
    {
        unsigned int spaceNeeded = dataAvailLength - (pCurlBuffer->curSize - pCurlBuffer->curUsed);
        //printf("spaceNeeded: %u\n", spaceNeeded);
        if (pCurlBuffer->pBuf)
        {
            pCurlBuffer->pBuf = realloc(pCurlBuffer->pBuf, pCurlBuffer->curSize + spaceNeeded);
            pCurlBuffer->curSize += spaceNeeded;
        }
        else
        {
            pCurlBuffer->curSize = spaceNeeded;
            pCurlBuffer->pBuf = malloc(pCurlBuffer->curSize);
            pCurlBuffer->curUsed = 0;
        }

        memcpy(pCurlBuffer->pBuf + pCurlBuffer->curUsed, buffer, dataAvailLength);
        pCurlBuffer->curUsed += dataAvailLength;
        amountWritten = dataAvailLength;
    }

    return amountWritten;
}