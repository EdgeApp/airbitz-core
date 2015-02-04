/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "JsonFile.hpp"
#include "FileIO.hpp"
#include "Json.hpp"

namespace abcd {

Status
JsonFile::load(const std::string &filename)
{
    clear();
    AutoString data;
    ABC_CHECK_OLD(ABC_FileIOReadFileStr(filename.c_str(), &data.get(), &error));
    ABC_CHECK(decode(data.get()));
    return Status();
}

Status
JsonFile::decode(const std::string &data)
{
    clear();
    json_error_t error;
    json_ = json_loadb(data.data(), data.size(), 0, &error);
    if (!json_)
        return ABC_ERROR(ABC_CC_JSONError, error.text);
    return Status();
}

Status
JsonFile::save(const std::string &filename) const
{
    std::string data;
    ABC_CHECK(encode(data));
    ABC_CHECK_OLD(ABC_FileIOWriteFileStr(filename.c_str(), data.c_str(), &error));
    return Status();
}

Status
JsonFile::encode(std::string &result) const
{
    char *raw = json_dumps(json_, JSON_INDENT(4) | JSON_SORT_KEYS);
    if (!raw)
        return ABC_ERROR(ABC_CC_JSONError, "Cannot encode JSON.");
    result = raw;
    ABC_UtilJanssonSecureFree(raw);
    return Status();
}

Status
JsonFile::setRaw(const char *key, json_t *value)
{
    create();
    if (json_object_set_new(json_, key, value) < 0)
        return ABC_ERROR(ABC_CC_JSONError, std::string("Cannot set: ") + key);
    return Status();
}

Status
JsonFile::getRaw(const char *key, json_t *&result) const
{
    json_t *out = json_object_get(json_, key);
    if (!out)
        return ABC_ERROR(ABC_CC_JSONError, std::string("Cannot get: ") + key);
    result = out;
    return Status();
}

json_t *
JsonFile::getRawOptional(const char *key) const
{
    return json_object_get(json_, key);
}

Status
JsonFile::setString(const char *key, const char *value)
{
    create();
    if (json_object_set_new(json_, key, json_string(value)) < 0)
        return ABC_ERROR(ABC_CC_JSONError, std::string("Cannot set: ") + key);
    return Status();
}

Status
JsonFile::getString(const char *key, const char *&result) const
{
    const char *out = json_string_value(json_object_get(json_, key));
    if (!out)
        return ABC_ERROR(ABC_CC_JSONError, std::string("Cannot get: ") + key);
    result = out;
    return Status();
}

const char *
JsonFile::getStringOptional(const char *key, const char *fallback) const
{
    const char *out = json_string_value(json_object_get(json_, key));
    if (!out)
        return fallback;
    return out;
}

void
JsonFile::clear()
{
    if (json_)
        json_decref(json_);
    json_ = nullptr;
}

Status
JsonFile::create()
{
    if (!json_)
        json_ = json_object();
    if (!json_)
        return ABC_ERROR(ABC_CC_JSONError, "Cannot create root object.");
    return Status();
}

} // namespace abcd
