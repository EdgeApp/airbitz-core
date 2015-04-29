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

class Account;

tABC_CC ABC_AccountCategoriesLoad(const Account &account,
                                  char ***paszCategories,
                                  unsigned int *pCount,
                                  tABC_Error *pError);

tABC_CC ABC_AccountCategoriesAdd(const Account &account,
                                 char *szCategory,
                                 tABC_Error *pError);

tABC_CC ABC_AccountCategoriesRemove(const Account &account,
                                    char *szCategory,
                                    tABC_Error *pError);

} // namespace abcd

#endif
