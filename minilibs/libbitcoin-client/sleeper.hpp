/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_CLIENT_SLEEPER_HPP
#define LIBBITCOIN_CLIENT_SLEEPER_HPP

#include <chrono>
#include <algorithm>

namespace libbitcoin {
namespace client {

typedef std::chrono::milliseconds sleep_time;

/**
 * An interface for objects that need to perform delayed work in a
 * non-blocking manner.
 *
 * Before going to sleep, the program's main loop should call the `wakeup`
 * method on any objects that implement this interface. This method will
 * return the amount of time until the object wants to be woken up again.
 * The main loop should sleep for this long. On the next time around the
 * loop, calling the `wakeup` method will perform the pending work
 * (assuming enough time has elapsed).
 */
class sleeper
{
public:
    virtual ~sleeper() {}

    /**
     * Performs any pending time-based work, and returns the number of
     * milliseconds between now and the next time work needs to be done.
     * Returns 0 if the class has no future work to do.
     */
    virtual sleep_time wakeup() = 0;
};

/**
 * Returns the smaller of two time periods, treating 0 as infinity.
 */
inline sleep_time min_sleep(sleep_time a, sleep_time b)
{
    if (!a.count())
        return b;
    if (!b.count())
        return a;
    return std::min(a, b);
}

} // namespace client
} // namespace libbitcoin

#endif

