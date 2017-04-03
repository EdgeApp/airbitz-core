/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AccountSettings.hpp"
#include "Account.hpp"
#include "../exchange/ExchangeSource.hpp"
#include "../json/JsonObject.hpp"
#include "../login/Login.hpp"
#include "../login/LoginPin2.hpp"
#include "../login/LoginStore.hpp"
#include "../util/Util.hpp"

namespace abcd {

struct BitcoinJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(BitcoinJson, JsonObject)

    ABC_JSON_INTEGER(labelType, "labeltype", ABC_DENOMINATION_UBTC) // Required
    ABC_JSON_INTEGER(satoshi, "satoshi", 100) // Required
};

struct SettingsJson:
    public JsonObject
{
    // Account:
    ABC_JSON_STRING(pin, "PIN", "")
    ABC_JSON_BOOLEAN(disablePinLogin, "disablePINLogin", false)
    ABC_JSON_BOOLEAN(disableFingerprintLogin, "disableFingerprintLogin", false)
    ABC_JSON_INTEGER(pinLoginCount, "pinLoginCount", 0)
    ABC_JSON_INTEGER(minutesAutoLogout, "minutesAutoLogout", 60) // Required
    ABC_JSON_INTEGER(secondsAutoLogout, "secondsAutoLogout", 60*60)
    ABC_JSON_INTEGER(recoveryReminderCount, "recoveryReminderCount", 0)

    // Bitcoin requests:
    ABC_JSON_BOOLEAN(nameOnPayments, "nameOnPayments", false) // Required
    ABC_JSON_STRING(firstName, "firstName", "")
    ABC_JSON_STRING(lastName, "lastName", "")
    ABC_JSON_STRING(nickname, "nickname", "")

    // Spend limits:
    ABC_JSON_BOOLEAN(spendRequirePinEnabled, "spendRequirePinEnabled", true)
    ABC_JSON_INTEGER(spendRequirePinSatoshis, "spendRequirePinSatoshis", 5000000)
    ABC_JSON_BOOLEAN(dailySpendLimitEnabled, "dailySpendLimitEnabled", false)
    ABC_JSON_INTEGER(dailySpendLimitSatoshis, "dailySpendLimitSatoshis", 0)

    // Personalization:
    ABC_JSON_BOOLEAN(advancedFeatures, "advancedFeatures", false) // Required
    ABC_JSON_VALUE(bitcoinDenomination, "bitcoinDenomination",
                   BitcoinJson) // Required
    ABC_JSON_STRING(exchangeRateSource, "exchangeRateSource",
                    exchangeSources.front().c_str())
    ABC_JSON_STRING(language, "language", "en") // Required
    ABC_JSON_INTEGER(numCurrency, "numCurrency",
                     static_cast<int>(Currency::USD)) // Required

    // Servers
    ABC_JSON_BOOLEAN(overrideBitcoinServers, "overrideBitcoinServers", false)
    ABC_JSON_STRING(overrideBitcoinServerList, "overrideBitcoinServerList", "")

    // TODO: Use a string for the currency. Not all currencies have codes.
};

static std::string
settingsPath(const Account &account)
{
    return account.dir() + "Settings.json";
}

static std::string
label(tABC_AccountSettings *pSettings)
{
    std::string out;

    if (ABC_STRLEN(pSettings->szFirstName))
    {
        out += pSettings->szFirstName;
    }

    if (ABC_STRLEN(pSettings->szLastName))
    {
        if (out.size())
            out += ' ';
        out += pSettings->szLastName;
    }

    if (ABC_STRLEN(pSettings->szNickname))
    {
        if (out.size())
            out += " - ";
        out += pSettings->szNickname;
    }

    return out;
}

static Status
accountSettingsPinSync(Login &login, tABC_AccountSettings *settings,
                       bool pinChanged)
{
    DataChunk pin2Key;
    bool pinExists = !!loginPin2Key(pin2Key, login.paths);

    if (settings->bDisablePINLogin)
    {
        // Only delete the PIN if the user *explicitly* asks for that:
        if (pinExists)
        {
            loginPin2Delete(login).log();
        }
    }
    else
    {
        if ((!pinExists || pinChanged) && settings->szPIN)
        {
            DataChunk pin2Key;
            ABC_CHECK(loginPin2Set(pin2Key, login, settings->szPIN));
        }
    }

    return Status();
}

tABC_AccountSettings *
accountSettingsLoad(Account &account)
{
    tABC_AccountSettings *out = structAlloc<tABC_AccountSettings>();

    SettingsJson json;
    json.load(settingsPath(account), account.dataKey()).log();

    // Account:
    out->szPIN = json.pinOk() ? stringCopy(json.pin()) : nullptr;
    out->bDisablePINLogin = json.disablePinLogin();
    out->bDisableFingerprintLogin = json.disableFingerprintLogin();
    out->pinLoginCount = json.pinLoginCount();
    out->secondsAutoLogout = json.secondsAutoLogoutOk() ?
                             json.secondsAutoLogout() :
                             60 * json.minutesAutoLogout();
    out->recoveryReminderCount = json.recoveryReminderCount();

    // Bitcoin requests:
    out->bNameOnPayments = json.nameOnPayments();
    out->szFirstName = json.firstNameOk() ? stringCopy(json.firstName()) : nullptr;
    out->szLastName = json.lastNameOk() ? stringCopy(json.lastName()) : nullptr;
    out->szNickname = json.nicknameOk() ? stringCopy(json.nickname()) : nullptr;

    // Spend limits:
    out->bSpendRequirePin = json.spendRequirePinEnabled();
    out->spendRequirePinSatoshis = json.spendRequirePinSatoshis();
    out->bDailySpendLimit = json.dailySpendLimitEnabled();
    out->dailySpendLimitSatoshis = json.dailySpendLimitSatoshis();

    // Personalization:
    out->bAdvancedFeatures = json.advancedFeatures();
    out->bitcoinDenomination.satoshi = json.bitcoinDenomination().satoshi();
    out->bitcoinDenomination.denominationType =
        json.bitcoinDenomination().labelType();
    out->szExchangeRateSource = stringCopy(json.exchangeRateSource());
    out->szLanguage = stringCopy(json.language());
    out->currencyNum = static_cast<int>(json.numCurrency());

    out->szFullName = stringCopy(label(out));

    if (out->szPIN)
        account.pin = out->szPIN;

    return out;
}

Status
accountSettingsSave(Account &account, tABC_AccountSettings *pSettings)
{
    SettingsJson json;

    // Bitcoin denomination sub-object:
    BitcoinJson bitcoin;
    ABC_CHECK(bitcoin.satoshiSet(pSettings->bitcoinDenomination.satoshi));
    ABC_CHECK(bitcoin.labelTypeSet(
                  pSettings->bitcoinDenomination.denominationType));

    // Account:
    if (pSettings->szPIN)
        ABC_CHECK(json.pinSet(pSettings->szPIN));
    ABC_CHECK(json.disablePinLoginSet(pSettings->bDisablePINLogin));
    ABC_CHECK(json.disableFingerprintLoginSet(pSettings->bDisableFingerprintLogin));
    ABC_CHECK(json.pinLoginCountSet(pSettings->pinLoginCount));
    auto minutesAutoLogout = (59 + pSettings->secondsAutoLogout) / 60;
    ABC_CHECK(json.minutesAutoLogoutSet(minutesAutoLogout));
    ABC_CHECK(json.secondsAutoLogoutSet(pSettings->secondsAutoLogout));
    ABC_CHECK(json.recoveryReminderCountSet(pSettings->recoveryReminderCount));

    // Bitcoin requests:
    ABC_CHECK(json.nameOnPaymentsSet(pSettings->bNameOnPayments));
    if (pSettings->szFirstName)
        ABC_CHECK(json.firstNameSet(pSettings->szFirstName));
    if (pSettings->szLastName)
        ABC_CHECK(json.lastNameSet(pSettings->szLastName));
    if (pSettings->szNickname)
        ABC_CHECK(json.nicknameSet(pSettings->szNickname));

    // Spend limits:
    ABC_CHECK(json.spendRequirePinEnabledSet(pSettings->bSpendRequirePin));
    ABC_CHECK(json.spendRequirePinSatoshisSet(pSettings->spendRequirePinSatoshis));
    ABC_CHECK(json.dailySpendLimitEnabledSet(pSettings->bDailySpendLimit));
    ABC_CHECK(json.dailySpendLimitSatoshisSet(pSettings->dailySpendLimitSatoshis));

    // Personalization:
    ABC_CHECK(json.advancedFeaturesSet(pSettings->bAdvancedFeatures));
    ABC_CHECK(json.bitcoinDenominationSet(bitcoin));
    ABC_CHECK(json.exchangeRateSourceSet(pSettings->szExchangeRateSource));
    ABC_CHECK(json.languageSet(pSettings->szLanguage));
    ABC_CHECK(json.numCurrencySet(pSettings->currencyNum));

    ABC_CHECK(json.save(settingsPath(account), account.dataKey()));

    // Update the PIN package to match:
    bool pinChanged = pSettings->szPIN && pSettings->szPIN != account.pin;
    ABC_CHECK(accountSettingsPinSync(account.login, pSettings, pinChanged));
    if (pSettings->szPIN)
        account.pin = pSettings->szPIN;

    return Status();
}

void
accountSettingsFree(tABC_AccountSettings *pSettings)
{
    if (pSettings)
    {
        ABC_FREE_STR(pSettings->szFirstName);
        ABC_FREE_STR(pSettings->szLastName);
        ABC_FREE_STR(pSettings->szFullName);
        ABC_FREE_STR(pSettings->szNickname);
        ABC_FREE_STR(pSettings->szLanguage);
        ABC_FREE_STR(pSettings->szPIN);
        ABC_FREE_STR(pSettings->szExchangeRateSource);

        ABC_CLEAR_FREE(pSettings, sizeof(tABC_AccountSettings));
    }
}

} // namespace abcd
