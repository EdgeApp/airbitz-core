/**
 * @file
 * AirBitz server definitions
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Defs_h
#define ABC_Defs_h

#define API_KEY_HEADER                      "Authorization: Token airbitz_api_key_goes_here"

#define ABC_SERVER_ROOT                     "http://mainnet.auth.airbitz.co/api/v1"
#define ABC_SERVER_ACCOUNT_CREATE_PATH      "account/create"
#define ABC_SERVER_ACCOUNT_ACTIVATE         "account/activate"
#define ABC_SERVER_GET_CARE_PACKAGE_PATH    "account/carepackage/get"
#define ABC_SERVER_UPDATE_CARE_PACKAGE_PATH "account/carepackage/update"
#define ABC_SERVER_CHANGE_PASSWORD_PATH     "account/password/update"
#define ABC_SERVER_LOGIN_PACK_GET_PATH      "account/loginpackage/get"
#define ABC_SERVER_LOGIN_PACK_UPDATE_PATH   "account/loginpackage/update"
#define ABC_SERVER_WALLET_CREATE_PATH       "wallet/create"
#define ABC_SERVER_WALLET_ACTIVATE_PATH     "wallet/activate"
#define ABC_SERVER_GET_QUESTIONS_PATH       "questions"
#define ABC_SERVER_GET_INFO_PATH            "getinfo"

#define ABC_SERVER_JSON_L1_FIELD            "l1"
// XXX: p1 needs to become lp1
#define ABC_SERVER_JSON_P1_FIELD            "lp1"
#define ABC_SERVER_JSON_REPO_FIELD          "repo_account_key"
#define ABC_SERVER_JSON_EREPO_FIELD         "erepo_account_key"
// XXX: new_p1 needs to become new_lp1
#define ABC_SERVER_JSON_NEW_P1_FIELD        "new_lp1"
#define ABC_SERVER_JSON_LRA1_FIELD          "lra"
#define ABC_SERVER_JSON_CARE_PACKAGE_FIELD  "care_package"
#define ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD "login_package"
#define ABC_SERVER_JSON_CATEGORY_FIELD      "category"
#define ABC_SERVER_JSON_MIN_LENGTH_FIELD    "min_length"
#define ABC_SERVER_JSON_QUESTION_FIELD      "question"

#define ABC_SERVER_JSON_MESSAGE_FIELD       "message"
#define ABC_SERVER_JSON_STATUS_CODE_FIELD   "status_code"
#define ABC_SERVER_JSON_RESULTS_FIELD       "results"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum eABC_Server_Code
    {
        ABC_Server_Code_Success = 0,
        ABC_Server_Code_Error = 1,
        ABC_Server_Code_AccountExists = 2,
        ABC_Server_Code_NoAccount = 3,
        ABC_Server_Code_InvalidPassword = 4,
        ABC_Server_Code_InvalidAnswers = 5
    } tABC_Server_Code;

#ifdef __cplusplus
}
#endif

#endif
