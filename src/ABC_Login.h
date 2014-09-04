/**
 * @file
 * AirBitz Account function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Login_h
#define ABC_Login_h

#include "ABC.h"
#include "ABC_Sync.h"

#ifdef __cplusplus
extern "C" {
#endif

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

    typedef enum eABC_LoginKey
    {
        ABC_LoginKey_L1,
        ABC_LoginKey_L4,
        ABC_LoginKey_LP1,
        ABC_LoginKey_LP2,
        ABC_LoginKey_MK,
        ABC_LoginKey_RepoAccountKey,
        ABC_LoginKey_RQ
    } tABC_LoginKey;

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

    void *ABC_LoginSetRecoveryThreaded(void *pData);

    tABC_CC ABC_LoginClearKeyCache(tABC_Error *pError);

    tABC_CC ABC_LoginGetKey(const char *szUserName,
                              const char *szPassword,
                              tABC_LoginKey keyType,
                              tABC_U08Buf *pKey,
                              tABC_Error *pError);

    tABC_CC ABC_LoginCheckRecoveryAnswers(const char *szUserName,
                                            const char *szRecoveryAnswers,
                                            bool *pbValid,
                                            tABC_Error *pError);

    tABC_CC ABC_LoginCheckCredentials(const char *szUserName,
                                        const char *szPassword,
                                        tABC_Error *pError);

    tABC_CC ABC_LoginCheckValidUser(const char *szUserName,
                                      tABC_Error *pError);


    tABC_CC ABC_LoginGetRecoveryQuestions(const char *szUserName,
                                            char **pszQuestions,
                                            tABC_Error *pError);

    tABC_CC ABC_LoginGetSyncKeys(const char *szUserName,
                                 const char *szPassword,
                                 tABC_SyncKeys **ppKeys,
                                 tABC_Error *pError);

	tABC_CC ABC_LoginUpdateLoginPackageFromServer(const char *szUserName, const char *szPassword, tABC_Error *pError);

    tABC_CC ABC_LoginSyncData(const char *szUserName,
                                const char *szPassword,
                                int *pDirty,
                                tABC_Error *pError);

	tABC_CC ABC_LoginUploadLogs(const char *szUserName,
								const char *szPassword,
								tABC_Error *pError);

    // Blocking functions:
    tABC_CC ABC_LoginSignIn(tABC_LoginRequestInfo *pInfo,
                              tABC_Error *pError);

    tABC_CC ABC_LoginCreate(tABC_LoginRequestInfo *pInfo,
                              tABC_Error *pError);

    tABC_CC ABC_LoginSetRecovery(tABC_LoginRequestInfo *pInfo,
                                   tABC_Error *pError);

    tABC_CC ABC_LoginChangePassword(tABC_LoginRequestInfo *pInfo,
                                      tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
