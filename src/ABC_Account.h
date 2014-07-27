/**
 * @file
 * AirBitz Account function prototypes
 *
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_Account_h
#define ABC_Account_h

#include "ABC.h"
#include "ABC_Util.h"

#define SYNC_REPO_KEY_LENGTH 20

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
    } tABC_AccountRequestInfo;

    typedef enum eABC_AccountKey
    {
        ABC_AccountKey_L1,
        ABC_AccountKey_L2,
        ABC_AccountKey_LP2,
        ABC_AccountKey_PIN,
        ABC_AccountKey_RepoAccountKey,
        ABC_AccountKey_RQ
    } tABC_AccountKey;

    /**
     * Contains info on bitcoin miner fee
     */
    typedef struct sABC_AccountMinerFee
    {
        uint64_t amountSatoshi;
        uint64_t sizeTransaction;
    } tABC_AccountMinerFee;

    /**
     * Contains information on AirBitz fees
     */
    typedef struct sABC_AccountAirBitzFee
    {
        double percentage; // maximum value 100.0
        uint64_t minSatoshi;
        uint64_t maxSatoshi;
        char *szAddresss;
    } tABC_AccountAirBitzFee;

    /**
     * Contains general info from the server
     */
    typedef struct sABC_AccountGeneralInfo
    {
        unsigned int            countMinersFees;
        tABC_AccountMinerFee    **aMinersFees;
        tABC_AccountAirBitzFee  *pAirBitzFee;
        unsigned int            countObeliskServers;
        char                    **aszObeliskServers;
        unsigned int            countSyncServers;
        char                    **aszSyncServers;
    } tABC_AccountGeneralInfo;

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

    tABC_CC ABC_AccountTestCredentials(const char *szUserName,
                                       const char *szPassword,
                                       tABC_Error *pError);

    tABC_CC ABC_AccountCheckValidUser(const char *szUserName,
                                      tABC_Error *pError);


    void ABC_AccountFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices);

    tABC_CC ABC_AccountGetRecoveryQuestions(const char *szUserName,
                                            char **pszQuestions,
                                            tABC_Error *pError);

    tABC_CC ABC_AccountServerUpdateGeneralInfo(tABC_Error *pError);

    tABC_CC ABC_AccountLoadGeneralInfo(tABC_AccountGeneralInfo **ppInfo,
                                       tABC_Error *pError);

    void ABC_AccountFreeGeneralInfo(tABC_AccountGeneralInfo *pInfo);

    tABC_CC ABC_AccountLoadSettings(const char *szUserName,
                                    const char *szPassword,
                                    tABC_AccountSettings **ppSettings,
                                    tABC_Error *pError);

    tABC_CC ABC_AccountSaveSettings(const char *szUserName,
                                    const char *szPassword,
                                    tABC_AccountSettings *pSettings,
                                    tABC_Error *pError);

    void ABC_AccountFreeSettings(tABC_AccountSettings *pSettings);

    tABC_CC ABC_AccountPickRepo(const char *szRepoKey, char **szRepoPath, tABC_Error *pError);

    tABC_CC ABC_AccountSyncData(const char *szUserName,
                                const char *szPassword,
                                int *pDirty,
                                tABC_Error *pError);

    // Blocking functions:
    tABC_CC ABC_AccountSignIn(tABC_AccountRequestInfo *pInfo,
                              tABC_Error *pError);

    tABC_CC ABC_AccountCreate(tABC_AccountRequestInfo *pInfo,
                              tABC_Error *pError);

    tABC_CC ABC_AccountSetRecovery(tABC_AccountRequestInfo *pInfo,
                                   tABC_Error *pError);

    tABC_CC ABC_AccountGetQuestionChoices(tABC_AccountRequestInfo *pInfo,
                                          tABC_QuestionChoices **ppQuestionChoices,
                                          tABC_Error *pError);

    tABC_CC ABC_AccountChangePassword(tABC_AccountRequestInfo *pInfo,
                                      tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
