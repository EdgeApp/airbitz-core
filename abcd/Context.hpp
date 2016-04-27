/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CONTEXT_H
#define ABCD_CONTEXT_H

#include "RootPaths.hpp"
#include <memory>

namespace abcd {

class ExchangeCache;

/**
 * An object holding app-wide information, such as paths.
 */
class Context
{
public:
    ~Context();
    Context(const std::string &rootDir, const std::string &certPath,
            const std::string &apiKey, const std::string &hiddenBitsKey);

    const std::string &apiKey() const { return apiKey_; }
    const std::string &hiddenBitsKey() const { return hiddenBitsKey_; }

private:
    const std::string apiKey_;
    const std::string hiddenBitsKey_;

public:
    RootPaths paths;
    ExchangeCache &exchangeCache;
};

/**
 * The global context instance.
 */
extern std::unique_ptr<Context> gContext;

} // namespace abcd

#endif
