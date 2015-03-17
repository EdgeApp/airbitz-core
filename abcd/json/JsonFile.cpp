/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "JsonFile.hpp"
#include "../util/FileIO.hpp"
#include "../util/Json.hpp"

namespace abcd {

constexpr size_t loadFlags = 0;
constexpr size_t saveFlags = JSON_INDENT(4) | JSON_SORT_KEYS;

JsonFile::~JsonFile()
{
    reset();
}

JsonFile::JsonFile():
    root_(nullptr)
{}

JsonFile::JsonFile(JsonFile &copy):
    root_(json_incref(copy.root()))
{}

JsonFile &
JsonFile::operator=(JsonFile &copy)
{
    reset(json_incref(copy.root()));
    return *this;
}

JsonFile::JsonFile(json_t *root):
    root_(json_incref(root))
{}

Status
JsonFile::load(const std::string &filename)
{
    json_error_t error;
    json_t *root = json_load_file(filename.c_str(), loadFlags, &error);
    if (!root)
        return ABC_ERROR(ABC_CC_JSONError, error.text);
    reset(root);
    return Status();
}

Status
JsonFile::decode(const std::string &data)
{
    json_error_t error;
    json_t *root = json_loadb(data.data(), data.size(), loadFlags, &error);
    if (!root)
        return ABC_ERROR(ABC_CC_JSONError, error.text);
    reset(root);
    return Status();
}

Status
JsonFile::save(const std::string &filename) const
{
    if (json_dump_file(root_, filename.c_str(), saveFlags))
        return ABC_ERROR(ABC_CC_JSONError, "Cannot write JSON file " + filename);
    return Status();
}

Status
JsonFile::encode(std::string &result) const
{
    char *raw = json_dumps(root_, saveFlags);
    if (!raw)
        return ABC_ERROR(ABC_CC_JSONError, "Cannot encode JSON.");
    result = raw;
    ABC_UtilJanssonSecureFree(raw);
    return Status();
}

void
JsonFile::reset(json_t *root)
{
    if (root_)
        json_decref(root_);
    root_ = root;
}

} // namespace abcd
