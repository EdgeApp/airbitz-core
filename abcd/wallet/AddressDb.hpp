/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_ADDRESS_DB_HPP
#define ABCD_WALLET_ADDRESS_DB_HPP

#include "../json/JsonPtr.hpp"
#include "../util/Status.hpp"
#include "TxMetadata.hpp"
#include <map>
#include <mutex>
#include <set>

namespace abcd {

class Wallet;

typedef std::set<std::string> AddressSet;
typedef std::map<const std::string, std::string> KeyTable;

struct Address
{
    size_t index;
    std::string address;
    bool recyclable;
    time_t time;
    TxMetadata metadata;
};

/**
 * Manages the addresses stored in the wallet sync directory.
 */
class AddressDb
{
public:
    AddressDb(const Wallet &wallet);

    /**
     * Loads the addresses off disk.
     */
    Status
    load();

    /**
     * Updates a particular address in the database.
     */
    Status
    save(const Address &address);

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
    get(Address &result, const std::string &address);

    /**
     * Returns a fresh address.
     */
    Status
    getNew(Address &result);

private:
    mutable std::mutex mutex_;
    const Wallet &wallet_;
    const std::string dir_;

    std::map<std::string, Address> addresses_;
    std::map<std::string, JsonPtr> files_;

    /**
     * Ensures that there are no gaps in the address list,
     * and at there are several extra addresses ready to go.
     */
    Status
    stockpile();

    std::string
    path(const Address &address);
};

} // namespace abcd

#endif
