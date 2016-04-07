/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Cache.hpp"
#include "../../json/JsonObject.hpp"
#include "../../util/FileIO.hpp"

namespace abcd {

Cache::Cache(const std::string &path, BlockCache &blockCache):
    blocks(blockCache),
    path_(path)
{
}

void
Cache::clear()
{
    blocks.clear();
    txs.clear();
    save();
}

Status
Cache::load()
{
    DataChunk data;
    ABC_CHECK(fileLoad(data, path_));
    ABC_CHECK(txs.load(data));
    return Status();
}

Status
Cache::save()
{
    ABC_CHECK(fileSave(txs.serialize(), path_));
    return Status();
}

} // namespace abcd
