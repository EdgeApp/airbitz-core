/**
 * @file
 * AirBitz Core API function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_h
#define ABC_h

#include <stdbool.h>
#include <stdint.h>

/** The maximum buffer length for default strings in the system */
#define ABC_MAX_STRING_LENGTH 256

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * AirBitz Core Condition Codes
     *
     * All AirBitz Core functions return this code.
     * ABC_CC_OK indicates that there was no issue.
     * All other values indication some issue.
     *
     */
    typedef enum eABC_CC
    {
        /** The function completed without an error */
        ABC_CC_Ok = 0,
        /** An error occured */
        ABC_CC_Error = 1,
        /** Unexpected NULL pointer */
        ABC_CC_NULLPtr = 2,
        /** Max number of accounts have been created */
        ABC_CC_NoAvailAccountSpace = 3,
        /** Could not read directory */
        ABC_CC_DirReadError = 4,
        /** Could not open file */
        ABC_CC_FileOpenError = 5,
        /** Could not read from file */
        ABC_CC_FileReadError = 6,
        /** Could not write to file */
        ABC_CC_FileWriteError = 7,
        /** No such file */
        ABC_CC_FileDoesNotExist = 8,
        /** Unknown crypto type */
        ABC_CC_UnknownCryptoType = 9,
        /** Invalid crypto type */
        ABC_CC_InvalidCryptoType = 10,
        /** Decryption error */
        ABC_CC_DecryptError = 11,
        /** Decryption failure due to incorrect key */
        ABC_CC_DecryptFailure = 12,
        /** Encryption error */
        ABC_CC_EncryptError = 13,
        /** Scrypt error */
        ABC_CC_ScryptError = 14,
        /** Account already exists */
        ABC_CC_AccountAlreadyExists = 15,
        /** Account does not exist */
        ABC_CC_AccountDoesNotExist = 16,
        /** JSON parsing error */
        ABC_CC_JSONError = 17,
        /** Incorrect password */
        ABC_CC_BadPassword = 18,
        /** Wallet already exists */
        ABC_CC_WalletAlreadyExists = 19,
        /** URL call failure */
        ABC_CC_URLError = 20,
        /** An call to an external API failed  */
        ABC_CC_SysError = 21,
        /** No required initialization made  */
        ABC_CC_NotInitialized = 22,
        /** Initialization after already initializing  */
        ABC_CC_Reinitialization = 23,
        /** Server error  */
        ABC_CC_ServerError = 24,
        /** The user has not set recovery questions */
        ABC_CC_NoRecoveryQuestions = 25,
        /** Functionality not supported */
        ABC_CC_NotSupported = 26,
        /** Mutex error if some type */
        ABC_CC_MutexError = 27,
        /** Transaction not found */
        ABC_CC_NoTransaction = 28,
        /** Failed to parse input text */
        ABC_CC_ParseError = 29
    } tABC_CC;

    /**
     * AirBitz Request Types
     *
     * The requests results structure contains this
     * identifier to indicate which request it is
     * associated with.
     *
     */
    typedef enum eABC_RequestType
    {
        /** Account sign-in request */
        ABC_RequestType_AccountSignIn = 0,
        /** Create account request */
        ABC_RequestType_CreateAccount = 1,
        /** Set account recovery questions */
        ABC_RequestType_SetAccountRecoveryQuestions = 2,
        /** Create wallet request */
        ABC_RequestType_CreateWallet = 3,
        /** Get Recovery Question Choices request */
        ABC_RequestType_GetQuestionChoices = 4,
        /** Change password request */
        ABC_RequestType_ChangePassword = 5,
        /** Send bitcoin request */
        ABC_RequestType_SendBitcoin = 6
    } tABC_RequestType;

    /**
     * AirBitz Core Error Structure
     *
     * This structure contains the detailed information associated
     * with an error.
     * Most AirBitz Core functions should offer the option of passing
     * a pointer to this structure to be filled out in the event of
     * error.
     *
     */
    typedef struct sABC_Error
    {
        /** The condition code code */
        tABC_CC code;
        /** String containing a description of the error */
        char szDescription[ABC_MAX_STRING_LENGTH + 1];
        /** String containing the function in which the error occurred */
        char szSourceFunc[ABC_MAX_STRING_LENGTH + 1];
        /** String containing the source file in which the error occurred */
        char szSourceFile[ABC_MAX_STRING_LENGTH + 1];
        /** Line number in the source file in which the error occurred */
        int  nSourceLine;
    } tABC_Error;

    /**
     * AirBitz Core Asynchronous Structure
     *
     * This structure contains the detailed information associated
     * with an asynchronous BitCoin event.
     *
     */
    typedef struct sABC_AsyncBitCoinInfo
    {
        /** data pointer given by caller at init */
        void    *pData;

        /** String containing a description of the event */
        char    szDescription[ABC_MAX_STRING_LENGTH + 1];
    } tABC_AsyncBitCoinInfo;

    /**
     * AirBitz Core Request Results Structure
     *
     * This structure contains the detailed information associated
     * with a create account result.
     *
     */
    typedef struct sABC_RequestResults
    {
        /** request type these results are associated with */
        tABC_RequestType    requestType;
        /** data pointer given by caller at initial create call time */
        void                *pData;
        /** data pointer holding return data if the request returns data */
        void                *pRetData;
        /** true if successful */
        bool                bSuccess;
        /** information the error if there was a failure */
        tABC_Error          errorInfo;
    } tABC_RequestResults;

    /**
     * AirBitz Currency Structure
     *
     * This structure contains the id's and names of all the currencies.
     *
     */
    typedef struct sABC_Currency
    {
        /** currency ISO 4217 code */
        char    *szCode;
        /** currency ISO 4217 num */
        int     num;
        /** currency description */
        char    *szDescription;
        /** currency countries */
        char    *szCountries;
    } tABC_Currency;

    /**
     * AirBitz Core Wallet Structure
     *
     * This structure contains wallet information.
     * All AirBitz Core functions should offer the
     *
     */
    typedef struct sABC_WalletInfo
    {
        /** wallet UUID */
        char            *szUUID;
        /** wallet name */
        char            *szName;
        /** account associated with this wallet */
        char            *szUserName;
        /** wallet ISO 4217 currency code */
        int             currencyNum;
        /** wallet attributes */
        unsigned int    attributes;
        /** wallet balance */
        int64_t         balanceSatoshi;
    } tABC_WalletInfo;

    /**
     * AirBitz Question Choice Structure
     *
     * This structure contains a recovery question choice.
     *
     */
    typedef struct sABC_QuestionChoice
    {
        /** question */
        char            *szQuestion;
        /** question category */
        char            *szCategory;
        /** miniumum length of an answer for this question */
        unsigned int    minAnswerLength;
    } tABC_QuestionChoice;

    /**
     * AirBitz Question Choices Structure
     *
     * This structure contains a recovery question choices.
     *
     */
    typedef struct sABC_QuestionChoices
    {
        /** number of choices */
        unsigned int        numChoices;
        /** array of choices */
        tABC_QuestionChoice **aChoices;
    } tABC_QuestionChoices;

    /**
     * AirBitz Bitcoin URI Elements
     *
     * This structure contains elements in
     * a Bitcoin URI
     *
     */
    typedef struct sABC_BitcoinURIInfo
    {
        /** label for that address (e.g. name of receiver) */
        char *szLabel;
        /** bitcoin address (base58) */
        char *szAddress;
        /** message that shown to the user after scanning the QR code */
        char *szMessage;
        /** amount of bitcoins */
        int64_t amountSatoshi;
    } tABC_BitcoinURIInfo;

    /**
     * AirBitz Transaction Details
     *
     * This structure contains details for transactions.
     * It is used in both transactions and transaction
     * requests.
     *
     */
    typedef struct sABC_TxDetails
    {
        /** amount of bitcoins */
        int64_t amountSatoshi;
        /** amount in currency */
        double amountCurrency;
        /** payer or payee */
        char *szName;
        /** category for the transaction */
        char *szCategory;
        /** notes for the transaction */
        char *szNotes;
        /** attributes for the transaction */
        unsigned int attributes;
    } tABC_TxDetails;

    /**
     * AirBitz Transaction Info
     *
     * This structure contains info for a transaction.
     *
     */
    typedef struct sABC_TxInfo
    {
        /** transaction identifier */
        char *szID;
        /** time of creation */
        int64_t timeCreation;
        /** transaction details */
        tABC_TxDetails *pDetails;
    } tABC_TxInfo;

    /**
     * AirBitz Request Info
     *
     * This structure contains info for a request.
     *
     */
    typedef struct sABC_RequestInfo
    {
        /** request identifier */
        char *szID;
        /** time of creation */
        int64_t timeCreate;
        /** request details */
        tABC_TxDetails *pDetails;
    } tABC_RequestInfo;

    /**
     * AirBitz Asynchronous BitCoin event callback
     *
     * This is the form of the callback that will be called when there is an
     * asynchronous BitCoin event.
     *
     */
    typedef void (*tABC_BitCoin_Event_Callback)(const tABC_AsyncBitCoinInfo *pInfo);

    /**
     * AirBitz Request callback
     *
     * This is the form of the callback that will be called when a request
     * call has completed.
     *
     */
    typedef void (*tABC_Request_Callback)(const tABC_RequestResults *pResults);


    tABC_CC ABC_Initialize(const char                   *szRootDir,
                           tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                           void                         *pData,
                           const unsigned char          *pSeedData,
                           unsigned int                 seedLength,
                           tABC_Error                   *pError);

    void ABC_Terminate();

    tABC_CC ABC_ClearKeyCache(tABC_Error *pError);

    tABC_CC ABC_SignIn(const char *szUserName,
                       const char *szPassword,
                       tABC_Request_Callback fRequestCallback,
                       void *pData,
                       tABC_Error *pError);

    tABC_CC ABC_CreateAccount(const char *szUserName,
                              const char *szPassword,
                              const char *szPIN,
                              tABC_Request_Callback fRequestCallback,
                              void *pData,
                              tABC_Error *pError);

    tABC_CC ABC_SetAccountRecoveryQuestions(const char *szUserName,
                                            const char *szPassword,
                                            const char *szRecoveryQuestions,
                                            const char *szRecoveryAnswers,
                                            tABC_Request_Callback fRequestCallback,
                                            void *pData,
                                            tABC_Error *pError);

    tABC_CC ABC_CreateWallet(const char *szUserName,
                             const char *szPassword,
                             const char *szWalletName,
                             int        currencyNum,
                             unsigned int attributes,
                             tABC_Request_Callback fRequestCallback,
                             void *pData,
                             tABC_Error *pError);

    tABC_CC ABC_GetCurrencies(tABC_Currency **paCurrencyArray,
                              int *pCount,
                             tABC_Error *pError);

    tABC_CC ABC_GetPIN(const char *szUserName,
                       const char *szPassword,
                       char **pszPIN,
                       tABC_Error *pError);

    tABC_CC ABC_SetPIN(const char *szUserName,
                       const char *szPassword,
                       const char *szPIN,
                       tABC_Error *pError);

    tABC_CC ABC_GetCategories(const char *szUserName,
                              char ***paszCategories,
                              unsigned int *pCount,
                              tABC_Error *pError);

    tABC_CC ABC_AddCategory(const char *szUserName,
                            char *szCategory,
                            tABC_Error *pError);

    tABC_CC ABC_RemoveCategory(const char *szUserName,
                               char *szCategory,
                               tABC_Error *pError);

    tABC_CC ABC_RenameWallet(const char *szUserName,
                             const char *szPassword,
                             const char *szUUID,
                             const char *szNewWalletName,
                             tABC_Error *pError);

    tABC_CC ABC_SetWalletAttributes(const char *szUserName,
                                    const char *szPassword,
                                    const char *szUUID,
                                    unsigned int attributes,
                                    tABC_Error *pError);

    tABC_CC ABC_CheckRecoveryAnswers(const char *szUserName,
                                     const char *szRecoveryAnswers,
                                     bool *pbValid,
                                     tABC_Error *pError);

    tABC_CC ABC_GetWalletInfo(const char *szUserName,
                              const char *szPassword,
                              const char *szUUID,
                              tABC_WalletInfo **ppWalletInfo,
                              tABC_Error *pError);

    void ABC_FreeWalletInfo(tABC_WalletInfo *pWalletInfo);

    tABC_CC ABC_GetWallets(const char *szUserName,
                           const char *szPassword,
                           tABC_WalletInfo ***paWalletInfo,
                           unsigned int *pCount,
                           tABC_Error *pError);

    void ABC_FreeWalletInfoArray(tABC_WalletInfo **aWalletInfo,
                                 unsigned int nCount);

    tABC_CC ABC_SetWalletOrder(const char *szUserName,
                               const char *szPassword,
                               char **aszUUIDArray,
                               unsigned int countUUIDs,
                               tABC_Error *pError);

    tABC_CC ABC_GetQuestionChoices(const char *szUserName,
                                   tABC_Request_Callback fRequestCallback,
                                   void *pData,
                                   tABC_Error *pError);

    void ABC_FreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices);

    tABC_CC ABC_GetRecoveryQuestions(const char *szUserName,
                                     char **pszQuestions,
                                     tABC_Error *pError);

    tABC_CC ABC_ChangePassword(const char *szUserName,
                               const char *szPassword,
                               const char *szNewPassword,
                               const char *szNewPIN,
                               tABC_Request_Callback fRequestCallback,
                               void *pData,
                               tABC_Error *pError);

    tABC_CC ABC_ChangePasswordWithRecoveryAnswers(const char *szUserName,
                                                  const char *szRecoveryAnswers,
                                                  const char *szNewPassword,
                                                  const char *szNewPIN,
                                                  tABC_Request_Callback fRequestCallback,
                                                  void *pData,
                                                  tABC_Error *pError);

    tABC_CC ABC_ParseBitcoinURI(const char *szURI,
                                tABC_BitcoinURIInfo **ppInfo,
                                tABC_Error *pError);

    void ABC_FreeURIInfo(tABC_BitcoinURIInfo *pInfo);

    double ABC_SatoshiToBitcoin(int64_t satoshi);

    int64_t ABC_BitcoinToSatoshi(double bitcoin);

    tABC_CC ABC_SatoshiToCurrency(int64_t satoshi,
                                  double *pCurrency,
                                  int currencyNum,
                                  tABC_Error *pError);

    tABC_CC ABC_CurrencyToSatoshi(double currency,
                                  int currencyNum,
                                  int64_t *pSatoshi,
                                  tABC_Error *pError);

    tABC_CC ABC_CreateReceiveRequest(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     tABC_TxDetails *pDetails,
                                     char **pszRequestID,
                                     tABC_Error *pError);

    tABC_CC ABC_ModifyReceiveRequest(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szRequestID,
                                     tABC_TxDetails *pDetails,
                                     tABC_Error *pError);

    tABC_CC ABC_FinalizeReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       const char *szRequestID,
                                       tABC_Error *pError);

    tABC_CC ABC_CancelReceiveRequest(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szRequestID,
                                     tABC_Error *pError);

    tABC_CC ABC_GenerateRequestQRCode(const char *szUserName,
                                      const char *szPassword,
                                      const char *szWalletUUID,
                                      const char *szRequestID,
                                      unsigned char **paData,
                                      unsigned int *pWidth,
                                      tABC_Error *pError);

    tABC_CC ABC_InitiateSendRequest(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szDestAddress,
                                    tABC_TxDetails *pDetails,
                                    tABC_Request_Callback fRequestCallback,
                                    void *pData,
                                    tABC_Error *pError);

    tABC_CC ABC_GetTransactions(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                tABC_TxInfo ***paTransactions,
                                unsigned int *pCount,
                                tABC_Error *pError);

    void ABC_FreeTransactions(tABC_TxInfo **aTransactions,
                              unsigned int count);

    tABC_CC ABC_SetTransactionDetails(const char *szUserName,
                                      const char *szPassword,
                                      const char *szWalletUUID,
                                      const char *szID,
                                      tABC_TxDetails *pDetails,
                                      tABC_Error *pError);

    tABC_CC ABC_GetPendingRequests(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   tABC_RequestInfo ***paRequests,
                                   unsigned int *pCount,
                                   tABC_Error *pError);

    void ABC_FreeRequests(tABC_RequestInfo **aRequests,
                          unsigned int count);

    // temp functions
    void tempEventA();
    void tempEventB();

#ifdef __cplusplus
}
#endif

#endif
