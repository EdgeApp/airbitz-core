/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "ServerDefs.hpp"
#include "TwoFactor.hpp"
#include "../util/Crypto.hpp"
#include "../util/FileIO.hpp"
#include "../util/Json.hpp"
#include "../util/Mutex.hpp"
#include "../util/URL.hpp"
#include "../util/Util.hpp"
#include <jansson.h>
#include <qrencode.h>
#include <map>
#include <string>
#include <vector>

#define EXP_DUR 86400

#define TIME_STEP 30
#define OTP_SECRET_LEN 32
#define OTP_FILENAME "Otp.json"
#define OTP_TOKEN_LENGTH 6
#define JSON_OTP_SECRET_FIELD "otp_secret"

namespace abcd {

static char             *gOtpSecret = NULL;
static bool             gbInitialized = false;
static pthread_mutex_t  gMutex;

static tABC_CC ABC_TwoFactorStoreSecret(tABC_Login *pSelf,
                                        const char *szSecret,
                                        tABC_Error *pError);
static tABC_CC ABC_TwoFactorReadSecret(tABC_Login *pSelf,
                                       char **szSecret,
                                       tABC_Error *pError);

static tABC_CC ABC_TwoFactorLock(tABC_Error *pError);
static tABC_CC ABC_TwoFactorUnlock(tABC_Error *pError);


/**
 * Initialize the Two Factor system
 */
tABC_CC ABC_TwoFactorInitialize(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_TwoFactor has already been initalized");

    // create a mutex to block multiple threads from accessing files at the same time
    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_Mutex could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_FileIO could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_Mutex could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gbInitialized = true;

exit:

    return cc;
}

void ABC_TwoFactorTerminate()
{
    if (gbInitialized == true)
    {
        pthread_mutex_destroy(&gMutex);

        gbInitialized = false;
    }
}

tABC_CC ABC_TwoFactorEnable(tABC_Login *pSelf,
                            tABC_U08Buf L1,
                            tABC_U08Buf LP1,
                            long timeout,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szSecret = NULL;
    tABC_U08Buf OTP_SECRET = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_CryptoCreateRandomData(OTP_SECRET_LEN, &OTP_SECRET, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(OTP_SECRET, &szSecret, pError));

    // Store on Auth server
    ABC_CHECK_RET(ABC_LoginServerOtpEnable(L1, LP1, szSecret, timeout, pError));

    // Write to disk and set global variable
    ABC_CHECK_RET(ABC_TwoFactorStoreSecret(pSelf, szSecret, pError));

exit:
    ABC_BUF_FREE(OTP_SECRET);
    ABC_FREE_STR(szSecret);
    return cc;
}

tABC_CC ABC_TwoFactorDisable(tABC_Login *pSelf,
                             tABC_U08Buf L1,
                             tABC_U08Buf LP1,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // Disable otp on server
    ABC_CHECK_RET(ABC_LoginServerOtpDisable(L1, LP1, pError));

    // Delete the 2FA file
    ABC_CHECK_RET(ABC_LoginDirFileDelete(pSelf->directory, OTP_FILENAME, pError));
exit:
    return cc;
}

tABC_CC ABC_TwoFactorCacheSecret(tABC_Login *pSelf, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_TwoFactorGetSecret(pSelf, NULL, pError));
exit:
    return cc;
}

tABC_CC ABC_TwoFactorGetSecret(tABC_Login *pSelf,
                               char **pszSecret,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szSecret = NULL;

    ABC_CHECK_RET(ABC_TwoFactorLock(pError));

    ABC_CHECK_RET(ABC_TwoFactorReadSecret(pSelf, &szSecret, pError));
    if (szSecret != NULL)
    {
        // Store in global
        ABC_STRDUP(gOtpSecret, szSecret);
        if (pszSecret != NULL)
        {
            ABC_STRDUP(*pszSecret, szSecret);
        }
    }
exit:
    ABC_CHECK_RET(ABC_TwoFactorUnlock(NULL));
    ABC_FREE_STR(szSecret);
    return cc;
}

tABC_CC ABC_TwoFactorGetQrCode(tABC_Login *pSelf, unsigned char **paData,
    unsigned int *pWidth, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szSecret = NULL;
    QRcode *qr = NULL;
    unsigned char *aData = NULL;
    unsigned int length = 0;

    ABC_CHECK_RET(ABC_TwoFactorGetSecret(pSelf, &szSecret, pError));

    ABC_DebugLog("Encoding: %s", szSecret);
    qr = QRcode_encodeString(szSecret, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    ABC_CHECK_ASSERT(qr != NULL, ABC_CC_Error, "Unable to create QR code");
    length = qr->width * qr->width;
    ABC_ARRAY_NEW(aData, length, unsigned char);
    for (unsigned i = 0; i < length; i++)
    {
        aData[i] = qr->data[i] & 0x1;
    }
    *pWidth = qr->width;
    *paData = aData;
    aData = NULL;
exit:
    ABC_FREE_STR(szSecret);
    QRcode_free(qr);
    ABC_CLEAR_FREE(aData, length);
    return cc;
}

tABC_CC ABC_TwoFactorSetSecret(tABC_Login *pSelf,
                               const char *szSecret,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_CHECK_RET(ABC_TwoFactorLock(pError));

    // Write to disk
    if (pSelf != NULL)
    {
        ABC_CHECK_RET(ABC_TwoFactorStoreSecret(pSelf, szSecret, pError));
    }

    // Set global variable
    ABC_STRDUP(gOtpSecret, szSecret);
exit:
    ABC_CHECK_RET(ABC_TwoFactorUnlock(NULL));
    return cc;
}

tABC_CC ABC_TwoFactorGetToken(char **pszToken, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    long c;
    char *szCounter = NULL;
    tABC_U08Buf K = ABC_BUF_NULL;
    tABC_U08Buf Token = ABC_BUF_NULL;
    tABC_U08Buf C = ABC_BUF_NULL;
    char *szBase64 = NULL;

    ABC_CHECK_RET(ABC_TwoFactorLock(pError));
    // No otp populated return NULL;
    if (gOtpSecret == NULL) {
        *pszToken = NULL;
        goto exit;
    }

    c = (long) time(NULL) / TIME_STEP;
    ABC_STRDUP(szCounter, std::to_string(c).c_str());

    ABC_BUF_SET_PTR(C, (unsigned char *)szCounter, strlen(szCounter));
    ABC_BUF_SET_PTR(K, (unsigned char *)gOtpSecret, strlen(gOtpSecret));

    ABC_CHECK_RET(ABC_CryptoHMAC256(C, K, &Token, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(Token, &szBase64, pError));
    ABC_CHECK_ASSERT(
        strlen(szBase64) >= OTP_TOKEN_LENGTH, ABC_CC_Error, "Unable to build 2FA token");

    // Take the last OTP_TOKEN_LENGTH bytes of the token
    ABC_STRDUP(*pszToken, szBase64 + (strlen(szBase64) - OTP_TOKEN_LENGTH));
exit:
    ABC_CHECK_RET(ABC_TwoFactorUnlock(NULL));
    ABC_BUF_FREE(Token);
    ABC_FREE_STR(szBase64);
    ABC_FREE_STR(szCounter);

    return cc;
}

tABC_CC ABC_TwoFactorReset(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // Reset otp on server
    ABC_CHECK_RET(ABC_LoginServerOtpReset(L1, LP1, pError));
exit:
    return cc;
}

tABC_CC ABC_TwoFactorPending(std::vector<tABC_U08Buf>& users,
    std::vector<bool>& isPending, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_CHECK_RET(ABC_LoginServerOtpPending(users, isPending, pError));
exit:
    return cc;
}

tABC_CC ABC_TwoFactorCancelPending(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_CHECK_RET(ABC_LoginServerOtpResetCancelPending(L1, LP1, pError));
exit:
    return cc;
}

static tABC_CC ABC_TwoFactorStoreSecret(tABC_Login *pSelf,
                                        const char *szSecret,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_t *pLocal = NULL;
    char *szLocal = NULL;
    char *szEncLocal = NULL;

    pLocal = json_pack("{ss}", JSON_OTP_SECRET_FIELD, szSecret);
    ABC_CHECK_NULL(pLocal);
    szLocal = ABC_UtilStringFromJSONObject(pLocal, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_NULL(szLocal);

    // Store string in Buf
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLocal, pSelf->directory, OTP_FILENAME, pError));
exit:
    if (pLocal)      json_decref(pLocal);
    ABC_FREE_STR(szLocal);
    ABC_FREE_STR(szEncLocal);
    return cc;
}

static tABC_CC ABC_TwoFactorReadSecret(tABC_Login *pSelf,
                                       char **szSecret,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    bool bExists = false;
    char *szEncJSON = NULL;
    char *szJSON = NULL;
    json_t *root = NULL;
    json_t *jsonVal = NULL;
    json_error_t je;

    ABC_CHECK_RET(ABC_TwoFactorLock(pError));

    ABC_CHECK_RET(ABC_LoginDirFileExists(&bExists, pSelf->directory, OTP_FILENAME, pError));
    if (!bExists)
    {
        goto exit;
    }
    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szJSON, pSelf->directory, OTP_FILENAME, pError));

    root = json_loads(szJSON, 0, &je);
    ABC_CHECK_ASSERT(root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(root), ABC_CC_JSONError, "Error parsing JSON account name");

    jsonVal = json_object_get(root, JSON_OTP_SECRET_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing A secret");

    ABC_STRDUP(*szSecret, json_string_value(jsonVal));
exit:
    ABC_CHECK_RET(ABC_TwoFactorUnlock(NULL));

    ABC_FREE_STR(szJSON);
    ABC_FREE_STR(szEncJSON);
    if (root)           json_decref(root);
    if (jsonVal)        json_decref(jsonVal);

    return cc;
}

/**
 * Locks the 2fa mutex
 */
tABC_CC ABC_TwoFactorLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Mutex has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_Mutex error locking mutex");

exit:

    return cc;
}

/**
 * Unlocks the 2fa mutex
 */
tABC_CC ABC_TwoFactorUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Mutex has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_Mutex error unlocking mutex");

exit:

    return cc;
}


} // namespace abcd
