/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_CATEGORIES_HPP
#define ABCD_ACCOUNT_ACCOUNT_CATEGORIES_HPP

#include "../util/Sync.hpp"

namespace abcd {

tABC_CC ABC_AccountCategoriesLoad(tABC_SyncKeys *pKeys,
                                  char ***paszCategories,
                                  unsigned int *pCount,
                                  tABC_Error *pError);

tABC_CC ABC_AccountCategoriesAdd(tABC_SyncKeys *pKeys,
                                 char *szCategory,
                                 tABC_Error *pError);

tABC_CC ABC_AccountCategoriesRemove(tABC_SyncKeys *pKeys,
                                    char *szCategory,
                                    tABC_Error *pError);

} // namespace abcd

#endif
