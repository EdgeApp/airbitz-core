/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Uri.hpp"
#include "../crypto/Encoding.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace abcd {

// These character classification functions correspond to RFC 3986.
// They avoid C standard library character classification functions,
// since those give different answers based on the current locale.
static bool is_base16(const char c)
{
    return
        ('0' <= c && c <= '9') ||
        ('A' <= c && c <= 'F') ||
        ('a' <= c && c <= 'f');
}
static bool is_alpha(const char c)
{
    return
        ('A' <= c && c <= 'Z') ||
        ('a' <= c && c <= 'z');
}
static bool is_scheme(const char c)
{
    return
        is_alpha(c) || ('0' <= c && c <= '9') ||
        '+' == c || '-' == c || '.' == c;
}
static bool is_pchar(const char c)
{
    return
        is_alpha(c) || ('0' <= c && c <= '9') ||
        '-' == c || '.' == c || '_' == c || '~' == c || // unreserved
        '!' == c || '$' == c || '&' == c || '\'' == c ||
        '(' == c || ')' == c || '*' == c || '+' == c ||
        ',' == c || ';' == c || '=' == c || // sub-delims
        ':' == c || '@' == c;
}
static bool is_path(const char c)
{
    return is_pchar(c) || '/' == c;
}
static bool is_query(const char c)
{
    return is_pchar(c) || '/' == c || '?' == c;
}
static bool is_qchar(const char c)
{
    return is_query(c) && '&' != c && '=' != c;
}

/**
 * Verifies that all RFC 3986 escape sequences in a string are valid,
 * and that all characters belong to the given class.
 */
static bool
validate(const std::string& in, bool (*is_valid)(const char))
{
    auto i = in.begin();
    while (in.end() != i)
    {
        if ('%' == *i)
        {
            if (!(2 < in.end() - i && is_base16(i[1]) && is_base16(i[2])))
                return false;
            i += 3;
        }
        else
        {
            if (!is_valid(*i))
                return false;
            i += 1;
        }
    }
    return true;
}

/**
 * Decodes all RFC 3986 escape sequences in a string.
 */
static std::string
unescape(const std::string& in)
{
    // Do the conversion:
    std::string out;
    out.reserve(in.size());

    auto i = in.begin();
    while (in.end() != i)
    {
        if ('%' == *i &&
            2 < in.end() - i && is_base16(i[1]) && is_base16(i[2]))
        {
            const char temp[] = {i[1], i[2], 0};
            DataChunk value;
            base16Decode(value, temp);
            out.push_back(value[0]);
            i += 3;
        }
        else
        {
            out.push_back(*i);
            i += 1;
        }
    }
    return out;
}

/**
 * Percent-encodes a string.
 * @param is_valid a function returning true for acceptable characters.
 */
static std::string
escape(const std::string& in, bool (*is_valid)(char))
{
    std::ostringstream stream;
    stream << std::hex << std::uppercase << std::setfill('0');
    for (auto c: in)
    {
        if (is_valid(c))
            stream << c;
        else
            stream << '%' << std::setw(2) << +c;
    }
    return stream.str();
}

bool
Uri::decode(const std::string& in, bool strict)
{
    auto i = in.begin();

    // Store the scheme:
    auto start = i;
    while (in.end() != i && ':' != *i)
        ++i;
    scheme_ = std::string(start, i);
    if (scheme_.empty() || !is_alpha(scheme_[0]))
        return false;
    if (!std::all_of(scheme_.begin(), scheme_.end(), is_scheme))
        return false;

    // Consume ':':
    if (in.end() == i)
        return false;
    ++i;

    // Consume "//":
    authority_.clear();
    authorityOk_ = false;
    if (1 < in.end() - i && '/' == i[0] && '/' == i[1])
    {
        authorityOk_ = true;
        i += 2;

        // Store authority part:
        start = i;
        while (in.end() != i && '#' != *i && '?' != *i && '/' != *i)
            ++i;
        authority_ = std::string(start, i);
        if (strict && !validate(authority_, is_pchar))
            return false;
    }

    // Store the path part:
    start = i;
    while (in.end() != i && '#' != *i && '?' != *i)
        ++i;
    path_ = std::string(start, i);
    if (strict && !validate(path_, is_path))
        return false;

    // Consume '?':
    queryOk_ = false;
    if (in.end() != i && '#' != *i)
    {
        queryOk_ = true;
        ++i;
    }

    // Store the query part:
    start = i;
    while (in.end() != i && '#' != *i)
        ++i;
    query_ = std::string(start, i);
    if (strict && !validate(query_, is_query))
        return false;

    // Consume '#':
    fragmentOk_ = false;
    if (in.end() != i)
    {
        fragmentOk_ = true;
        ++i;
    }

    // Store the fragment part:
    fragment_ = std::string(i, in.end());
    if (strict && !validate(fragment_, is_query))
        return false;

    return true;
}

std::string
Uri::encode() const
{
    std::ostringstream out;
    out << scheme_ << ':';
    if (authorityOk_)
        out << "//" << authority_;
    out << path_;
    if (queryOk_)
        out << '?' << query_;
    if (fragmentOk_)
        out << '#' << fragment_;
    return out.str();
}

// Scheme accessors:

std::string
Uri::scheme() const
{
    auto out = scheme_;
    for (auto& c: out)
        if ('A' <= c && c <= 'Z')
            c = c - 'A' + 'a';
    return out;
}

void
Uri::schemeSet(const std::string& scheme)
{
    scheme_ = scheme;
}

// Authority accessors:

std::string
Uri::authority() const
{
    return unescape(authority_);
}

bool
Uri::authorityOk() const
{
    return authorityOk_;
}

void
Uri::authoritySet(const std::string& authority)
{
    authorityOk_ = true;
    authority_ = escape(authority, is_pchar);
}

void
Uri::authorityRemove()
{
    authorityOk_ = false;
}

// Path accessors:

std::string
Uri::path() const
{
    return unescape(path_);
}

void
Uri::pathSet(const std::string& path)
{
    path_ = escape(path, is_path);
}

// Query accessors:

std::string
Uri::query() const
{
    return unescape(query_);
}

bool
Uri::queryOk() const
{
    return queryOk_;
}

void
Uri::querySet(const std::string& query)
{
    queryOk_ = true;
    query_ = escape(query, is_query);
}

void
Uri::queryRemove()
{
    queryOk_ = false;
}

// Fragment accessors:

std::string
Uri::fragment() const
{
    return unescape(fragment_);
}

bool
Uri::fragmentOk() const
{
    return fragmentOk_;
}

void
Uri::fragmentSet(const std::string& fragment)
{
    fragmentOk_ = true;
    fragment_ = escape(fragment, is_query);
}

void
Uri::fragmentRemove()
{
    fragmentOk_ = false;
}

// Query interpretation:

Uri::QueryMap
Uri::queryDecode() const
{
    QueryMap out;

    auto i = query_.begin();
    while (query_.end() != i)
    {
        // Read the key:
        auto begin = i;
        while (query_.end() != i && '&' != *i && '=' != *i)
            ++i;
        auto key = unescape(std::string(begin, i));

        // Consume '=':
        if (query_.end() != i && '&' != *i)
            ++i;

        // Read the value:
        begin = i;
        while (query_.end() != i && '&' != *i)
            ++i;
        out[key] = unescape(std::string(begin, i));

        // Consume '&':
        if (query_.end() != i)
            ++i;
    }

    return out;
}

void
Uri::queryEncode(const QueryMap& map)
{
    bool first = true;
    std::ostringstream query;

    for (const auto& i: map)
    {
        if (!first)
            query << '&';
        first = false;

        query << escape(i.first, is_qchar);
        if (!i.second.empty())
            query << '=' << escape(i.second, is_qchar);
    }

    queryOk_ = true;
    query_ = query.str();
}

void
Uri::authorize()
{
    if (!authorityOk_)
    {
        const auto i = std::find(path_.begin(), path_.end(), '/');
        authority_ = std::string(path_.begin(), i);
        path_.erase(path_.begin(), i);
    }
    authorityOk_ = true;
}

void
Uri::deauthorize()
{
    if (authorityOk_)
        path_ = authority_ + path_;
    authorityOk_ = false;
}

} // namespace abcd
