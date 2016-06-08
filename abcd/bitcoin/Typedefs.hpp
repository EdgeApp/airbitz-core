/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * General bitcoin data types.
 */

#ifndef ABCD_BITCOIN_TYPES_HPP
#define ABCD_BITCOIN_TYPES_HPP

#include "../util/Status.hpp"
#include <functional>
#include <set>
#include <string>

namespace libbitcoin {

struct block_header_type;
struct transaction_type;

} // namespace libbitcoin

namespace abcd {

typedef std::set<std::string> AddressSet;
typedef std::set<std::string> TxidSet;

typedef std::function<void(Status)> StatusCallback;

} // namespace abcd

#endif
