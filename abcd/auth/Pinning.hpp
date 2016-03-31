/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * SSL certificate pinning logic.
 */

#ifndef ABC_PIN_h
#define ABC_PIN_h

#include <openssl/x509.h>

namespace abcd {

int ABC_PinCertCallback(int pok, X509_STORE_CTX *ctx);

int ABC_PinPubkeyCallback(int pok, X509_STORE_CTX *ctx);

} // namespace abcd

#endif
