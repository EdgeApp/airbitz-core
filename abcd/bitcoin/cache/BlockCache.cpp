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
    dirty_(false),
    height_(0)
{
}

void
BlockCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    height_ = 0;
    dirty_ = true;
}

Status
BlockCache::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    BlockCacheJson json;
    ABC_CHECK(json.load(path_));
    height_ = json.height();
    dirty_ = false;

    return Status();
}

Status
BlockCache::save()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (dirty_)
    {
        BlockCacheJson json;
        ABC_CHECK(json.heightSet(height_));
        ABC_CHECK(json.save(path_));
        dirty_ = false;
    }

    return Status();
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
        dirty_ = true;

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

} // namespace abcd
