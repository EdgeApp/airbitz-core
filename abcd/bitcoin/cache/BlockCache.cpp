/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "BlockCache.hpp"
#include "../Utility.hpp"
#include "../../crypto/Encoding.hpp"
#include "../../json/JsonArray.hpp"
#include "../../json/JsonObject.hpp"
#include "../../util/Debug.hpp"

namespace abcd {

constexpr time_t onHeaderTimeout = 5;

struct BlockHeaderJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(BlockHeaderJson, JsonObject)

    ABC_JSON_INTEGER(height, "height", 0)
    ABC_JSON_STRING(header, "header", "")
};

struct BlockCacheJson:
    public JsonObject
{
    ABC_JSON_INTEGER(height, "height", 0)
    ABC_JSON_VALUE(headers, "headers", JsonArray)
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
    headers_.clear();
    headersNeeded_.clear();
    dirty_ = true;
}

Status
BlockCache::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    BlockCacheJson json;
    ABC_CHECK(json.load(path_));
    height_ = json.height();

    auto headersJson = json.headers();
    size_t headersSize = headersJson.size();
    for (size_t i = 0; i < headersSize; i++)
    {
        BlockHeaderJson blockHeaderJson(headersJson[i]);
        if (blockHeaderJson.headerOk() && blockHeaderJson.heightOk())
        {
            DataChunk rawHeader;
            ABC_CHECK(base64Decode(rawHeader, blockHeaderJson.header()));
            bc::block_header_type header;
            ABC_CHECK(decodeHeader(header, rawHeader));

            headers_[blockHeaderJson.height()] = std::move(header);
        }
    }

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

        JsonArray headersJson;
        for (const auto &header: headers_)
        {
            bc::data_chunk rawHeader(satoshi_raw_size(header.second));
            bc::satoshi_save(header.second, rawHeader.begin());

            BlockHeaderJson blockHeaderJson;
            ABC_CHECK(blockHeaderJson.heightSet(header.first));
            ABC_CHECK(blockHeaderJson.headerSet(base64Encode(rawHeader)));
            ABC_CHECK(headersJson.append(blockHeaderJson));
        }
        ABC_CHECK(json.headersSet(headersJson));

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

Status
BlockCache::headerTime(time_t &result, size_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = headers_.find(height);
    if (it == headers_.end())
        return ABC_ERROR(ABC_CC_Synchronizing, "Header not available.");

    result = it->second.timestamp;
    return Status();
}

bool
BlockCache::headerInsert(size_t height, const bc::block_header_type &header)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // Do not stomp existing headers:
    if (headers_.end() == headers_.find(height))
    {
        ABC_DebugLog("Adding header %d", height);
        headers_[height] = header;
        dirty_ = true;
        headersDirty_ = true;

        return true;
    }

    return false;
}

void
BlockCache::onHeaderSet(const HeaderCallback &onHeader)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onHeader_ = onHeader;
}

void
BlockCache::onHeaderInvoke(void)
{
    std::unique_lock<std::mutex> lock(mutex_);

    if (onHeader_ && headersDirty_)
    {
        const auto now = time(nullptr);
        if (onHeaderTimeout <= now - onHeaderLastCall_)
        {
            ABC_DebugLog("onHeaderInvoke SENDING NOTIFICATION");
            onHeader_();
            onHeaderLastCall_ = now;
            headersDirty_ = false;
        }
        else
        {
            ABC_DebugLog("onHeaderInvoke PENDING NOTIFICATION");
        }
    }
}

size_t
BlockCache::headerNeeded()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (!headersNeeded_.empty())
    {
        // Pull an item from the set:
        const auto out = *headersNeeded_.begin();
        headersNeeded_.erase(headersNeeded_.begin());

        // Only return the item if it is truly missing:
        if (headers_.end() == headers_.find(out))
            return out;
    }

    // There is none:
    return 0;
}

void
BlockCache::headerNeededAdd(size_t height)
{
    std::unique_lock<std::mutex> lock(mutex_);
    headersNeeded_.insert(height);
}

} // namespace abcd
