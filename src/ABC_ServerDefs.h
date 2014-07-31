/**
 * @file
 * AirBitz server definitions
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
