/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "JsonPtr.hpp"
#include "../crypto/Crypto.hpp"
#include "../util/FileIO.hpp"
#include "../util/Json.hpp"

namespace abcd {

constexpr size_t loadFlags = 0;
constexpr size_t saveFlags = JSON_INDENT(4) | JSON_SORT_KEYS;

JsonPtr::~JsonPtr()
{
    reset();
}

JsonPtr::JsonPtr():
    root_(nullptr)
{}

JsonPtr::JsonPtr(JsonPtr &&move):
    root_(move.root_)
{
    move.root_ = nullptr;
}

JsonPtr::JsonPtr(const JsonPtr &copy):
    root_(json_incref(copy.root_))
{}

JsonPtr &
JsonPtr::operator=(const JsonPtr &copy)
{
    reset(json_incref(copy.root_));
    return *this;
}

JsonPtr::JsonPtr(json_t *root):
    root_(root)
{}

void
JsonPtr::reset(json_t *root)
{
    if (root_)
        json_decref(root_);
    root_ = root;
}

Status
JsonPtr::load(const std::string &filename)
{
    json_error_t error;
    json_t *root = json_load_file(filename.c_str(), loadFlags, &error);
    if (!root)
        return ABC_ERROR(ABC_CC_JSONError, error.text);
    reset(root);
    return Status();
}

Status
JsonPtr::load(const std::string &filename, DataSlice dataKey)
{
    json_t *root = nullptr;
    ABC_CHECK_OLD(ABC_CryptoDecryptJSONFileObject(filename.c_str(),
        toU08Buf(dataKey), &root, &error));
    reset(root);
    return Status();
}

Status
JsonPtr::decode(const std::string &data)
{
    json_error_t error;
    json_t *root = json_loadb(data.data(), data.size(), loadFlags, &error);
    if (!root)
        return ABC_ERROR(ABC_CC_JSONError, error.text);
    reset(root);
    return Status();
}

Status
JsonPtr::save(const std::string &filename) const
{
    if (json_dump_file(root_, filename.c_str(), saveFlags))
        return ABC_ERROR(ABC_CC_JSONError, "Cannot write JSON file " + filename);
    return Status();
}

Status
JsonPtr::save(const std::string &filename, DataSlice dataKey) const
{
    ABC_CHECK_OLD(ABC_CryptoEncryptJSONFileObject(root_,
        toU08Buf(dataKey), ABC_CryptoType_AES256,
        filename.c_str(), &error));
    return Status();
}

Status
JsonPtr::encode(std::string &result) const
{
    char *raw = json_dumps(root_, saveFlags);
    if (!raw)
        return ABC_ERROR(ABC_CC_JSONError, "Cannot encode JSON.");
    result = raw;
    ABC_UtilJanssonSecureFree(raw);
    return Status();
}

} // namespace abcd
