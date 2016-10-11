/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_ACCOUNT_REQUEST_HPP
#define ABCD_LOGIN_ACCOUNT_REQUEST_HPP

#include "../util/Status.hpp"
#include "../json/JsonPtr.hpp"
#include <memory>

namespace abcd {

class Login;

struct AccountRequest
{
    std::string displayName;
    std::string displayImageUrl;
    std::string type;
};

/**
 * Extracts an account request (if any) from the given lobby JSON.
 */
Status
accountRequest(AccountRequest &result, JsonPtr lobby);

/**
 * Approves the edge-login request with the given id.
 */
Status
accountRequestApprove(Login &login,
                      const std::string &id,
                      const std::string &pin,
                      JsonPtr lobby);

} // namespace abcd

#endif
