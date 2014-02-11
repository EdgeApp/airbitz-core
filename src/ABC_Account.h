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
        const char *szRecoveryQuestions;
        const char *szRecoveryAnswers;
        const char *szPIN;
        tABC_Create_Account_Callback fCreateAccountCallback;
        
        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_AccountCreateInfo;

    typedef enum eAccountKey
    {
        AccountKey_L2,
        AccountKey_LP2
    } tAccountKey;
    
    tABC_CC ABC_AccountCreateInfoAlloc(tABC_AccountCreateInfo **ppAccountCreateInfo,
                                       const char *szUserName,
                                       const char *szPassword,
                                       const char *szRecoveryQuestions,
                                       const char *szRecoveryAnswers,
                                       const char *szPIN,
                                       tABC_Create_Account_Callback fCreateAccountCallback,
                                       void *pData,
                                       tABC_Error *pError);
    
    tABC_CC ABC_AccountCreateInfoFree(tABC_AccountCreateInfo *pAccountCreateInfo,
                                      tABC_Error *pError);
    
    void *ABC_AccountCreateThreaded(void *pData);
    
    tABC_CC ABC_AccountClearKeyCache(tABC_Error *pError);

    tABC_CC ABC_AccountCacheKeys(const char *szUserName, 
                                 const char *szPassword, 
                                 tABC_Error *pError);
    
#ifdef __cplusplus
}
#endif

#endif
