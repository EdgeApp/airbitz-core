/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_UTIL_URI_HPP
#define ABCD_UTIL_URI_HPP

#include "../util/Status.hpp"
#include <map>

namespace abcd {

/**
 * A parsed URI according to RFC 3986.
 */
class Uri
{
public:
    /**
     * Decodes a URI from a string.
     * @param strict Set to false to tolerate unescaped special characters.
     */
    bool decode(const std::string &in, bool strict=true);
    std::string encode() const;

    /**
     * Returns the lowercased URI scheme.
     */
    std::string scheme() const;
    void schemeSet(const std::string &scheme);

    /**
     * Obtains the unescaped authority part, if any (user@server:port).
     */
    std::string authority() const;
    bool authorityOk() const;
    void authoritySet(const std::string &authority);
    void authorityRemove();

    /**
     * Obtains the unescaped path part.
     */
    std::string path() const;
    void pathSet(const std::string &path);

    /**
     * Returns the unescaped query string, if any.
     */
    std::string query() const;
    bool queryOk() const;
    void querySet(const std::string &query);
    void queryRemove();

    /**
     * Returns the unescaped fragment string, if any.
     */
    std::string fragment() const;
    bool fragmentOk() const;
    void fragmentSet(const std::string &fragment);
    void fragmentRemove();

    typedef std::map<std::string, std::string> QueryMap;

    /**
     * Interprets the query string as a sequence of key-value pairs.
     * All query strings are valid, so this function cannot fail.
     * The results are unescaped. Both keys and values can be zero-length,
     * and if the same key is appears multiple times, the final one wins.
     */
    QueryMap queryDecode() const;
    void queryEncode(const QueryMap &map);

    /**
     * Ensure that the URI has an authority part,
     * extracting it from the path if necessary.
     * This is useful for fixing URI's that should have a double slash
     * after the scheme, but don't.
     */
    void authorize();
    void deauthorize();

private:
    // All parts are stored with their original escaping:
    std::string scheme_;
    std::string authority_;
    std::string path_;
    std::string query_;
    std::string fragment_;

    bool authorityOk_ = false;
    bool queryOk_ = false;
    bool fragmentOk_ = false;
};

} // namespace abcd

#endif
