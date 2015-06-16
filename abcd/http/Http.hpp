/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_HTTP_HTTP_HPP
#define ABCD_HTTP_HTTP_HPP

#include "../util/Status.hpp"

namespace abcd {

/**
 * Initialize the cURL library.
 */
Status
httpInit();

} // namespace abcd

#endif
