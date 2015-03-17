/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_CATEGORIES_HPP
#define ABCD_ACCOUNT_ACCOUNT_CATEGORIES_HPP

#include "../../src/ABC.h"

namespace abcd {

class Login;

tABC_CC ABC_AccountCategoriesLoad(const Login &login,
                                  char ***paszCategories,
                                  unsigned int *pCount,
                                  tABC_Error *pError);

tABC_CC ABC_AccountCategoriesAdd(const Login &login,
                                 char *szCategory,
                                 tABC_Error *pError);

tABC_CC ABC_AccountCategoriesRemove(const Login &login,
                                    char *szCategory,
                                    tABC_Error *pError);

} // namespace abcd

#endif
