/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "JsonArray.hpp"

namespace abcd {

Status
JsonArray::create()
{
    if (!json_is_array(root_))
        reset(json_array());
    if (!root_)
        return ABC_ERROR(ABC_CC_JSONError, "Cannot create array");
    return Status();
}

JsonPtr
JsonArray::operator[](size_t i)
{
    return json_incref(json_array_get(root_, i));
}

Status
JsonArray::append(JsonPtr value)
{
    ABC_CHECK(create());
    if (json_array_append(root_, value.get()) < 0)
        return ABC_ERROR(ABC_CC_JSONError, "Cannot append to array");
    return Status();
}

} // namespace abcd
