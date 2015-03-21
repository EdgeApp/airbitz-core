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
    ABC_JSON_CONSTRUCTORS(JsonObject, JsonPtr)

protected:
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

#define ABC_JSON_VALUE(name, key, Type) \
    Type name() const                           { return Type(json_incref(json_object_get(root_, key))); } \
    abcd::Status name##Set(const JsonPtr &value){ return setValue(key, json_incref(value.get())); }

#define ABC_JSON_STRING(name, key, fallback) \
    const char *name() const                    { return getString(key, fallback); } \
    abcd::Status name##Ok() const               { return hasString(key); } \
    abcd::Status name##Set(const char *value)   { return setValue(key, json_string(value)); }

#define ABC_JSON_NUMBER(name, key, fallback) \
    double name() const                         { return getNumber(key, fallback); } \
    abcd::Status name##Ok() const               { return hasNumber(key); } \
    abcd::Status name##Set(double value)        { return setValue(key, json_real(value)); }

#define ABC_JSON_BOOLEAN(name, key, fallback) \
    bool name() const                           { return getBoolean(key, fallback); } \
    abcd::Status name##Ok() const               { return hasBoolean(key); } \
    abcd::Status name##Set(bool value)          { return setValue(key, json_boolean(value)); }

#define ABC_JSON_INTEGER(name, key, fallback) \
    json_int_t name() const                     { return getInteger(key, fallback); } \
    abcd::Status name##Ok() const               { return hasInteger(key); } \
    abcd::Status name##Set(json_int_t value)    { return setValue(key, json_integer(value)); }

} // namespace abcd

#endif
