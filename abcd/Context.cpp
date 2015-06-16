/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Context.hpp"
#include "bitcoin/Testnet.hpp"

namespace abcd {

std::unique_ptr<Context> gContext;

static std::string
slashify(const std::string &s)
{
    return s.back() == '/' ? s : s + '/';
}

Context::Context(const std::string &rootDir, const std::string &certPath):
    rootDir_(slashify(rootDir)),
    certPath_(certPath),
    exchangeCache(rootDir_)
{}

std::string
Context::accountsDir() const
{
    if (isTestnet())
        return rootDir_ + "Accounts-testnet/";
    else
        return rootDir_ + "Accounts/";
}

} // namespace abcd

