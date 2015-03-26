/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_JSON_JSON_PTR_HPP
#define ABCD_JSON_JSON_PTR_HPP

#include "../util/Status.hpp"
#include <jansson.h>
#include <string>

namespace abcd {

/**
 * A json_t smart pointer.
 */
class JsonPtr
{
public:
    ~JsonPtr();
    JsonPtr();
    JsonPtr(JsonPtr &&move);
    JsonPtr(const JsonPtr &copy);
    JsonPtr &operator=(const JsonPtr &copy);

    /**
     * Accepts a JSON value for use as the file root.
     * Takes ownership of the passed-in value.
     */
    JsonPtr(json_t *root);

    /**
     * Frees the JSON root value and replaces it with a new one.
     * Takes ownership of the passed-in value.
     */
    void
    reset(json_t *root=nullptr);

    /**
     * Obtains the root JSON node. Do not free.
     */
    json_t *get() const { return root_; }
    explicit operator bool() const { return root_; }

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
    json_t *root_;
};

/**
 * Adds the standard constructors to JsonPtr child classes.
 */
#define ABC_JSON_CONSTRUCTORS(This, Base) \
    This() {} \
    This(JsonPtr &&move): Base(std::move(move)) {} \
    This(const JsonPtr &copy): Base(copy) {}

} // namespace abcd

#endif
