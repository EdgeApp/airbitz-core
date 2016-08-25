/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_BIT_ID_HPP
#define ABCD_LOGIN_BIT_ID_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

class Uri;
class Wallet;

/**
 * Extracts the callback URI from a bitid URI.
 */
Status
bitidCallback(Uri &result, const std::string &bitidUri, bool strict=true);

/**
 * A signed Bitid message.
 */
struct BitidSignature
{
    std::string address;
    std::string signature;
};

/**
 * Signs a message.
 * @param index allows multiple keys for the same site.
 */
BitidSignature
bitidSign(DataSlice rootKey, const std::string &message,
          const std::string &callbackUri, uint32_t index=0);

/**
 * Performs a BitID login to the specified URI.
 */
Status
bitidLogin(DataSlice rootKey, const std::string &bitidUri, uint32_t index=0,
           Wallet *wallet=nullptr, const std::string &kycUri="");

} // namespace abcd

#endif
