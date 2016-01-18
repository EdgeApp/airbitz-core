/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Context.hpp"

namespace abcd {

std::unique_ptr<Context> gContext;

Context::Context(const std::string &rootDir, const std::string &certPath,
                 const std::string &apiKey, const std::string &hiddenBitzKey):
    apiKey_(apiKey),
    hiddenBitzKey_(hiddenBitzKey),
    paths(rootDir, certPath),
    exchangeCache(paths.exchangeCachePath())
{}

} // namespace abcd
