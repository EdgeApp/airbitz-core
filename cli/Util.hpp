/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Utilities and helpers shared between commands.
 */

#ifndef CLI_UTIL_HPP
#define CLI_UTIL_HPP

#include "../abcd/util/Status.hpp"
#include <thread>

struct Session;

/**
 * Launches and runs a watcher thread.
 */
class WatcherThread
{
public:
    ~WatcherThread();

    abcd::Status
    init(const Session &session);

private:
    std::string uuid_;
    std::thread *thread_ = nullptr;
};

#endif
