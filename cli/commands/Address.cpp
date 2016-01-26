/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/util/Util.hpp"
#include "../../abcd/wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::wallet, CliAddressAllocate, "address-allocate",
        " <count>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto count = atol(argv[0]);

    for(int i = 0; i < count; ++i)
    {
        tABC_TxDetails txDetails;
        AutoString requestId;
        ABC_CHECK_OLD(ABC_CreateReceiveRequest(session.username.c_str(),
                                               session.password.c_str(),
                                               session.uuid.c_str(),
                                               &txDetails,
                                               &requestId.get(),
                                               &error));
        ABC_CHECK_OLD(ABC_FinalizeReceiveRequest(session.username.c_str(),
                      session.password.c_str(),
                      session.uuid.c_str(),
                      requestId, &error));

        std::cout << requestId << std::endl;
    }
    return Status();
}

COMMAND(InitLevel::wallet, CliAddressList, "address-list",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    auto list = session.wallet->addresses.list();
    for (const auto &i: list)
    {
        abcd::Address address;
        ABC_CHECK(session.wallet->addresses.get(address, i));
        std::cout << address.address << " #" <<
                  address.index << ", " <<
                  (address.recyclable ? "recyclable" : "used") << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::wallet, CliAddressCalculate, "address-calculate",
        " <count>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto count = atol(argv[0]);

    bc::hd_private_key m(session.wallet->bitcoinKey());
    bc::hd_private_key m0 = m.generate_private_key(0);
    bc::hd_private_key m00 = m0.generate_private_key(0);
    for (int i = 0; i < count; ++i)
    {
        bc::hd_private_key m00n = m00.generate_private_key(i);
        std::cout << "watch " << m00n.address().encoded() << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::wallet, CliAddressSearch, "address-search",
        " <addr> <start> <end>")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto address = argv[0];
    const auto start = atol(argv[1]);
    const auto end = atol(argv[2]);

    bc::hd_private_key m(session.wallet->bitcoinKey());
    bc::hd_private_key m0 = m.generate_private_key(0);
    bc::hd_private_key m00 = m0.generate_private_key(0);

    for (long i = start, c = 0; i <= end; i++, ++c)
    {
        bc::hd_private_key m00n = m00.generate_private_key(i);
        if (m00n.address().encoded() == address)
        {
            std::cout << "Found " << address << " at " << i << std::endl;
            break;
        }
        if (c == 100000)
        {
            std::cout << i << std::endl;
            c = 0;
        }
    }

    return Status();
}
