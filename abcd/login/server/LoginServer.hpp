/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Functions for communicating with the AirBitz login servers.
 */

#ifndef ABCD_LOGIN_LOGIN_SERVER_HPP
#define ABCD_LOGIN_LOGIN_SERVER_HPP

#include "../../util/Data.hpp"
#include "../../util/Status.hpp"
#include <time.h>
#include <list>

namespace abcd {

class Account;
class AuthJson;
class JsonPtr;
class Login;
class LoginReplyJson;
class LoginStore;
struct CarePackage;
struct LoginPackage;

/**
 * The server returns this along with any OTP error.
 */
struct AuthError
{
    // ABC_CC_InvalidPinWait:
    int pinWait = 0; // Seconds

    // ABC_CC_InvalidOTP:
    std::string otpDate;
    std::string otpToken;
};

Status
loginServerGetGeneral(JsonPtr &result);

Status
loginServerGetQuestions(JsonPtr &result);

/**
 * Creates an account on the server.
 */
Status
loginServerCreate(const LoginStore &store, DataSlice LP1,
                  const CarePackage &carePackage,
                  const LoginPackage &loginPackage,
                  const std::string &syncKey);

/**
 * Activate an account on the server.
 * Should be called once the initial git sync is complete.
 */
Status
loginServerActivate(const Login &login);

/**
 * Queries the server to determine if a username is available.
 */
Status
loginServerAvailable(const LoginStore &store);

/**
 * Saves a rootKey into the account.
 * @param rootKeyBox The new rootKey, encrypted with dataKey
 * @param mnemonicBox The new mnemonic, encrypted with infoKey
 * @param dataKeyBox The old dataKey, encrypted with infoKey
 */
Status
loginServerAccountUpgrade(const Login &login, JsonPtr rootKeyBox,
                          JsonPtr mnemonicBox, JsonPtr dataKeyBox);

/**
 * Changes the password for an account on the server.
 */
Status
loginServerChangePassword(const Login &login,
                          DataSlice newLP1, DataSlice newLRA1,
                          const CarePackage &carePackage,
                          const LoginPackage &loginPackage);

Status
loginServerGetPinPackage(DataSlice DID, DataSlice LPIN1, std::string &result,
                         AuthError &authError);

/**
 * Create a git repository on the server, suitable for holding a wallet.
 */
Status
loginServerWalletCreate(const Login &login, const std::string &syncKey);

/**
 * Lock the server wallet repository, so it is not automatically deleted.
 */
Status
loginServerWalletActivate(const Login &login, const std::string &syncKey);

/**
 * Apply 2-factor authentication to the account.
 */
Status
loginServerOtpEnable(const Login &login, const std::string &otpToken,
                     const long timeout);

/**
 * Remove 2-factor authentication from the account.
 */
Status
loginServerOtpDisable(const Login &login);

/**
 * Determine whether this account requires 2-factor authentication.
 */
Status
loginServerOtpStatus(const Login &login, bool &on, long &timeout);

/**
 * Request a 2-factor authentication reset.
 */
Status
loginServerOtpReset(const LoginStore &store, const std::string &token);

/**
 * Determine which accounts have pending 2-factor authentication resets.
 */
Status
loginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending);

/**
 * Cancel a pending 2-factor authentication reset.
 */
Status
loginServerOtpResetCancelPending(const Login &login);

/**
 * Upload files to auth server for debugging.
 */
Status
loginServerUploadLogs(const Account *account);

/**
 * Accesses the v2 login endpoint.
 */
Status
loginServerLogin(LoginReplyJson &result, AuthJson authJson,
                 AuthError *authError=nullptr);

/**
 * Changes the password on the server using the v2 endpoint.
 */
Status
loginServerPasswordSet(AuthJson authJson,
                       DataSlice passwordAuth,
                       JsonPtr passwordKeySnrp,
                       JsonPtr passwordBox,
                       JsonPtr passwordAuthBox);

/**
 * Sets up PIN v2 login on the server.
 */
Status
loginServerPin2Set(AuthJson authJson,
                   DataSlice pin2Id, DataSlice pin2Auth,
                   JsonPtr pin2Box, JsonPtr pin2KeyBox);

/**
 * Deletes the PIN v2 login from the server.
 */
Status
loginServerPin2Delete(AuthJson authJson);

/**
 * Sets up recovery2 questions the on the server using the v2 endpoint.
 */
Status
loginServerRecovery2Set(AuthJson authJson,
                        DataSlice recovery2Id, JsonPtr recovery2Auth,
                        JsonPtr question2Box, JsonPtr recovery2Box,
                        JsonPtr recovery2KeyBox);

/**
 * Deletes the recovery2 questions from the server.
 */
Status
loginServerRecovery2Delete(AuthJson authJson);

/**
 * Attaches a new repo to the login.
 */
Status
loginServerKeyAdd(AuthJson authJson, JsonPtr keyBox, std::string syncKey="");

/**
 * Checks a collection of usernames for pending messages.
 */
Status
loginServerMessages(JsonPtr &result, const std::list<std::string> &usernames);

/**
 * Downloads the contents of a lobby.
 */
Status
loginServerLobbyGet(JsonPtr &result, const std::string &id);

/**
 * Uploads new contents to a lobby.
 */
Status
loginServerLobbySet(const std::string &id, JsonPtr &lobby,
                    unsigned expires=300);

} // namespace abcd

#endif
