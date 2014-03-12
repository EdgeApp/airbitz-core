/**
 * @file
 * AirBitz File I/O functions.
 *
 * This file contains all of the functions associated with file functions.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <jansson.h>
#include "ABC.h"
#include "ABC_FileIO.h"
#include "ABC_Util.h"

static char             gszRootDir[ABC_MAX_STRING_LENGTH + 1] = ".";

static bool             gbInitialized = false;
static pthread_mutex_t  gMutex; // to block multiple threads from accessing files at the same time

/**
 * Initialize the FileIO system
 */
tABC_CC ABC_FileIOInitialize(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_FileIO has already been initalized");

    // create a mutex to block multiple threads from accessing files at the same time
    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_FileIO could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_FileIO could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_FileIO could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gbInitialized = true;

exit:

    return cc;
}

/**
 * Shut down the FileIO system
 */
void ABC_FileIOTerminate()
{
    if (gbInitialized == true)
    {
        pthread_mutex_destroy(&gMutex);

        gbInitialized = false;
    }
}

// sets the root directory to the string given
tABC_CC ABC_FileIOSetRootDir(const char *szRootDir,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(szRootDir);
    strncpy(gszRootDir, szRootDir, ABC_MAX_STRING_LENGTH);
    gszRootDir[ABC_MAX_STRING_LENGTH] = '\0';

exit:

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the root directory
 *
 * @param pszRootDir pointer to store allocated string
 *                   (the user is responsible for free'ing)
 */
tABC_CC ABC_FileIOGetRootDir(char **pszRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(pszRootDir);

    *pszRootDir = strdup(gszRootDir);

exit:

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// creates a filelist structure for a specified directory
tABC_CC ABC_FileIOCreateFileList(tABC_FileIOList **ppFileList,
                                 const char *szDir,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(ppFileList);
    ABC_CHECK_NULL(szDir);

    tABC_FileIOList *pFileList = NULL;
    ABC_ALLOC(pFileList, sizeof(tABC_FileIOList));

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(szDir)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (pFileList->nCount)
            {
                pFileList->apFiles = (tABC_FileIOFileInfo **)realloc(pFileList->apFiles, sizeof(tABC_FileIOFileInfo *) * (pFileList->nCount + 1));
            }
            else
            {
                ABC_ALLOC(pFileList->apFiles, sizeof(tABC_FileIOFileInfo *));
            }

            pFileList->apFiles[pFileList->nCount] = NULL;
            ABC_ALLOC(pFileList->apFiles[pFileList->nCount], sizeof(tABC_FileIOFileInfo));

            pFileList->apFiles[pFileList->nCount]->szName = strdup(ent->d_name);
            if (ent->d_type == DT_UNKNOWN)
            {
                pFileList->apFiles[pFileList->nCount]->type = ABC_FileIOFileType_Unknown;
            }
            else if (ent->d_type == DT_DIR)
            {
                pFileList->apFiles[pFileList->nCount]->type = ABC_FILEIOFileType_Directory;
            }
            else
            {
                pFileList->apFiles[pFileList->nCount]->type = ABC_FileIOFileType_Regular;
            }

            pFileList->nCount++;
        }
        closedir (dir);
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_DirReadError, "Could not read directory");
    }

    *ppFileList = pFileList;

exit:

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// frees a file list structure
void ABC_FileIOFreeFileList(tABC_FileIOList *pFileList)
{
    if (pFileList)
    {
        if (pFileList->apFiles)
        {
            for (int i = 0; i < pFileList->nCount; i++)
            {
                if (pFileList->apFiles[i])
                {
                    ABC_FREE_STR(pFileList->apFiles[i]->szName);
                    ABC_CLEAR_FREE(pFileList->apFiles[i], sizeof(tABC_FileIOFileInfo));
                }
            }
        }
        ABC_CLEAR_FREE(pFileList, sizeof(tABC_FileIOList));
    }
}

// checks if a file exists
tABC_CC ABC_FileIOFileExists(const char *szFilename,
                             bool *pbExists,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(pbExists);
    *pbExists = false;

    if (szFilename != NULL)
    {
        if (access(szFilename, F_OK) != -1 )
        {
            *pbExists = true;
        }
    }

exit:

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// creates a directory
tABC_CC ABC_FileIOCreateDir(const char *szDir,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(szDir);

    mode_t process_mask = umask(0);
    int result_code = mkdir(szDir, S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);

    if (0 != result_code)
    {
        ABC_RET_ERROR(ABC_CC_DirReadError, "Could not create directory");
    }

exit:

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// writes the given data to the specified filename
tABC_CC ABC_FileIOWriteFile(const char *szFilename,
                            tABC_U08Buf Data,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    FILE *fp = NULL;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL_BUF(Data);

    // open the file
    fp = fopen(szFilename, "wb");
    if (fp == NULL)
    {
        ABC_RET_ERROR(ABC_CC_FileOpenError, "Could not open file for writing");
    }

    // write the data
    if (fwrite(ABC_BUF_PTR(Data), 1, ABC_BUF_SIZE(Data), fp) != ABC_BUF_SIZE(Data))
    {
        ABC_RET_ERROR(ABC_CC_FileWriteError, "Could not write to file");
    }

exit:
    if (fp) fclose(fp);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// writes the given string to the specified filename
// a newline is added to the end of the file
tABC_CC ABC_FileIOWriteFileStr(const char *szFilename,
                               const char *szData,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    FILE *fp = NULL;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(szData);

    // open the file
    fp = fopen(szFilename, "wb");
    if (fp == NULL)
    {
        ABC_RET_ERROR(ABC_CC_FileOpenError, "Could not open file for writing");
    }

    // write the data
    if (fwrite(szData, 1, strlen(szData), fp) != strlen(szData))
    {
        ABC_RET_ERROR(ABC_CC_FileWriteError, "Could not write to file");
    }

    // write a newline
    if (fwrite("\n", 1, 1, fp) != 1)
    {
        ABC_RET_ERROR(ABC_CC_FileWriteError, "Could not write to file");
    }


exit:
    if (fp) fclose(fp);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// reads the given filename into a string
// the data is stored in an allocated buffer and then a '\0' is appended
tABC_CC ABC_FileIOReadFileStr(const char  *szFilename,
                              char        **pszData,
                              tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    FILE *fp = NULL;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pszData);

    // open the file
    fp = fopen(szFilename, "rb");
    if (fp == NULL)
    {
        ABC_RET_ERROR(ABC_CC_FileOpenError, "Could not open file for reading");
    }

    // get the length
    fseek(fp, 0, SEEK_END);
    long int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // create the memory
    ABC_ALLOC(*pszData, size + 1); // +1 for the '\0'

    // write the data
    if (fread(*pszData, 1, size, fp) != size)
    {
        ABC_FREE_STR(*pszData);
        ABC_RET_ERROR(ABC_CC_FileReadError, "Could not read from file");
    }

exit:
    if (fp) fclose(fp);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

// reads the given filename into a JSON object
// the JSON object must be deref'ed by the caller
// if bMustExist is false, a new empty object is created if the file doesn't exist
tABC_CC ABC_FileIOReadFileObject(const char  *szFilename,
                                 json_t **ppJSON_Data,
                                 bool bMustExist,
                                 tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szData_JSON = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError));
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(ppJSON_Data);

    // if the file exists
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    if (true == bExists)
    {
        ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szData_JSON, pError));

        // decode the json
        json_error_t error;
        pJSON_Root = json_loads(szData_JSON, 0, &error);
        ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
        ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");
    }
    else if (false == bMustExist)
    {
        // make a new one
        pJSON_Root = json_object();
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_FileDoesNotExist, "Could not find file");
    }

    *ppJSON_Data = pJSON_Root;
    pJSON_Root = NULL; // so we don't decref it

exit:
    ABC_FREE_STR(szData_JSON);
    if (pJSON_Root) json_decref(pJSON_Root);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * Deletes the specified file
 *
 */
tABC_CC ABC_FileIODeleteFile(const char *szFilename,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");

    // delete it
    if (0 != unlink(szFilename))
    {
        ABC_RET_ERROR(ABC_CC_Error, "Could not delete file");
    }

exit:

    return cc;
}

/**
 * Locks the global mutex
 *
 */
tABC_CC ABC_FileIOMutexLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_FileIO has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_FileIO error locking mutex");

exit:

    return cc;
}

/**
 * Unlocks the global mutex
 *
 */
tABC_CC ABC_FileIOMutexUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_FileIO has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_FileIO error unlocking mutex");

exit:

    return cc;
}
