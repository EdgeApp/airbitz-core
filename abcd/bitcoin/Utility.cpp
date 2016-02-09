/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Utility.hpp"

namespace abcd {

bc::hash_digest
makeNtxid(bc::transaction_type tx)
{
    for (auto &input: tx.inputs)
        input.script = bc::script_type();
    return bc::hash_transaction(tx, bc::sighash::all);
}

} // namespace abcd
