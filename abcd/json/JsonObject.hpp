/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_JSON_JSON_OBJECT_HPP
#define ABCD_JSON_JSON_OBJECT_HPP

#include "JsonPtr.hpp"

namespace abcd {

/**
 * A JsonPtr with an object (key-value pair) as it's root element.
 * This allows all sorts of member lookups.
 */
class JsonObject:
    public JsonPtr
{
public:
    JsonObject() {}

    /**
     * Accepts a JSON object for use as the file root.
     * Takes ownership of the passed-in value.
     */
    JsonObject(json_t *root);

    /**
     * Returns true if the object contains a key with the given type.
     */
    Status
    hasValue(const char *key, json_type type) const;

    /**
     * Reads a JSON key-value pair from the object.
     * @return nullptr if there is no value with that key. Do not free.
     */
    json_t *
    getValue(const char *key) const;

    /**
     * Writes a key-value pair to the root object,
     * creating the root if necessary.
     * Takes ownership of the passed-in value.
     */
    Status
    setValue(const char *key, json_t *value);

    // Type helpers:
    Status hasString (const char *key) const;
    Status hasNumber (const char *key) const;
    Status hasBoolean(const char *key) const;
    Status hasInteger(const char *key) const;

    // Read helpers:
    const char *getString (const char *key, const char *fallback) const;
    double      getNumber (const char *key, double fallback) const;
    bool        getBoolean(const char *key, bool fallback) const;
    json_int_t  getInteger(const char *key, json_int_t fallback) const;
};

// Helper macros for implementing JsonObject child classes:

#define ABC_JSON_VALUE(name, key, type) \
    abcd::Status has##name() const              { return hasValue(key, type); } \
    json_t *get##name() const                   { return getValue(key); } \
    abcd::Status set##name(json_t *value)       { return setValue(key, value); }

#define ABC_JSON_STRING(name, key, fallback) \
    abcd::Status has##name() const              { return hasString(key); } \
    const char *get##name() const               { return getString(key, fallback); } \
    abcd::Status set##name(const char *value)   { return setValue(key, json_string(value)); }

#define ABC_JSON_NUMBER(name, key, fallback) \
    abcd::Status has##name() const              { return hasNumber(key); } \
    double get##name() const                    { return getNumber(key, fallback); } \
    abcd::Status set##name(double value)        { return setValue(key, json_real(value)); }

#define ABC_JSON_BOOLEAN(name, key, fallback) \
    abcd::Status has##name() const              { return hasBoolean(key); } \
    bool get##name() const                      { return getBoolean(key, fallback); } \
    abcd::Status set##name(bool value)          { return setValue(key, json_boolean(value)); }

#define ABC_JSON_INTEGER(name, key, fallback) \
    abcd::Status has##name() const              { return hasInteger(key); } \
    json_int_t get##name() const                { return getInteger(key, fallback); } \
    abcd::Status set##name(json_int_t value)    { return setValue(key, json_integer(value)); }

} // namespace abcd

#endif
