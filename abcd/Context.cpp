/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Context.hpp"
#include "bitcoin/cache/BlockCache.hpp"
#include "exchange/ExchangeCache.hpp"

namespace abcd {

std::unique_ptr<Context> gContext;

Context::~Context()
{
    delete &blockCache;
    delete &exchangeCache;
}

Context::Context(const std::string &rootDir, const std::string &certPath,
                 const std::string &apiKey, const std::string &hiddenBitsKey):
    apiKey_(apiKey),
    hiddenBitsKey_(hiddenBitsKey),
    paths(rootDir, certPath),
    blockCache(*new BlockCache(paths.blockCachePath())),
    exchangeCache(*new ExchangeCache(paths.exchangeCachePath()))
{
}

} // namespace abcd
