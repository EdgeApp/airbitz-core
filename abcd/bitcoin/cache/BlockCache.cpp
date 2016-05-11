/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "BlockCache.hpp"
#include "../../json/JsonObject.hpp"

namespace abcd {

struct BlockCacheJson:
    public JsonObject
{
    ABC_JSON_INTEGER(height, "height", 0)
};

BlockCache::BlockCache(const std::string &path):
    path_(path),
    height_(0)
{
    load().log(); // Failure is fine
}

void
BlockCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    height_ = 0;
    save().log(); // Failure is fine
}

size_t
BlockCache::height() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return height_;
}

void
BlockCache::heightSet(size_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (height_ < height)
    {
        height_ = height;
        save().log(); // Failure is fine

        if (onHeight_)
            onHeight_(height_);
    }
}

void
BlockCache::onHeightSet(const HeightCallback &onHeight)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onHeight_ = onHeight;
}

Status
BlockCache::load()
{
    BlockCacheJson json;
    ABC_CHECK(json.load(path_));
    height_ = json.height();
    return Status();
}

Status
BlockCache::save()
{
    BlockCacheJson json;
    ABC_CHECK(json.heightSet(height_));
    ABC_CHECK(json.save(path_));
    return Status();
}

} // namespace abcd
