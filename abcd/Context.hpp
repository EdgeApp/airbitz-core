/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CONTEXT_H
#define ABCD_CONTEXT_H

#include "exchange/ExchangeCache.hpp"
#include <memory>

namespace abcd {

/**
 * An object holding app-wide information, such as paths.
 */
class Context
{
public:
    Context(const std::string &rootDir, const std::string &certPath,
            const std::string &apiKeyHeader,const std::string &chainApiUserPwd,
            const std::string &hiddenBitzKey);

    const std::string &rootDir() const { return rootDir_; }
    const std::string &certPath() const { return certPath_; }
    const std::string &apiKeyHeader() const { return apiKeyHeader_; }
    const std::string &chainApiUserPwd() const { return chainApiUserPwd_; }
    const std::string &hiddenBitzKey() const { return hiddenBitzKey_; }
    std::string accountsDir() const;
    std::string walletsDir() const { return rootDir_ + "Wallets/"; }

private:
    const std::string rootDir_;
    const std::string certPath_;
    const std::string apiKeyHeader_;
    const std::string chainApiUserPwd_;
    const std::string hiddenBitzKey_;

public:
    ExchangeCache exchangeCache;
};

/**
 * The global context instance.
 */
extern std::unique_ptr<Context> gContext;

} // namespace abcd

#endif
