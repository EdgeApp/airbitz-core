/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef commands_h
#define commands_h

#include "../src/ABC.h"
#include "../abcd/util/Status.hpp"

abcd::Status accountDecrypt(int argc, char *argv[]);
abcd::Status accountEncrypt(int argc, char *argv[]);
abcd::Status addCategory(int argc, char *argv[]);
abcd::Status changePassword(int argc, char *argv[]);
abcd::Status checkPassword(int argc, char *argv[]);
abcd::Status checkRecoveryAnswers(int argc, char *argv[]);
abcd::Status createAccount(int argc, char *argv[]);
abcd::Status createWallet(int argc, char *argv[]);
abcd::Status dataSync(int argc, char *argv[]);
abcd::Status generateAddresses(int argc, char *argv[]);
abcd::Status getBitcoinSeed(int argc, char *argv[]);
abcd::Status getCategories(int argc, char *argv[]);
abcd::Status getExchangeRate(int argc, char *argv[]);
abcd::Status getQuestionChoices(int argc, char *argv[]);
abcd::Status getSettings(int argc, char *argv[]);
abcd::Status getWalletInfo(int argc, char *argv[]);
abcd::Status listWallets(int argc, char *argv[]);
abcd::Status pinLogin(int argc, char *argv[]);
abcd::Status pinLoginSetup(int argc, char *argv[]);
abcd::Status recoveryReminderSet(int argc, char *argv[]);
abcd::Status removeCategory(int argc, char *argv[]);
abcd::Status searchBitcoinSeed(int argc, char *argv[]);
abcd::Status setNickname(int argc, char *argv[]);
abcd::Status signIn(int argc, char *argv[]);
abcd::Status uploadLogs(int argc, char *argv[]);
abcd::Status walletDecrypt(int argc, char *argv[]);
abcd::Status walletEncrypt(int argc, char *argv[]);
abcd::Status walletGetAddress(int argc, char *argv[]);

// Implemented in its own file:
abcd::Status washer(int argc, char *argv[]);

#endif
