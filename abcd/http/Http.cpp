/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Http.hpp"
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <memory>
#include <mutex>

namespace abcd {

/**
 * Manages the cURL library global memory lifetime.
 */
struct HttpSingleton
{
    ~HttpSingleton();
    HttpSingleton();

    std::unique_ptr<std::mutex[]> mutexes;
    Status status;
};

// Global variables:
static HttpSingleton gSingleton;
std::string gCertPath;

static void
sslLockCallback(int mode, int n, const char *sourceFile, int sourceLine)
{
    if (mode & CRYPTO_LOCK)
        gSingleton.mutexes[n].lock();
    else
        gSingleton.mutexes[n].unlock();
}

static void
sslThreadIdCallback(CRYPTO_THREADID *id)
{
#ifdef __APPLE__
    CRYPTO_THREADID_set_pointer(id, pthread_self());
#else
    CRYPTO_THREADID_set_numeric(id, pthread_self());
#endif
}

HttpSingleton::~HttpSingleton()
{
    curl_global_cleanup();
}

HttpSingleton::HttpSingleton()
{
    // Enable SSL thread safety:
    mutexes.reset(new std::mutex[CRYPTO_num_locks()]);
    CRYPTO_set_locking_callback(sslLockCallback);
    CRYPTO_THREADID_set_callback(sslThreadIdCallback);

    // Initialize cURL:
    if (curl_global_init(CURL_GLOBAL_DEFAULT))
        status = ABC_ERROR(ABC_CC_Error, "Cannot initialize cURL");
}

Status
httpInit(const std::string &certPath)
{
    gCertPath = certPath;
    return gSingleton.status;
}

} // namespace abcd
