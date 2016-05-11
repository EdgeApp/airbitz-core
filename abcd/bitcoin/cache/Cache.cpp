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
    addresses(txs),
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
    JsonObject cacheJson;
    ABC_CHECK(cacheJson.load(path_));
    ABC_CHECK(txs.load(cacheJson));
    ABC_CHECK(addresses.load(cacheJson));
    return Status();
}

Status
Cache::loadLegacy(const std::string &path)
{
    DataChunk data;
    ABC_CHECK(fileLoad(data, path));
    auto serial = bc::make_deserializer(data.begin(), data.end());

    auto now = time(nullptr);
    TxidSet txids;

    try
    {
        // Header bytes:
        auto magic = serial.read_4_bytes();
        if (0xfecdb763 != magic)
        {
            return 0x3eab61c3 == magic ?
                   ABC_ERROR(ABC_CC_ParseError, "Outdated transaction database format") :
                   ABC_ERROR(ABC_CC_ParseError, "Unknown transaction database header");
        }

        // Last block height:
        blocks.heightSet(serial.read_8_bytes());

        while (data.end() != serial.iterator())
        {
            if (0x42 != serial.read_byte())
                return ABC_ERROR(ABC_CC_ParseError, "Unknown cache entry");

            auto txid = bc::encode_hash(serial.read_hash());
            bc::transaction_type tx;
            bc::satoshi_load(serial.iterator(), data.end(), tx);
            const auto step = serial.iterator() + satoshi_raw_size(tx);
            serial.set_iterator(step);

            auto state         = serial.read_byte();
            auto height        = serial.read_8_bytes();
            (void)serial.read_byte(); // Was need_check
            (void)serial.read_hash(); // Was txid
            (void)serial.read_hash(); // Was ntxid
            auto malleated     = serial.read_byte();
            auto masterConfirm = serial.read_byte();

            // The height field is the timestamp for unconfirmed txs:
            time_t timestamp = now;
            if (0 == state)
            {
                timestamp = height;
                height = 0;
            }

            // Malleated transactions can have inaccurate state:
            if (malleated && !masterConfirm)
                height = 0;

            txids.insert(txid);
            txs.insert(tx);
            txs.confirmed(txid, height, timestamp);
        }
    }
    catch (bc::end_of_stream)
    {
        return ABC_ERROR(ABC_CC_ParseError, "Truncated transaction database");
    }

    // The address table doesn't exist,
    // so we rebuild it by pretending to do a bunch of spends:
    for (const auto txid: txids)
    {
        TxInfo info;
        if (txs.info(info, txid))
            addresses.updateSpend(info);
    }

    return Status();
}

Status
Cache::save()
{
    JsonObject cacheJson;
    ABC_CHECK(txs.save(cacheJson));
    ABC_CHECK(addresses.save(cacheJson));
    ABC_CHECK(cacheJson.save(path_));
    return Status();
}

} // namespace abcd
