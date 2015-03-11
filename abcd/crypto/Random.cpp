/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Random.hpp"
#include "../util/FileIO.hpp"
#include <openssl/rand.h>
#ifndef __ANDROID__
#include <sys/statvfs.h>
#endif
#include <sys/time.h>
#include <unistd.h>

namespace abcd {

#define UUID_BYTE_COUNT         16
#define UUID_STR_LENGTH         (UUID_BYTE_COUNT * 2) + 4

/**
 * Sets the seed for the random number generator
 */
tABC_CC ABC_CryptoSetRandomSeed(const tABC_U08Buf Seed,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFileIORootDir = NULL;
    unsigned long timeVal;
    time_t timeResult;
    clock_t clockVal;
    pid_t pid;
    std::string rootDir = getRootDir();

    AutoU08Buf NewSeed;

    ABC_CHECK_NULL_BUF(Seed);

    // create our own copy so we can add to it
    ABC_BUF_DUP(NewSeed, Seed);

    // mix in some info on our file system
#ifndef __ANDROID__
    ABC_BUF_APPEND_PTR(NewSeed, rootDir.data(), rootDir.size());
    struct statvfs fiData;
    if ((statvfs(rootDir.c_str(), &fiData)) >= 0 )
    {
        ABC_BUF_APPEND_PTR(NewSeed, (unsigned char *)&fiData, sizeof(struct statvfs));

        //printf("Disk %s: \n", szRootDir);
        //printf("\tblock size: %lu\n", fiData.f_bsize);
        //printf("\ttotal no blocks: %i\n", fiData.f_blocks);
        //printf("\tfree blocks: %i\n", fiData.f_bfree);
    }
#endif

    // add some time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timeVal = tv.tv_sec * tv.tv_usec;
    ABC_BUF_APPEND_PTR(NewSeed, &timeVal, sizeof(unsigned long));

    timeResult = time(NULL);
    ABC_BUF_APPEND_PTR(NewSeed, &timeResult, sizeof(time_t));

    clockVal = clock();
    ABC_BUF_APPEND_PTR(NewSeed, &clockVal, sizeof(clock_t));

    timeVal = CLOCKS_PER_SEC ;
    ABC_BUF_APPEND_PTR(NewSeed, &timeVal, sizeof(unsigned long));

    // add process id's
    pid = getpid();
    ABC_BUF_APPEND_PTR(NewSeed, &pid, sizeof(pid_t));

    pid = getppid();
    ABC_BUF_APPEND_PTR(NewSeed, &pid, sizeof(pid_t));

    // TODO: add more random seed data here

    // seed it
    RAND_seed(ABC_BUF_PTR(NewSeed), ABC_BUF_SIZE(NewSeed));

exit:
    ABC_FREE_STR(szFileIORootDir);

    return cc;
}

/**
 * Creates a buffer of random data
 */
tABC_CC ABC_CryptoCreateRandomData(unsigned int  length,
                                   tABC_U08Buf   *pData,
                                   tABC_Error    *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    int rc;

    ABC_CHECK_NULL(pData);

    ABC_BUF_NEW(*pData, length);

    rc = RAND_bytes(ABC_BUF_PTR(*pData), length);
    //unsigned long err = ERR_get_error();

    if (rc != 1)
    {
        ABC_BUF_FREE(*pData);
        ABC_RET_ERROR(ABC_CC_Error, "Random data generation failed");
    }

exit:

    return cc;
}

/**
 * generates a randome UUID (version 4)
 *
 * Version 4 UUIDs use a scheme relying only on random numbers.
 * This algorithm sets the version number (4 bits) as well as two reserved bits.
 * All other bits (the remaining 122 bits) are set using a random or pseudorandom data source.
 * Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx where x is any hexadecimal
 * digit and y is one of 8, 9, A, or B (e.g., F47AC10B-58CC-4372-A567-0E02B2E3D479).
 */
tABC_CC ABC_CryptoGenUUIDString(char       **pszUUID,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    unsigned char *pData = NULL;
    char *szUUID = NULL;
    AutoU08Buf Data;

    ABC_CHECK_NULL(pszUUID);

    ABC_STR_NEW(szUUID, UUID_STR_LENGTH + 1);

    ABC_CHECK_RET(ABC_CryptoCreateRandomData(UUID_BYTE_COUNT, &Data, pError));
    pData = ABC_BUF_PTR(Data);

    // put in the version
    // To put in the version, take the 7th byte and perform an and operation using 0x0f, followed by an or operation with 0x40.
    pData[6] = (pData[6] & 0xf) | 0x40;

    // 9th byte significant nibble is one of 8, 9, A, or B
    pData[8] = (pData[8] | 0x80) & 0xbf;

    sprintf(szUUID, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7],
            pData[8], pData[9], pData[10], pData[11], pData[12], pData[13], pData[14], pData[15]);

    *pszUUID = szUUID;

exit:
    return cc;
}

} // namespace abcd
