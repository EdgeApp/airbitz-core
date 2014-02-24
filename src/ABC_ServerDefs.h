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

#define ABC_SERVER_ROOT                     "http://198.61.234.21/api/v1"
#define ABC_SERVER_ACCOUNT_CREATE_PATH      "account/create"
#define ABC_SERVER_UPDATE_CARE_PACKAGE_PATH "account/carepackage/update"
#define ABC_SERVER_GET_QUESTIONS_PATH       "questions"

#define ABC_SERVER_JSON_L1_FIELD            "l1"
#define ABC_SERVER_JSON_P1_FIELD            "p1"
#define ABC_SERVER_JSON_LRA1_FIELD          "lra"
#define ABC_SERVER_JSON_CARE_PACKAGE_FIELD  "care_package"
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
