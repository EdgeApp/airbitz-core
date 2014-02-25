/**
 * @file
 * AirBitz Account function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Account_h
#define ABC_Account_h

#include "ABC.h"
#include "ABC_Util.h"

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
    typedef struct sABC_AccountRequestInfo
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

        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_AccountRequestInfo;

    typedef enum eABC_AccountKey
    {
        ABC_AccountKey_L1,
        ABC_AccountKey_L2,
        ABC_AccountKey_LP2,
        ABC_AccountKey_PIN,
        ABC_AccountKey_RQ
    } tABC_AccountKey;

    tABC_CC ABC_AccountRequestInfoAlloc(tABC_AccountRequestInfo **ppAccountRequestInfo,
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

    void ABC_AccountRequestInfoFree(tABC_AccountRequestInfo *pAccountRequestInfo);

    void *ABC_AccountRequestThreaded(void *pData);

    void *ABC_AccountSetRecoveryThreaded(void *pData);

    tABC_CC ABC_AccountClearKeyCache(tABC_Error *pError);

    tABC_CC ABC_AccountGetKey(const char *szUserName,
                              const char *szPassword,
                              tABC_AccountKey keyType,
                              tABC_U08Buf *pKey,
                              tABC_Error *pError);

    tABC_CC ABC_AccountGetDirName(const char *szUserName,
                                  char **pszDirName,
                                  tABC_Error *pError);

    tABC_CC ABC_AccountGetSyncDirName(const char *szUserName,
                                      char **pszDirName,
                                      tABC_Error *pError);

    tABC_CC ABC_AccountSetPIN(const char *szUserName,
                              const char *szPassword,
                              const char *szPIN,
                              tABC_Error *pError);

    tABC_CC ABC_AccountGetCategories(const char *szUserName,
                              char ***paszCategories,
                              unsigned int *pCount,
                              tABC_Error *pError);

    tABC_CC ABC_AccountAddCategory(const char *szUserName,
                            char *szCategory,
                            tABC_Error *pError);

    tABC_CC ABC_AccountRemoveCategory(const char *szUserName,
                               char *szCategory,
                               tABC_Error *pError);

    tABC_CC ABC_AccountCheckRecoveryAnswers(const char *szUserName,
                                            const char *szRecoveryAnswers,
                                            bool *pbValid,
                                            tABC_Error *pError);

    tABC_CC ABC_AccountCheckCredentials(const char *szUserName,
                                        const char *szPassword,
                                        tABC_Error *pError);

    tABC_CC ABC_AccountCheckValidUser(const char *szUserName,
                                      tABC_Error *pError);


    void ABC_AccountFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices);

    tABC_CC ABC_AccountGetRecoveryQuestions(const char *szUserName,
                                            char **pszQuestions,
                                            tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
