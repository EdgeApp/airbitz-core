/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Sharing.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "server/LoginServer.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonBox.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

/**
 * A request for a login store.
 */
struct LoginRequestJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(LoginRequestJson, JsonObject)

    ABC_JSON_STRING(appId, "appId", nullptr)

    // These are a spoofing security flaw, but we keep them for now:
    ABC_JSON_STRING(displayName, "displayName", "")
    ABC_JSON_STRING(displayImageUrl, "displayImageUrl", "")
};

/**
 * A generic lobby request.
 */
struct LobbyRequestJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(LobbyRequestJson, JsonObject)

    ABC_JSON_INTEGER(timeout, "timeout", 600)
    ABC_JSON_STRING(publicKey, "publicKey", "!bad") // base64
    ABC_JSON_VALUE(loginRequest, "loginRequest", LoginRequestJson)
    // ABC_JSON_VALUE(walletRequest, "walletRequest", WalletRequestJson)
};

/**
 * A generic reply to a lobby request.
 */
struct LobbyReplyJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(LobbyReplyJson, JsonObject)

    ABC_JSON_STRING(publicKey, "publicKey", "!bad") // base64
    ABC_JSON_VALUE(box, "box", JsonBox)
};

/**
 * The top-level lobby JSON format returned by the server.
 */
struct LobbyJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(LobbyJson, JsonObject)

    ABC_JSON_VALUE(request, "request", LobbyRequestJson)
    ABC_JSON_VALUE(replies, "replies", JsonArray)
};

/**
 * Encrypts a lobby reply.
 */
static Status
encryptReply(LobbyReplyJson &result, JsonPtr &replyJson, const Lobby &lobby)
{
    // Make an ephemeral private key:
    bc::ec_secret replyKey;
    do
    {
        DataChunk random;
        ABC_CHECK(randomData(random, replyKey.size()));
        std::copy(random.begin(), random.end(), replyKey.begin());
    }
    while (!bc::verify_private_key(replyKey));
    const auto replyPubkey = bc::secret_to_public_key(replyKey);

    // Derive the secret X coordinate via ECDH:
    bc::ec_point secretPoint = lobby.requestPubkey;
    if (!bc::ec_multiply(secretPoint, replyKey))
        return ABC_ERROR(ABC_CC_EncryptError, "Lobby ECDH error");

    // The secret X coordinate, in big-endian format:
    const auto secretX = DataSlice(secretPoint.data() + 1,
                                   secretPoint.data() + 33);

    // From NIST.SP.800-56Ar2 section 5.8.1:
    uint32_t counter = 1;
    const auto kdfInput = bc::build_data({bc::to_big_endian(counter), secretX});
    const auto dataKey = hmacSha256(kdfInput, std::string("dataKey"));

    // Encrypt the reply:
    JsonBox replyBox;
    ABC_CHECK(replyBox.encrypt(replyJson.encode(), dataKey));

    // Assemble result:
    LobbyReplyJson out;
    ABC_CHECK(out.boxSet(replyBox));
    ABC_CHECK(out.publicKeySet(base64Encode(replyPubkey)));

    result = out;
    return Status();
}

Status
lobbyFetch(Lobby &result, const std::string &id)
{
    Lobby out;
    out.id = id;
    ABC_CHECK(loginServerLobbyGet(out.json, id));

    // Grab the public key:
    LobbyJson lobbyJson(out.json);
    ABC_CHECK(base64Decode(out.requestPubkey, lobbyJson.request().publicKey()));
    if (!bc::verify_public_key(out.requestPubkey))
        return ABC_ERROR(ABC_CC_ParseError, "Invalid lobby public key");

    // Verify the public key:
    auto checksum = bc::sha256_hash(bc::sha256_hash(out.requestPubkey));
    DataChunk idBytes;
    ABC_CHECK(base58Decode(idBytes, id));
    for (unsigned i = 0; i < idBytes.size(); ++i)
    {
        if (idBytes[i] != checksum[i])
            return ABC_ERROR(ABC_CC_DecryptError, "Lobby ECDH integrity error");
    }

    result = out;
    return Status();
}

Status
loginRequestLoad(LoginRequest &result, const Lobby &lobby)
{
    auto requestJson = LobbyJson(lobby.json).request().loginRequest();
    ABC_CHECK(requestJson.appIdOk());

    result.appId = requestJson.appId();
    result.displayName = requestJson.displayName();
    result.displayImageUrl = requestJson.displayImageUrl();
    return Status();
}

Status
loginRequestApprove(Login &login,
                    Lobby &lobby,
                    const std::string &pin)
{
    LoginRequest loginRequest;
    ABC_CHECK(loginRequestLoad(loginRequest, lobby));

    JsonPtr edgeLogin;
    ABC_CHECK(login.makeEdgeLogin(edgeLogin, loginRequest.appId, pin));

    // Upload:
    LobbyReplyJson replyJson;
    ABC_CHECK(encryptReply(replyJson, edgeLogin, lobby));
    ABC_CHECK(loginServerLobbyReply(lobby.id, replyJson));

    return Status();
}

} // namespace abcd
