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
 * A base class for implementing JSON-based files.
 */
class JsonPtr
{
public:
    ~JsonPtr();
    JsonPtr();
    JsonPtr(JsonPtr &copy);
    JsonPtr &operator=(JsonPtr &copy);

    /**
     * Accepts a JSON value for use as the file root.
     * Takes ownership of the passed-in value.
     */
    JsonPtr(json_t *root);

    /**
     * Obtains the root JSON node. Do not free.
     */
    json_t *root() { return root_; }
    const json_t *root() const { return root_; }

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
    /**
     * Frees the JSON root object and replaces it with a new one.
     * Takes ownership of the passed-in value.
     */
    void
    reset(json_t *root=nullptr);

    json_t *root_;
};

} // namespace abcd

#endif
