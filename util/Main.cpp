/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Commands.hpp"
#include "../abcd/util/Util.hpp"
#include <stdio.h>
#include <iostream>
#include <string>

using namespace abcd;

#define CA_CERT "./ca-certificates.crt"

/**
 * The main program body.
 */
static Status run(int argc, char *argv[])
{
    std::string program = argv[0];
    if (argc < 3)
        return ABC_ERROR(ABC_CC_Error, "usage: " + program + " <dir> <command> ...\n");

    unsigned char seed[] = {1, 2, 3};
    ABC_CHECK_OLD(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));

    std::string command = argv[2];
    ABC_CHECK(
        command == "account-decrypt"    ? accountDecrypt(argc-3, argv+3) :
        command == "account-encrypt"    ? accountEncrypt(argc-3, argv+3) :
        command == "add-category"       ? addCategory(argc-3, argv+3) :
        command == "change-password"    ? changePassword(argc-3, argv+3) :
        command == "check-password"     ? checkPassword(argc-3, argv+3) :
        command == "check-recovery-answers" ? checkRecoveryAnswers(argc-3, argv+3) :
        command == "create-account"     ? createAccount(argc-3, argv+3) :
        command == "create-wallet"      ? createWallet(argc-3, argv+3) :
        command == "data-sync"          ? dataSync(argc-3, argv+3) :
        command == "generate-addresses" ? generateAddresses(argc-3, argv+3) :
        command == "get-bitcoin-seed"   ? getBitcoinSeed(argc-3, argv+3) :
        command == "get-categories"     ? getCategories(argc-3, argv+3) :
        command == "get-exchange-rate"  ? getExchangeRate(argc-3, argv+3) :
        command == "get-question-choices" ? getQuestionChoices(argc-3, argv+3) :
        command == "get-questions"      ? getQuestions(argc-3, argv+3) :
        command == "get-settings"       ? getSettings(argc-3, argv+3) :
        command == "get-wallet-info"    ? getWalletInfo(argc-3, argv+3) :
        command == "list-accounts"      ? listAccounts(argc-3, argv+3) :
        command == "list-wallets"       ? listWallets(argc-3, argv+3) :
        command == "pin-login"          ? pinLogin(argc-3, argv+3) :
        command == "pin-login-setup"    ? pinLoginSetup(argc-3, argv+3) :
        command == "recovery-reminder-set" ? recoveryReminderSet(argc-3, argv+3) :
        command == "remove-category"    ? removeCategory(argc-3, argv+3) :
        command == "search-bitcoin-seed" ? searchBitcoinSeed(argc-3, argv+3) :
        command == "set-nickname"       ? setNickname(argc-3, argv+3) :
        command == "sign-in"            ? signIn(argc-3, argv+3) :
        command == "upload-logs"        ? uploadLogs(argc-3, argv+3) :
        command == "wallet-decrypt"     ? walletDecrypt(argc-3, argv+3) :
        command == "wallet-encrypt"     ? walletEncrypt(argc-3, argv+3) :
        command == "wallet-get-address" ? walletGetAddress(argc-3, argv+3) :
        command == "washer"             ? washer(argc-3, argv+3) :
        ABC_ERROR(ABC_CC_Error, "unknown command " + command));

    ABC_CHECK_OLD(ABC_ClearKeyCache(&error));
    return Status();
}

int main(int argc, char *argv[])
{
    Status s = run(argc, argv);
    if (!s)
        std::cerr << s << std::endl;
    return s ? 0 : 1;
}
