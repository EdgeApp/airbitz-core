/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Data.hpp"

namespace abcd {

DataChunk
buildData(std::initializer_list<DataSlice> slices)
{
    size_t size = 0;
    for (auto slice: slices)
        size += slice.size();

    DataChunk out;
    out.reserve(size);
    for (auto slice: slices)
        out.insert(out.end(), slice.begin(), slice.end());
    return out;
}

} // namespace abcd
