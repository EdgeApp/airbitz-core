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
     * AirBitz Core Create Account Structure
     *
     * This structure contains the detailed information associated
     * with creating a new account.
     *
     */
    typedef struct sABC_AccountCreateInfo
    {
        /** data pointer given by caller at initial create call time */
        void        *pData;
        
        const char *szUserName;
        const char *szPassword;
        const char *szPIN;
        tABC_Request_Callback fRequestCallback;
        
        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_AccountCreateInfo;
    
    typedef struct sABC_AccountSignInInfo
    {
        /** data pointer given by caller at initial create call time */
        void        *pData;
        
        const char *szUserName;
        const char *szPassword;
        tABC_Request_Callback fRequestCallback;
        
        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_AccountSignInInfo;

    typedef struct sABC_AccountSetRecoveryInfo
    {
        /** data pointer given by caller at initial create call time */
        void        *pData;
        
        const char *szUserName;
        const char *szPassword;
        const char *szRecoveryQuestions;
        const char *szRecoveryAnswers;
        tABC_Request_Callback fRequestCallback;
        
        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_AccountSetRecoveryInfo;


    typedef enum eABC_AccountKey
    {
        ABC_AccountKey_L2,
        ABC_AccountKey_LP2,
        ABC_AccountKey_PIN
    } tABC_AccountKey;
    
    tABC_CC ABC_AccountSignInInfoAlloc(tABC_AccountSignInInfo **ppAccountSignInInfo,
                                       const char *szUserName,
                                       const char *szPassword,
                                       tABC_Request_Callback fRequestCallback,
                                       void *pData,
                                       tABC_Error *pError);
    
    void ABC_AccountSignInInfoFree(tABC_AccountSignInInfo *pAccountCreateInfo);
    
    void *ABC_AccountSignInThreaded(void *pData);
    
    tABC_CC ABC_AccountCreateInfoAlloc(tABC_AccountCreateInfo **ppAccountCreateInfo,
                                       const char *szUserName,
                                       const char *szPassword,
                                       const char *szPIN,
                                       tABC_Request_Callback fRequestCallback,
                                       void *pData,
                                       tABC_Error *pError);
    
    void ABC_AccountCreateInfoFree(tABC_AccountCreateInfo *pAccountCreateInfo);
    
    void *ABC_AccountCreateThreaded(void *pData);
    
    tABC_CC ABC_AccountSetRecoveryInfoAlloc(tABC_AccountSetRecoveryInfo **ppAccountSetRecoveryInfo,
                                            const char *szUserName,
                                            const char *szPassword,
                                            const char *szRecoveryQuestions,
                                            const char *szRecoveryAnswers,
                                            tABC_Request_Callback fRequestCallback,
                                            void *pData,
                                            tABC_Error *pError);
    
    void ABC_AccountSetRecoveryInfoFree(tABC_AccountSetRecoveryInfo *pAccountSetRecoveryInfo);
    


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
    
#ifdef __cplusplus
}
#endif

#endif
