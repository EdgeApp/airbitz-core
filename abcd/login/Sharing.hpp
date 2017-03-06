/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_SHARING_HPP
#define ABCD_LOGIN_SHARING_HPP

#include "../util/Status.hpp"
#include "../json/JsonPtr.hpp"
#include <memory>

namespace abcd {

class Login;

/**
 * A reference to an auth-server communications lobby.
 */
struct Lobby
{
    std::string id;
    JsonPtr json;
};

/**
 * Fetches a lobby from the auth server.
 */
Status
lobbyFetch(Lobby &result, const std::string &id);

/**
 * A login request, parsed out of a lobby.
 */
struct LoginRequest
{
    std::string displayName;
    std::string displayImageUrl;
    std::string type;
};

/**
 * Extracts an edge-login request (if any) from the given lobby.
 */
Status
loginRequestLoad(LoginRequest &result, const Lobby &lobby);

/**
 * Approves the edge-login request contained in the given lobby.
 */
Status
loginRequestApprove(Login &login,
                    Lobby &lobby,
                    const std::string &pin="");

} // namespace abcd

#endif
