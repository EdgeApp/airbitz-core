/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_HTTP_HTTP_HPP
#define ABCD_HTTP_HTTP_HPP

#include "../util/Status.hpp"

namespace abcd {

/**
 * Initialize the cURL library and sets the CA certificate path.
 */
Status
httpInit(const std::string &certPath);

} // namespace abcd

#endif
