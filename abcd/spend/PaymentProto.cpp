/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "PaymentProto.hpp"
#include "../bitcoin/Testnet.hpp"
#include "../http/HttpRequest.hpp"
#include "../util/AutoFree.hpp"

#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <time.h>

namespace abcd {

#define SSL_ERROR(code, ssl_ctx) \
    ABC_ERROR(code, X509_verify_cert_error_string( \
        X509_STORE_CTX_get_error(ssl_ctx)));

const char *USER_AGENT = "Airbitz";
const char *BIP71_MIMETYPE_PAYMENT = "application/bitcoin-payment";
const char *BIP71_MIMETYPE_PAYMENTACK = "application/bitcoin-paymentack";
const char *BIP71_MIMETYPE_PAYMENTREQUEST = "application/bitcoin-paymentrequest";

class AutoX509:
    public std::vector<X509 *>
{
public:
    ~AutoX509()
    {
        for (unsigned int i = 0; i < this->size(); ++i)
            X509_free(this->at(i));
    }
};

static std::string gCertPath;

static bool
loadCerts(payments::X509Certificates certChain, AutoX509 &certs)
{
    for (int i = 0; i < certChain.certificate_size(); i++)
    {
        const unsigned char *data =
            (const unsigned char *) certChain.certificate(i).data();
        X509 *cert = d2i_X509(NULL, &data, certChain.certificate(i).size());
        if (cert)
            certs.push_back(cert);
    }
    return certs.size() > 0;
}

static bool
isValidSignature(X509 *cert, const EVP_MD* alg, payments::PaymentRequest req)
{
    payments::PaymentRequest rcopy(req);
    rcopy.set_signature(std::string(""));
    std::string data;
    rcopy.SerializeToString(&data);

    EVP_MD_CTX ctx;
    EVP_PKEY *pubkey = X509_get_pubkey(cert);
    EVP_MD_CTX_init(&ctx);
    if (!EVP_VerifyInit_ex(&ctx, alg, NULL))
        return false;
    if (!EVP_VerifyUpdate(&ctx, data.data(), data.size()))
        return false;
    if (!EVP_VerifyFinal(&ctx,
            (const unsigned char*) req.signature().data(),
            (unsigned int) req.signature().size(), pubkey))
        return false;
    return true;
}

Status
paymentInit(const std::string &certPath)
{
    gCertPath = certPath;
    return Status();
}

Status
PaymentRequest::fetch(const std::string &url)
{
    HttpReply reply;

    ABC_CHECK(HttpRequest()
        .header("Accept", BIP71_MIMETYPE_PAYMENTREQUEST)
        .header("User-Agent", USER_AGENT)
        .get(reply, url));
    ABC_CHECK(reply.codeOk());

    if (!request_.ParseFromString(reply.body))
        return ABC_ERROR(ABC_CC_Error, "Failed to parse PaymentRequest");

    if (!details_.ParseFromString(request_.serialized_payment_details()))
        return ABC_ERROR(ABC_CC_Error, "Failed to parse details");

    // Are we on the right network?
    if ((isTestnet() && "test" != details_.network())
            || (!isTestnet() && "main" != details_.network()))
        return ABC_ERROR(ABC_CC_Error, "Unsupported network");

    if (details_.has_memo())
        memo_ = details_.memo();

    return Status();
}

Status
PaymentRequest::signatureOk()
{
    const EVP_MD* alg = NULL;
    if (request_.pki_type() == "x509+sha256")
        alg = EVP_sha256();
    else if (request_.pki_type() == "x509+sha1")
        alg = EVP_sha1();
    else if (request_.pki_type() == "none")
        return ABC_ERROR(ABC_CC_Error, "Pki_type == none");
    else
        return ABC_ERROR(ABC_CC_Error, "Unknown pki_type");

    payments::X509Certificates certChain;
    if (!certChain.ParseFromString(request_.pki_data()))
        return ABC_ERROR(ABC_CC_Error, "Error parsing pki_data");

    AutoX509 certs;
    if (!loadCerts(certChain, certs))
        return ABC_ERROR(ABC_CC_Error, "Error loading certs");

    // The first cert is the signing cert,
    // the rest are untrusted certs that chain
    // to a valid root authority. OpenSSL needs them separately.
    STACK_OF(X509) *chain = sk_X509_new_null();
    for (int i = certs.size() - 1; i > 0; i--) {
        sk_X509_push(chain, certs[i]);
    }
    X509 *signing_cert = certs[0];

    AutoFree<X509_STORE_CTX, X509_STORE_CTX_free> store_ctx(X509_STORE_CTX_new());
    if (!store_ctx.get())
        return ABC_ERROR(ABC_CC_Error, "Error creating X509_STORE_CTX");

    AutoFree<SSL_CTX, SSL_CTX_free> sslContext(SSL_CTX_new(SSLv23_client_method()));
    if (!SSL_CTX_load_verify_locations(sslContext.get(), gCertPath.c_str(), NULL))
        return ABC_ERROR(ABC_CC_Error, "Unable to load caCerts");

    if (!X509_STORE_CTX_init(store_ctx.get(),
            SSL_CTX_get_cert_store(sslContext.get()), signing_cert, chain))
        return SSL_ERROR(ABC_CC_Error, store_ctx.get());

    if (1 != X509_verify_cert(store_ctx.get()))
        return SSL_ERROR(ABC_CC_Error, store_ctx.get())

    if (!isValidSignature(signing_cert, alg, request_))
        return ABC_ERROR(ABC_CC_Error, "Bad signature");

    X509_NAME *certname = X509_get_subject_name(signing_cert);
    int textlen = X509_NAME_get_text_by_NID(certname, NID_commonName, NULL, 0);
    std::vector<char> website(textlen + 1);
    if (X509_NAME_get_text_by_NID(certname, NID_commonName,
        website.data(), website.size()) != textlen && 0 < textlen)
        return ABC_ERROR(ABC_CC_Error, "Missing common name");

    merchant_ = website.data();

    return Status();
}

std::list<PaymentOutput>
PaymentRequest::outputs() const
{
    std::list<PaymentOutput> out;
    for (auto &i: details_.outputs())
    {
        PaymentOutput o =
        {
            i.amount(),
            DataSlice(i.script())
        };
        out.push_back(o);
    }
    return out;
}

uint64_t
PaymentRequest::amount() const
{
    uint64_t out = 0;
    for (auto &i: details_.outputs())
        out += i.amount();
    return out;
}

Status
PaymentRequest::pay(PaymentReceipt &result, DataSlice tx, DataSlice refund)
{
    payments::Payment payment;
    payment.set_merchant_data(details_.merchant_data());
    payment.add_transactions(tx.data(), tx.size());

    // Added refund address
    payments::Output* refund_to = payment.add_refund_to();
    refund_to->set_script(refund.data(), refund.size());

    std::string response;
    payment.SerializeToString(&response);

    // Check request expiration
    time_t now = time(NULL);
    if (details_.has_expires() && (int64_t) details_.expires() < now)
        return ABC_ERROR(ABC_CC_Error, "Payment request has expired");

    HttpReply reply;
    ABC_CHECK(HttpRequest()
        .header("Accept", BIP71_MIMETYPE_PAYMENTACK)
        .header("Content-Type", BIP71_MIMETYPE_PAYMENT)
        .header("User-Agent", USER_AGENT)
        .post(reply, details_.payment_url(), response));
    ABC_CHECK(reply.codeOk());

    if (!result.ack.ParseFromString(reply.body))
        return ABC_ERROR(ABC_CC_Error, "Failed to parse PaymentAck");

    return Status();
}

} // namespace abcd
