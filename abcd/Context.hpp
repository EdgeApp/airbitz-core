/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CONTEXT_H
#define ABCD_CONTEXT_H

#include "util/Status.hpp"
#include <memory>

namespace abcd {

/**
 * An object holding app-wide information, such as paths.
 */
class Context
{
public:
    Context(const std::string &rootDir, const std::string &certPath);

    const std::string &rootDir() const { return rootDir_; }
    const std::string &certPath() const { return certPath_; }
    std::string accountsDir() const;
    std::string walletsDir() const { return rootDir_ + "Wallets/"; }

private:
    const std::string rootDir_;
    const std::string certPath_;
};

/**
 * The global context instance.
 */
extern std::unique_ptr<Context> gContext;

} // namespace abcd

#endif
