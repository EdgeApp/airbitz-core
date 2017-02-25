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
#include "../json/JsonBox.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

struct AccountReplyJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AccountReplyJson, JsonObject)

    ABC_JSON_VALUE(info, "info", RepoInfoJson)
    ABC_JSON_STRING(username, "username", nullptr)
    ABC_JSON_STRING(pinString, "pinString", nullptr)
};

struct AccountRequestJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AccountRequestJson, JsonObject)

    ABC_JSON_STRING(displayName, "displayName", "")
    ABC_JSON_STRING(displayImageUrl, "displayImageUrl", "")
    ABC_JSON_VALUE(replyBox, "replyBox", JsonBox)
    ABC_JSON_STRING(replyKey, "replyKey", nullptr)
    ABC_JSON_STRING(requestKey, "requestKey", nullptr)
    ABC_JSON_STRING(type, "type", nullptr)
};

struct LobbyJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(LobbyJson, JsonObject)

    ABC_JSON_VALUE(accountRequest, "accountRequest", AccountRequestJson)
};

/**
 * Given the request's public key,
 * create an encryption key and corresponding public key.
 */
static Status
makeKeys(DataChunk &result, std::string &replyPubkey,
         const std::string &requestPubkey)
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

    // Derive the secret X coordinate via ECDH:
    bc::ec_point requestKey;
    ABC_CHECK(base16Decode(requestKey, requestPubkey));
    if (!bc::ec_multiply(requestKey, replyKey))
        return ABC_ERROR(ABC_CC_EncryptError, "Lobby ECDH error");

    // The secret X coordinate, in big-endian format:
    const auto secretX = DataChunk(requestKey.begin() + 1,
                                   requestKey.begin() + 33);

    // We could have used the KDF from NIST.SP.800-56Ar2 section 5.8.1,
    // `hmac(0x00 | 0x00 | 0x00 | 0x01 | x | otherInfo, salt)`,
    // but this is similar, and we are alredy using it:
    const auto dataKey = hmacSha256(std::string("dataKey"), secretX);

    replyPubkey = base16Encode(bc::secret_to_public_key(replyKey));
    result = dataKey;
    return Status();
}

Status
lobbyFetch(Lobby &result, const std::string &id)
{
    Lobby out;
    out.id = id;
    ABC_CHECK(loginServerLobbyGet(out.json, id));

    result = out;
    return Status();
}

Status
loginRequestLoad(LoginRequest &result, const Lobby &lobby)
{
    auto requestJson = LobbyJson(lobby.json).accountRequest();
    ABC_CHECK(requestJson.requestKeyOk());
    ABC_CHECK(requestJson.typeOk());

    result.displayName = requestJson.displayName();
    result.displayImageUrl = requestJson.displayImageUrl();
    result.type = requestJson.type();
    return Status();
}

Status
loginRequestApprove(Login &login,
                    Lobby &lobby,
                    const std::string &pin)
{
    auto requestJson = LobbyJson(lobby.json).accountRequest();
    ABC_CHECK(requestJson.requestKeyOk());
    ABC_CHECK(requestJson.typeOk());

    // Create the encryption key:
    DataChunk dataKey;
    std::string replyPubkey;
    ABC_CHECK(makeKeys(dataKey, replyPubkey, requestJson.requestKey()));

    // Get the repo info we need:
    RepoInfo repoInfo;
    ABC_CHECK(login.repoFind(repoInfo, requestJson.type(), true));
    RepoInfoJson infoJson;
    infoJson.dataKeySet(base16Encode(repoInfo.dataKey));
    infoJson.syncKeySet(repoInfo.syncKey);

    // Assemble the reply JSON:
    AccountReplyJson replyJson;
    ABC_CHECK(replyJson.infoSet(infoJson));
    ABC_CHECK(replyJson.usernameSet(login.store.username()));
    if (pin.length() == 4)
    {
        ABC_CHECK(replyJson.pinStringSet(pin));
    }
    JsonBox replyBox;
    ABC_CHECK(replyBox.encrypt(replyJson.encode(), dataKey));

    // Update the lobby JSON:
    ABC_CHECK(requestJson.replyBoxSet(replyBox));
    ABC_CHECK(requestJson.replyKeySet(replyPubkey));

    // Upload:
    ABC_CHECK(loginServerLobbySet(lobby.id, lobby.json));

    return Status();
}

} // namespace abcd
