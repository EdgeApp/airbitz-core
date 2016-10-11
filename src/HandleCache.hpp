/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef SRC_HANDLE_CACHE_HPP
#define SRC_HANDLE_CACHE_HPP

#include "../abcd/util/Status.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace abcd {

/**
 * Provides a mapping between opaque integer handles and internal C++ objects.
 */
template <typename T>
class HandleCache
{
public:
    /**
     * Looks up a handle, returning the referenced object.
     */
    Status
    find(std::shared_ptr<T> &result, int handle) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto i = cache_.find(handle);
        if (cache_.end() == i)
            return ABC_ERROR(ABC_CC_NULLPtr, "Invalid handle");

        result = i->second;
        return Status();
    }

    /**
     * Inserts a pointer into the cache, returning a new handle.
     */
    int
    insert(std::shared_ptr<T> p)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto handle = ++lastHandle_;
        cache_[handle] = p;
        return handle;
    }

    /**
     * Removes an item from the cache.
     */
    void
    erase(int handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        cache_.erase(handle);
    }

private:
    mutable std::mutex mutex_;
    int lastHandle_ = 0;
    std::unordered_map<int, std::shared_ptr<T>> cache_;
};

} // namespace abcd

#endif
