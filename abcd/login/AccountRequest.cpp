/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AccountRequest.hpp"
#include "Login.hpp"
#include "server/LoginServer.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonBox.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

struct AccountRequestJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AccountRequestJson, JsonObject)

    ABC_JSON_STRING(displayName, "displayName", "")
    ABC_JSON_VALUE(infoBox, "infoBox", JsonBox)
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

Status
accountRequest(AccountRequest &result, JsonPtr lobby)
{
    auto requestJson = LobbyJson(lobby).accountRequest();
    ABC_CHECK(requestJson.requestKeyOk());
    ABC_CHECK(requestJson.typeOk());

    result.displayName = requestJson.displayName();
    result.type = requestJson.type();
    return Status();
}

Status
accountRequestApprove(Login &login,
                      const std::string &id,
                      JsonPtr lobby)
{
    auto requestJson = LobbyJson(lobby).accountRequest();
    ABC_CHECK(requestJson.requestKeyOk());
    ABC_CHECK(requestJson.typeOk());

    // Make an ephemeral private key:
    bc::ec_secret replyKey;
    do
    {
        DataChunk random;
        ABC_CHECK(randomData(random, replyKey.size()));
        std::copy(random.begin(), random.end(), replyKey.begin());
    }
    while (!bc::verify_private_key(replyKey));

    // Derive the encryption key via ECDH:
    bc::ec_point requestKey;
    ABC_CHECK(base16Decode(requestKey, requestJson.requestKey()));
    if (!bc::ec_multiply(requestKey, replyKey))
        return ABC_ERROR(ABC_CC_EncryptError, "Lobby ECDH error");
    const auto secret = DataChunk(requestKey.begin() + 1,
                                  requestKey.begin() + 33);
    const auto infoKey = hmacSha256(std::string("infoKey"), secret);

    // Get the repo info we need:
    RepoInfo repoInfo;
    ABC_CHECK(login.repoFind(repoInfo, requestJson.type(), true));
    RepoInfoJson infoJson;
    infoJson.dataKeySet(base16Encode(repoInfo.dataKey));
    infoJson.syncKeySet(repoInfo.syncKey);

    // Update the lobby JSON:
    JsonBox infoBox;
    ABC_CHECK(infoBox.encrypt(infoJson.encode(), infoKey));
    ABC_CHECK(requestJson.infoBoxSet(infoBox));
    ABC_CHECK(requestJson.replyKeySet(base16Encode(
                                          bc::secret_to_public_key(replyKey))));

    // Upload:
    ABC_CHECK(loginServerLobbySet(id, lobby));

    return Status();
}

} // namespace abcd
