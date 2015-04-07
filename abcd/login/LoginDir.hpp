/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Storage backend for login data.
 */

#ifndef ABCD_LOGIN_LOGIN_DIR_HPP
#define ABCD_LOGIN_LOGIN_DIR_HPP

#include "LoginPackages.hpp"
#include "../../src/ABC.h"
#include <list>
#include <string>

namespace abcd {

/**
 * List all the accounts currently on the device.
 */
std::list<std::string>
loginDirList();

/**
 * Locates the account directory for a given username.
 * Returns a blank string if there is no directory.
 */
std::string
loginDirFind(const std::string &username);

tABC_CC ABC_LoginDirCreate(std::string &directory,
                           const char *szUserName,
                           tABC_Error *pError);

tABC_CC ABC_LoginDirFileLoad(char **pszData,
                             const std::string &directory,
                             const char *szFile,
                             tABC_Error *pError);

tABC_CC ABC_LoginDirFileSave(const char *szData,
                             const std::string &directory,
                             const char *szFile,
                             tABC_Error *pError);

tABC_CC ABC_LoginDirFileDelete(const std::string &directory,
                               const char *szFile,
                               tABC_Error *pError);

tABC_CC ABC_LoginDirLoadPackages(const std::string &directory,
                                 CarePackage &carePackage,
                                 LoginPackage &loginPackage,
                                 tABC_Error *pError);

tABC_CC ABC_LoginDirSavePackages(const std::string &directory,
                                 const CarePackage &carePackage,
                                 const LoginPackage &loginPackage,
                                 tABC_Error *pError);

} // namespace abcd

#endif
