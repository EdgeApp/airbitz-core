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
    "MIIERTCCAy2gAwIBAgIJAJMqGdkJp/u+MA0GCSqGSIb3DQEBBQUAMHQxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRIwEAYDVQQHEwlTYW4gRGllZ28x\n"
    "FDASBgNVBAoTC0FpcmJpdHogSW5jMSYwJAYDVQQDEx1BaXJiaXR6IENlcnRpZmlj\n"
    "YXRlIEF1dGhvcml0eTAeFw0xNzA1MTkwMzM0MTJaFw0yNzA1MTcwMzM0MTJaMHQx\n"
    "CzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRIwEAYDVQQHEwlTYW4g\n"
    "RGllZ28xFDASBgNVBAoTC0FpcmJpdHogSW5jMSYwJAYDVQQDEx1BaXJiaXR6IENl\n"
    "cnRpZmljYXRlIEF1dGhvcml0eTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC\n"
    "ggEBAONr1weXsjrqhQ6w9Dk1J/ASQdjasSiufgeFTTWIo52sYu6dstQisUCW6/V3\n"
    "hu33ajfXrJSb+EGC2cHk+uQllNq4bA8DgFLm1Dv1tEABK64497lAo3L764q8SJXp\n"
    "EqXzkItXuPn0Hzev94nxG6flgIbomvDLUBCptsIoBmjFUzuRVLXbPkL6W3cs5ZVj\n"
    "Pchk+2bw2nRko/F4EDkGdFstn5MbjSfW/g5hXJ5D6mJPqCkb2cBKl2av35rKA6Mo\n"
    "TREC0Ypv00umSXd7s7T547WT4BAQ43qimiaBm47jbWAnsOPpPfgSDSLsOCD4mf6v\n"
    "QhP5O83fsNcgCezTZ6uv/+JJiAMCAwEAAaOB2TCB1jAdBgNVHQ4EFgQUjVTfJnh4\n"
    "ZcEWgt+ovyvrLihpaVMwgaYGA1UdIwSBnjCBm4AUjVTfJnh4ZcEWgt+ovyvrLihp\n"
    "aVOheKR2MHQxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRIwEAYD\n"
    "VQQHEwlTYW4gRGllZ28xFDASBgNVBAoTC0FpcmJpdHogSW5jMSYwJAYDVQQDEx1B\n"
    "aXJiaXR6IENlcnRpZmljYXRlIEF1dGhvcml0eYIJAJMqGdkJp/u+MAwGA1UdEwQF\n"
    "MAMBAf8wDQYJKoZIhvcNAQEFBQADggEBAJ2/xdnVUhJ+dawVHyPl+x/Nk5uMmo3I\n"
    "eEuGtkY+OCy/ugXxVGM0TFSR26e0ujl+e2c/XsBCrQSp3jlHE5WRBeb77HAmOptK\n"
    "T3Ad/gdXseQSBMcCrWCsobeM5WPzmdSqP/ywRrkMS9O5NXJhg/8y1XvK8pPcGj9r\n"
    "K/8ktj9T6BiqygO/2APB6UFkdYZKg/noQc+A7t5LZaAW5g90jaB3ezJk/ifwB7Vk\n"
    "YyjfbWOSmzoaBloNSFF1kLKN89yUTBL2uMjXzK2dUjWic49AMVKNn4GSBF9U1Yhw\n"
    "TUyfbZKtniN8XH7a2hH0XwBs8sYS9AuD2javsufeqLP+xK/yAft6wm8=\n"
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
    "MIIDuDCCAqCgAwIBAgIJANqR/ZB72nCDMA0GCSqGSIb3DQEBCwUAMHQxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRIwEAYDVQQHEwlTYW4gRGllZ28x\n"
    "FDASBgNVBAoTC0FpcmJpdHogSW5jMSYwJAYDVQQDEx1BaXJiaXR6IENlcnRpZmlj\n"
    "YXRlIEF1dGhvcml0eTAeFw0xNzA1MTkwNDE4MTJaFw0yNzA1MTcwNDE4MTJaMGgx\n"
    "CzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRIwEAYDVQQHDAlTYW4g\n"
    "RGllZ28xFDASBgNVBAoMC0FpcmJpdHogSW5jMRowGAYDVQQDDBEqLmF1dGguYWly\n"
    "Yml0ei5jbzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAN2vmx0lkXmk\n"
    "7MQ4+v+QTbpQVpRUQwQVqLbO7bG2tF+7npL2N+gqsDlKq3494DlMksk0Q74ULbOx\n"
    "vlhfKrtFUnkyRlBv8r6b60WT0aOH18LbKs2H3or4uOnI2RJQILKdX8eOfRQrwZwI\n"
    "Dib6sbiBK0AA5y7PUBxOumubLCPymDLtcNOSMEdaibmXJ4wbqug/mtbLzFBekads\n"
    "oMppqEm5KJhGBZlQ3UXizL5lBwao1TEnfmXjrDmT60IaVv2jDvDHysnImumwDtSB\n"
    "nffW8ApKJ1FLQzGGmcQuLTPYq9u4EO5RNgNA2ujLl6+KLxeoi7xqDFCYG3aJ7an7\n"
    "Cc1k18+H4VcCAwEAAaNZMFcwHwYDVR0jBBgwFoAUjVTfJnh4ZcEWgt+ovyvrLihp\n"
    "aVMwCQYDVR0TBAIwADALBgNVHQ8EBAMCBPAwHAYDVR0RBBUwE4IRKi5hdXRoLmFp\n"
    "cmJpdHouY28wDQYJKoZIhvcNAQELBQADggEBAMHc4HpbH2zUt+CuFuKww3z64hjR\n"
    "Th2KGZDZ2CdxNfxZMa5RhMShS3QVmD985V5IXGaDqQSD/K96IEbmnegrojp30w80\n"
    "Wt4tBSGce29IALlLlBOx5iYUHDZxv58HVNeZ10EIL78FhLfUzSessWfxolgZ5Kom\n"
    "l5eoWrgBIYmI/djUzO8dPdEeCraarZklY7zo1/5wCrgFkN8rFRxYxlGDsRjwiXtK\n"
    "jujUQ61+sCTxj2fu3J/Ga//J01qRspiheXOvWXmiXnSIZ13QAwvRidIF71z4U0b7\n"
    "4f6GsjFl+8n0XPp0N+pu58Obujm9BBN9lpocg+lWSPaAAxvU4B4i2DkhcRs=\n"
    "-----END CERTIFICATE-----\n";

int ABC_PinCertCallback(int pok, X509_STORE_CTX *ctx)
{
    int ok = pok;

    X509 *cert = NULL;
    BIO *b64 = NULL;
    BUF_MEM *bptr = NULL;
    char *szCert = NULL;

//    goto exit; // Disable pinning for the test server
//
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
