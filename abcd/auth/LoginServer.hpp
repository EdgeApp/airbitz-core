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

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <time.h>
#include <list>

namespace abcd {

class Account;
class Lobby;
class Login;
class JsonPtr;
struct CarePackage;
struct LoginPackage;

// We need a better way to get this data out than writing to globals:
extern std::string gOtpResetDate;

Status
loginServerGetGeneral(JsonPtr &result);

Status
loginServerGetQuestions(JsonPtr &result);

/**
 * Creates an account on the server.
 */
Status
loginServerCreate(const Lobby &lobby, DataSlice LP1,
                  const CarePackage &carePackage, const LoginPackage &loginPackage,
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
loginServerAvailable(const Lobby &lobby);

/**
 * Saves a rootKey into the account.
 * @param rootKeyBox The new rootKey, encrypted with dataKey
 * @param mnemonicBox The new mnemonic, encrypted with infoKey
 * @param dataKeyBox The old dataKey, encrypted with infoKey
 */
Status
loginServerAccountUpgrade(const Login &login,
                          JsonPtr rootKeyBox, JsonPtr mnemonicBox, JsonPtr dataKeyBox);

/**
 * Changes the password for an account on the server.
 */
Status
loginServerChangePassword(const Login &login,
                          DataSlice newLP1, DataSlice newLRA1,
                          const CarePackage &carePackage, const LoginPackage &loginPackage);

Status
loginServerGetCarePackage(const Lobby &lobby, CarePackage &result);

/**
 * Retrieves the LoginPackage for the account.
 * The caller must supply at least one form of credentials (LP1/LRA1).
 * @param rootKeyBox Returns the rootKey encrypted with the dataKey,
 * but only on accounts that have already been upgraded.
 */
Status
loginServerGetLoginPackage(const Lobby &lobby,
                           DataSlice LP1, DataSlice LRA1, LoginPackage &result, JsonPtr &rootKeyBox);

Status
loginServerGetPinPackage(DataSlice DID, DataSlice LPIN1, std::string &result);

/**
 * Uploads the pin package.
 * @param DID           Device id
 * @param LPIN1         Hashed pin
 * @param ali           Auto-logout interval
 */
Status
loginServerUpdatePinPackage(const Login &login,
                            DataSlice DID, DataSlice LPIN1, const std::string &pinPackage,
                            time_t ali);

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
loginServerOtpReset(const Lobby &lobby);

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

} // namespace abcd

#endif
