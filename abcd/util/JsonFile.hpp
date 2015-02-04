/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_UTIL_JSONFILE_HPP
#define ABCD_UTIL_JSONFILE_HPP

#include "Status.hpp"
#include <jansson.h>
#include <string>

namespace abcd {

/**
 * A base class for implementing JSON-based files.
 */
class JsonFile
{
public:
    ~JsonFile() { clear(); }
    JsonFile(): json_(nullptr) {}

    /**
     * Loads the JSON object from disk.
     */
    Status
    load(const std::string &filename);

    /**
     * Loads the JSON object from an in-memory string.
     */
    Status
    decode(const std::string &data);

    /**
     * Saves the JSON object to disk.
     */
    Status
    save(const std::string &filename) const;

    /**
     * Saves the JSON object to an in-memory string.
     */
    Status
    encode(std::string &result) const;

protected:
    // Read/write helpers:
    Status setRaw(const char *key, json_t *value);
    Status getRaw(const char *key, json_t *&result) const;
    json_t *getRawOptional(const char *key) const;

    Status setString(const char *key, const char *value);
    Status getString(const char *key, const char *&result) const;
    const char *getStringOptional(const char *key, const char *fallback) const;

    /**
     * Frees the JSON root object.
     */
    void
    clear();

    /**
     * Creates an empty JSON object if the root is empty.
     */
    Status
    create();

    json_t *json_;
};

// Helper macros for implementing JsonFile child classes:

#define ABC_JSON_RAW(name, key) \
    Status set##name(json_t *value)             { return setRaw(key, value); } \
    Status get##name(json_t *&result) const     { return getRaw(key, result); }

#define ABC_JSON_RAW_OPTIONAL(name, key) \
    Status set##name(json_t *value)             { return setRaw(key, value); } \
    json_t *get##name() const                   { return getRawOptional(key); }

#define ABC_JSON_STRING(name, key) \
    Status set##name(const char *value)         { return setString(key, value); } \
    Status get##name(const char *&result) const { return getString(key, result); }

#define ABC_JSON_STRING_OPTIONAL(name, key, fallback) \
    Status set##name(const char *value)         { return setString(key, value); } \
    const char *get##name() const               { return getStringOptional(key, fallback); }

} // namespace abcd

#endif
