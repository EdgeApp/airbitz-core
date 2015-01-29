/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginRequest.hpp"
#include "LoginShim.hpp"
#include "../abcd/util/Util.hpp"

namespace abcd {

/**
 * Allocates and fills in an account request structure with the info given.
 *
 * @param ppAccountRequestInfo      Pointer to store allocated request info
 * @param requestType               Type of request this is being used for
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account (can be NULL for some requests)
 * @param szRecoveryQuestions       Recovery questions seperated by newlines (can be NULL for some requests)
 * @param szRecoveryAnswers         Recovery answers seperated by newlines (can be NULL for some requests)
 * @param szPIN                     PIN number for the account (can be NULL for some requests)
 * @param szNewPassword             New password for the account (for change password requests)
 * @param fRequestCallback          The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginRequestInfoAlloc(tABC_LoginRequestInfo **ppAccountRequestInfo,
                                    tABC_RequestType requestType,
                                    const char *szUserName,
                                    const char *szPassword,
                                    const char *szRecoveryQuestions,
                                    const char *szRecoveryAnswers,
                                    const char *szPIN,
                                    const char *szNewPassword,
                                    tABC_Request_Callback fRequestCallback,
                                    void *pData,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_LoginRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_NULL(ppAccountRequestInfo);
    ABC_CHECK_NULL(szUserName);

    ABC_NEW(pAccountRequestInfo, tABC_LoginRequestInfo);

    pAccountRequestInfo->requestType = requestType;

    ABC_STRDUP(pAccountRequestInfo->szUserName, szUserName);

    if (NULL != szPassword)
    {
        ABC_STRDUP(pAccountRequestInfo->szPassword, szPassword);
    }

    if (NULL != szRecoveryQuestions)
    {
        ABC_STRDUP(pAccountRequestInfo->szRecoveryQuestions, szRecoveryQuestions);
    }

    if (NULL != szRecoveryAnswers)
    {
        ABC_STRDUP(pAccountRequestInfo->szRecoveryAnswers, szRecoveryAnswers);
    }

    if (NULL != szPIN)
    {
        ABC_STRDUP(pAccountRequestInfo->szPIN, szPIN);
    }

    if (NULL != szNewPassword)
    {
        ABC_STRDUP(pAccountRequestInfo->szNewPassword, szNewPassword);
    }

    pAccountRequestInfo->pData = pData;

    pAccountRequestInfo->fRequestCallback = fRequestCallback;

    *ppAccountRequestInfo = pAccountRequestInfo;

exit:

    return cc;
}

/**
 * Frees the account creation info structure
 */
void ABC_LoginRequestInfoFree(tABC_LoginRequestInfo *pAccountRequestInfo)
{
    if (pAccountRequestInfo)
    {
        ABC_FREE_STR(pAccountRequestInfo->szUserName);

        ABC_FREE_STR(pAccountRequestInfo->szPassword);

        ABC_FREE_STR(pAccountRequestInfo->szRecoveryQuestions);

        ABC_FREE_STR(pAccountRequestInfo->szRecoveryAnswers);

        ABC_FREE_STR(pAccountRequestInfo->szPIN);

        ABC_FREE_STR(pAccountRequestInfo->szNewPassword);

        ABC_CLEAR_FREE(pAccountRequestInfo, sizeof(tABC_LoginRequestInfo));
    }
}

/**
 * Performs the request specified. Assumes it is running in a thread.
 *
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create an account (should be a tABC_LoginCreateInfo)
 */
void *ABC_LoginRequestThreaded(void *pData)
{
    tABC_LoginRequestInfo *pInfo = (tABC_LoginRequestInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;

        results.requestType = pInfo->requestType;

        results.bSuccess = false;

        tABC_CC CC = ABC_CC_Error;

        // perform the appropriate request
        if (ABC_RequestType_CreateAccount == pInfo->requestType)
        {
            // create the account
            CC = ABC_LoginShimNewAccount(pInfo->szUserName, pInfo->szPassword, &(results.errorInfo));

            // hack to set pin:
            tABC_Error error;
            ABC_SetPIN(pInfo->szUserName, pInfo->szPassword, pInfo->szPIN, &error);
        }
        else if (ABC_RequestType_AccountSignIn == pInfo->requestType)
        {
            // sign-in
            CC = ABC_LoginShimLogin(pInfo->szUserName, pInfo->szPassword, &(results.errorInfo));
        }
        else if (ABC_RequestType_SetAccountRecoveryQuestions == pInfo->requestType)
        {
            // set the recovery information
            CC = ABC_LoginShimSetRecovery(pInfo->szUserName, pInfo->szPassword,
                pInfo->szRecoveryQuestions, pInfo->szRecoveryAnswers,
                &(results.errorInfo));
        }
        else if (ABC_RequestType_ChangePassword == pInfo->requestType)
        {
            // change the password
            CC = ABC_LoginShimSetPassword(pInfo->szUserName, pInfo->szPassword,
                pInfo->szRecoveryAnswers, pInfo->szNewPassword, &(results.errorInfo));
        }


        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_LoginRequestInfoFree(pInfo);
    }

    return NULL;
}

} // namespace abcd
