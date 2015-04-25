/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "U08Buf.hpp"

namespace abcd {

void U08BufFree(U08Buf &self)
{
    if (self.p)
    {
        ABC_UtilGuaranteedMemset(self.p, 0, ABC_BUF_SIZE(self));
        free(self.p);
    }
    self = U08Buf();
}

} // namespace abcd
