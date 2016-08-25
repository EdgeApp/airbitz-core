/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "RepoJson.hpp"
#include "../../crypto/Encoding.hpp"

namespace abcd {

Status
RepoJson::decode(RepoInfo &result, DataSlice dataKey)
{
    DataChunk rawInfo;
    RepoInfoJson infoJson;
    ABC_CHECK(info().decrypt(rawInfo, dataKey));
    ABC_CHECK(infoJson.decode(toString(rawInfo)));
    ABC_CHECK(typeOk());

    DataChunk repoDataKey;
    DataChunk repoSyncKey;
    ABC_CHECK(base16Decode(repoDataKey, infoJson.dataKey()));
    ABC_CHECK(base16Decode(repoSyncKey, infoJson.syncKey()));

    result = RepoInfo
    {
        type(), repoDataKey, base16Encode(repoSyncKey)
    };
    return Status();
}

Status
RepoJson::encode(const RepoInfo &info, DataSlice dataKey)
{
    RepoInfoJson infoJson;
    ABC_CHECK(infoJson.dataKeySet(base16Encode(info.dataKey)));
    ABC_CHECK(infoJson.syncKeySet(info.syncKey));

    JsonBox box;
    ABC_CHECK(box.encrypt(infoJson.encode(), dataKey));
    ABC_CHECK(infoSet(box));
    ABC_CHECK(typeSet(info.type));

    return Status();
}

} // namespace abcd
