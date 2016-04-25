/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_ADDRESS_DB_HPP
#define ABCD_WALLET_ADDRESS_DB_HPP

#include "Metadata.hpp"
#include "../bitcoin/Typedefs.hpp"
#include "../json/JsonPtr.hpp"
#include <list>
#include <map>
#include <mutex>

namespace abcd {

class Wallet;

struct TxInOut;
typedef std::map<const std::string, std::string> KeyTable;

struct AddressMeta
{
    size_t index;
    std::string address;
    bool recyclable;
    time_t time;
    int64_t requestAmount = 0;
    Metadata metadata;
};

/**
 * Manages the addresses stored in the wallet sync directory.
 */
class AddressDb
{
public:
    AddressDb(Wallet &wallet);

    /**
     * Loads the addresses off disk.
     */
    Status
    load();

    /**
     * Updates a particular address in the database.
     */
    Status
    save(const AddressMeta &address);

    /**
     * Lists all the addresses in the wallet.
     */
    AddressSet
    list() const;

    /**
     * Returns the private keys for the wallet's addresses.
     */
    KeyTable
    keyTable();

    /**
     * Returns true if the database contains the given address.
     */
    bool
    has(const std::string &address);

    /**
     * Looks up a particular address in the wallet.
     */
    Status
    get(AddressMeta &result, const std::string &address);

    /**
     * Returns a fresh address.
     */
    Status
    getNew(AddressMeta &result);

    /**
     * Sets the recycle bit on the address.
     */
    Status
    recycleSet(const std::string &address, bool recycle);

    /**
     * Marks a transaction's output addresses as having received money.
     */
    Status
    markOutputs(const std::list<TxInOut> &ios);

private:
    mutable std::mutex mutex_;
    Wallet &wallet_;
    const std::string dir_;

    std::map<std::string, AddressMeta> addresses_;
    std::map<std::string, JsonPtr> files_;

    /**
     * Ensures that there are no gaps in the address list,
     * and at there are several extra addresses ready to go.
     */
    Status
    stockpile();

    std::string
    path(const AddressMeta &address);
};

} // namespace abcd

#endif
