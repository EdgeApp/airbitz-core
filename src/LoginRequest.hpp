/**
 * @file
 * Pthread-compatible wrappers around long-running login functions.
 */

#ifndef ABC_LoginRequest_h
#define ABC_LoginRequest_h

#include "ABC.h"

namespace abcd {

    /**
     * AirBitz Core Account Request Structure
     *
     * This structure contains the detailed information associated
     * with threaded requests on accounts
     *
     */
    typedef struct sABC_LoginRequestInfo
    {
        /** request type */
        tABC_RequestType requestType;

        /** account username */
        char        *szUserName;

        /** account password */
        char        *szPassword;

        /** recovery questions (not used in all requests) */
        char        *szRecoveryQuestions;

        /** recovery answers (not used in all requests) */
        char        *szRecoveryAnswers;

        /** account PIN for create account requests */
        char        *szPIN;

        /** new password for password change request */
        char        *szNewPassword;

        /** data pointer given by caller at initial create call time */
        void        *pData;

        /** callback function when request is complete */
        tABC_Request_Callback fRequestCallback;
    } tABC_LoginRequestInfo;

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
                                        tABC_Error *pError);

    void ABC_LoginRequestInfoFree(tABC_LoginRequestInfo *pAccountRequestInfo);

    void *ABC_LoginRequestThreaded(void *pData);

} // namespace abcd

#endif
