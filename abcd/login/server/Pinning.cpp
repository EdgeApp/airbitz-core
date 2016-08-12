/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Pinning.hpp"
#include "../../util/Util.hpp"
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>

namespace abcd {

/* Certificate Pinning
 * Code based off the openssl example at:
 * https://www.owasp.org/index.php/Certificate_and_Public_Key_Pinning
 */

#define PIN_ASSERT(assert, err, desc) \
    { \
        if (!(assert)) \
        { \
            ok = 0; \
            ABC_LOG_ERROR(err, desc); \
            goto exit; \
        } \
    } \

const char *CA_CERTIFICATE =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDuzCCAqOgAwIBAgIJAPMXB5xlUjQSMA0GCSqGSIb3DQEBCwUAMHQxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRIwEAYDVQQHDAlTYW4gRGllZ28x\n"
    "FDASBgNVBAoMC0FpcmJpdHogSW5jMSYwJAYDVQQDDB1BaXJiaXR6IENlcnRpZmlj\n"
    "YXRlIEF1dGhvcml0eTAeFw0xNDA3MzAwMDUwNTJaFw0xNzA1MTkwMDUwNTJaMHQx\n"
    "CzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRIwEAYDVQQHDAlTYW4g\n"
    "RGllZ28xFDASBgNVBAoMC0FpcmJpdHogSW5jMSYwJAYDVQQDDB1BaXJiaXR6IENl\n"
    "cnRpZmljYXRlIEF1dGhvcml0eTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC\n"
    "ggEBAKpSTCS4GAaTmBz1HBLZVwSBQ4M3Y0czgH8jbweGyitqFOhhA/yro2t2bgXY\n"
    "NsNZneM/nDwXcjiosU5ZRoupgf2kRNpfeTjfZtDkBtCE7BPxlFZBo6tZDxCZJlTQ\n"
    "BzBCzPOsukseEYZYGgW1MAKOUzLWg5NNXObr2iDZeA81hnjiGa/a1aPzekeahndC\n"
    "dGlQG6ytfpU/75ucN7f3GRWUHMTHkptj9VHRyZQl+p4Ju39e+pt9wZMpEGXABtDm\n"
    "8BSTSKLBH875pegwenE6rEsTvyKz4F62H9KPc9hPGzestz7eS00L99dFKtw9BYq9\n"
    "xro6VRwTULvaIAMaDvuxfydSejcCAwEAAaNQME4wHQYDVR0OBBYEFBOiP5bbSlRX\n"
    "DkltoA+CHDp1m0rIMB8GA1UdIwQYMBaAFBOiP5bbSlRXDkltoA+CHDp1m0rIMAwG\n"
    "A1UdEwQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAFHRv1yPh2ORlqe57zvGT6wx\n"
    "OtAeYnu1rvo+4k7V8zkVCb9A3tEboDeC0h71/S+4Cq2Vr6h6QtMFmfNNFbVMIro6\n"
    "FzeDJ27xyLcMqIY6x1GQiBBzzMhDDdK4MotNNrc/McPt4be8I1b1wVdmDEvonfEj\n"
    "UMKK8XHiwIVZJIKlyCMNWDvlRhdgenfocZJQmwwrfpTdMOdP/kaDRUNQcLGsU+wz\n"
    "TtGMn/1UeGxijct0sQpQ9PCHRc1+8kETDTMKAB/F1zBUvMCivtMYZ+j3bnq7llVh\n"
    "FRphU1/lkdwUh7+d9balfXUHn9Jk7T67mhwvJUDo7FY6FScsZ4wZB2HPmbjhdGw=\n"
    "-----END CERTIFICATE-----\n";

// Deprecated...we need to support public key pinning rather than crt pinning
const char *OLD_AUTH_CERTIFICATE =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDWDCCAkACCQDakf2Qe9pwfDANBgkqhkiG9w0BAQsFADB0MQswCQYDVQQGEwJV\n"
    "UzETMBEGA1UECAwKQ2FsaWZvcm5pYTESMBAGA1UEBwwJU2FuIERpZWdvMRQwEgYD\n"
    "VQQKDAtBaXJiaXR6IEluYzEmMCQGA1UEAwwdQWlyYml0eiBDZXJ0aWZpY2F0ZSBB\n"
    "dXRob3JpdHkwHhcNMTQwOTEwMTUzMTIwWhcNMTYwMTIzMTUzMTIwWjBoMQswCQYD\n"
    "VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTESMBAGA1UEBwwJU2FuIERpZWdv\n"
    "MRQwEgYDVQQKDAtBaXJiaXR6IEluYzEaMBgGA1UEAwwRKi5hdXRoLmFpcmJpdHou\n"
    "Y28wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDdr5sdJZF5pOzEOPr/\n"
    "kE26UFaUVEMEFai2zu2xtrRfu56S9jfoKrA5Sqt+PeA5TJLJNEO+FC2zsb5YXyq7\n"
    "RVJ5MkZQb/K+m+tFk9Gjh9fC2yrNh96K+LjpyNkSUCCynV/Hjn0UK8GcCA4m+rG4\n"
    "gStAAOcuz1AcTrprmywj8pgy7XDTkjBHWom5lyeMG6roP5rWy8xQXpGnbKDKaahJ\n"
    "uSiYRgWZUN1F4sy+ZQcGqNUxJ35l46w5k+tCGlb9ow7wx8rJyJrpsA7UgZ331vAK\n"
    "SidRS0MxhpnELi0z2KvbuBDuUTYDQNroy5evii8XqIu8agxQmBt2ie2p+wnNZNfP\n"
    "h+FXAgMBAAEwDQYJKoZIhvcNAQELBQADggEBABRd3m6ZhutEt/FzLlQHFHX+Wo0Y\n"
    "ny7YEXzTWkK2gTOScDJ8Ej6ukJzRgGCeTon1QRuzDxnx6EUx6hJUkuIQmv+6X+26\n"
    "KzBkAIEC9el0mR/NEaCrc4TYeiaDs00DVoq928cjXHIEXRX/Rbi7pEEiFLZAXW/U\n"
    "x+9J64cv+9aLZ01iljYhdMm5Kj0v7l5RrzG8FmjamayoqPQh7O498SQOQCYtmqEX\n"
    "3u0tuFme7mX8bMWfMXiaLyxf+Ra6Ynl/I8GzFAy4aOz8m9guY33012V/gC0i7/d9\n"
    "AEhCYWQ4tLZOTiJI3YTG9i5jhbzfwWVVLS8g3LXfyq71V3AzjAb6amhUZ4Y=\n"
    "-----END CERTIFICATE-----\n";

const char *AUTH_CERTIFICATE =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDWDCCAkACCQDakf2Qe9pwgDANBgkqhkiG9w0BAQsFADB0MQswCQYDVQQGEwJV\n"
    "UzETMBEGA1UECAwKQ2FsaWZvcm5pYTESMBAGA1UEBwwJU2FuIERpZWdvMRQwEgYD\n"
    "VQQKDAtBaXJiaXR6IEluYzEmMCQGA1UEAwwdQWlyYml0eiBDZXJ0aWZpY2F0ZSBB\n"
    "dXRob3JpdHkwHhcNMTUxMjE3MjMzNDA1WhcNMTkxMjE2MjMzNDA1WjBoMQswCQYD\n"
    "VQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTESMBAGA1UEBxMJU2FuIERpZWdv\n"
    "MRQwEgYDVQQKEwtBaXJiaXR6IEluYzEaMBgGA1UEAxQRKi5hdXRoLmFpcmJpdHou\n"
    "Y28wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDdr5sdJZF5pOzEOPr/\n"
    "kE26UFaUVEMEFai2zu2xtrRfu56S9jfoKrA5Sqt+PeA5TJLJNEO+FC2zsb5YXyq7\n"
    "RVJ5MkZQb/K+m+tFk9Gjh9fC2yrNh96K+LjpyNkSUCCynV/Hjn0UK8GcCA4m+rG4\n"
    "gStAAOcuz1AcTrprmywj8pgy7XDTkjBHWom5lyeMG6roP5rWy8xQXpGnbKDKaahJ\n"
    "uSiYRgWZUN1F4sy+ZQcGqNUxJ35l46w5k+tCGlb9ow7wx8rJyJrpsA7UgZ331vAK\n"
    "SidRS0MxhpnELi0z2KvbuBDuUTYDQNroy5evii8XqIu8agxQmBt2ie2p+wnNZNfP\n"
    "h+FXAgMBAAEwDQYJKoZIhvcNAQELBQADggEBAA4CFHl47lc0hrCY7qp1oFpCByYF\n"
    "WathfwzH6v+2orirS+lP2MB36ZnEupThvfW82xJm/YhqBXHeHoyjmfFt/AZ6Qda2\n"
    "d4nJ2vMM8PA6IZlH+IB4Qpmo9S3c1SqDCMaKR1LTUxn5F1DWuXePOEZoUFGWTZUG\n"
    "rmWE+pyNMJ9v3/0VUxr/lW15mco7CK3pWpL6fvWps5R8iKVfOuXd0uaD8QV0y7tc\n"
    "wpqjghHH0LL06f7aT1u7J2NQmHsXFoUrNacEWxNy1eH1RmbVbR2GkTcSRghR88k/\n"
    "SyfPC5XvSS5TSO5pIVaCUAbI5veBop1460axqRKH8PQQFZqj9PQppJmWcCY=\n"
    "-----END CERTIFICATE-----\n";

int ABC_PinCertCallback(int pok, X509_STORE_CTX *ctx)
{
    int ok = pok;

    X509 *cert = NULL;
    BIO *b64 = NULL;
    BUF_MEM *bptr = NULL;
    char *szCert = NULL;

    PIN_ASSERT((cert = ctx->current_cert) != NULL,
               ABC_CC_Error, "Unable to retrieve certificate");
    PIN_ASSERT((b64 = BIO_new(BIO_s_mem())) != NULL,
               ABC_CC_Error, "Unable to alloc BIO");
    PIN_ASSERT(1 == PEM_write_bio_X509(b64, cert),
               ABC_CC_Error, "Unable to write bio");

    BIO_get_mem_ptr(b64, &bptr);

    PIN_ASSERT(NULL != (szCert = (char *)malloc(bptr->length + 1)),
               ABC_CC_Error, "Unable to malloc");
    PIN_ASSERT(0 < BIO_read(b64, szCert, bptr->length),
               ABC_CC_Error, "Unable to read into bio to char *");

    PIN_ASSERT(strncmp(szCert, OLD_AUTH_CERTIFICATE,
                       strlen(OLD_AUTH_CERTIFICATE)) == 0
               || strncmp(szCert, AUTH_CERTIFICATE, strlen(AUTH_CERTIFICATE)) == 0
               || strncmp(szCert, CA_CERTIFICATE, strlen(CA_CERTIFICATE)) == 0,
               ABC_CC_Error, "Pinned certificate mismatch");
exit:
    ABC_FREE(szCert);
    if (b64)
        BIO_free(b64);
    return ok;
}

} // namespace abcd
