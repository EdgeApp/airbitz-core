/**
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */

#include "FileIO.hpp"
#include "Util.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <jansson.h>

namespace abcd {

static std::string gRootDir = ".";
std::recursive_mutex gFileMutex;

void
setRootDir(const std::string &rootDir)
{
    AutoFileLock lock(gFileMutex);

    gRootDir = rootDir;
    if (gRootDir.back() != '/')
        gRootDir += '/';
}

const std::string &
getRootDir()
{
    AutoFileLock lock(gFileMutex);
    return gRootDir;
}

Status
fileEnsureDir(const std::string &dir)
{
    AutoFileLock lock(gFileMutex);

    bool exists;
    ABC_CHECK_OLD(ABC_FileIOFileExists(dir.c_str(), &exists, &error));
    if (!exists)
    {
        mode_t process_mask = umask(0);
        int e = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
        umask(process_mask);

        if (e)
            return ABC_ERROR(ABC_CC_DirReadError, "Could not create directory");
    }

    return Status();
}

/**
 * Creates a filelist structure for a specified directory
 */
tABC_CC ABC_FileIOCreateFileList(tABC_FileIOList **ppFileList,
                                 const char *szDir,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoFileLock lock(gFileMutex);

    tABC_FileIOList *pFileList = NULL;

    ABC_CHECK_NULL(ppFileList);
    ABC_CHECK_NULL(szDir);

    ABC_NEW(pFileList, tABC_FileIOList);

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(szDir)) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (pFileList->nCount)
            {
                ABC_ARRAY_RESIZE(pFileList->apFiles, pFileList->nCount + 1, tABC_FileIOFileInfo*);
            }
            else
            {
                ABC_ARRAY_NEW(pFileList->apFiles, 1, tABC_FileIOFileInfo*);
            }

            pFileList->apFiles[pFileList->nCount] = NULL;
            ABC_NEW(pFileList->apFiles[pFileList->nCount], tABC_FileIOFileInfo);

            ABC_STRDUP(pFileList->apFiles[pFileList->nCount]->szName, ent->d_name);
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
    return cc;
}

/**
 * Frees a file list structure
 */
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
            ABC_FREE(pFileList->apFiles);
        }
        ABC_CLEAR_FREE(pFileList, sizeof(tABC_FileIOList));
    }
}

/**
 * Checks if a file exists
 */
tABC_CC ABC_FileIOFileExists(const char *szFilename,
                             bool *pbExists,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoFileLock lock(gFileMutex);

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
    return cc;
}

Status
fileLoad(DataChunk &result, const std::string &filename)
{
    AutoFileLock lock(gFileMutex);

    FILE *fp = fopen(filename.c_str(), "rb");
    if (!fp)
        return ABC_ERROR(ABC_CC_FileOpenError, "Cannot open for reading: " + filename);

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    result.resize(size);
    if (fread(result.data(), 1, size, fp) != size)
    {
        fclose(fp);
        return ABC_ERROR(ABC_CC_FileReadError, "Cannot read file: " + filename);
    }

    fclose(fp);
    return Status();
}

Status
fileSave(DataSlice data, const std::string &filename)
{
    AutoFileLock lock(gFileMutex);

    FILE *fp = fopen(filename.c_str(), "wb");
    if (!fp)
        return ABC_ERROR(ABC_CC_FileOpenError, "Cannot open for writing: " + filename);

    if (1 != fwrite(data.data(), data.size(), 1, fp))
    {
        fclose(fp);
        return ABC_ERROR(ABC_CC_FileReadError, "Cannot write file: " + filename);
    }

    fclose(fp);
    return Status();
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
 * Recursively deletes a directory or a file.
 */
tABC_CC ABC_FileIODeleteRecursive(const char *szFilename, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    DIR *pDir = NULL;
    char *szPath = NULL;

    // First, be sure the file exists:
    struct stat statbuf;
    if (!stat(szFilename, &statbuf))
    {
        // If this is a directory, delete the contents:
        if (S_ISDIR(statbuf.st_mode))
        {
            struct dirent *pEntry = NULL;
            size_t baseSize = strlen(szFilename);
            size_t pathSize;

            pDir = opendir(szFilename);
            ABC_CHECK_SYS(pDir, "opendir");

            pEntry = readdir(pDir);
            while (pEntry)
            {
                // These two are not real entries:
                if (strcmp(pEntry->d_name, ".") && strcmp(pEntry->d_name, ".."))
                {
                    // Delete the entry:
                    pathSize = baseSize + strlen(pEntry->d_name) + 2;
                    ABC_STR_NEW(szPath, pathSize);
                    snprintf(szPath, pathSize, "%s/%s", szFilename, pEntry->d_name);

                    ABC_CHECK_RET(ABC_FileIODeleteRecursive(szPath, pError));

                    ABC_FREE(szPath);
                }
                pEntry = readdir(pDir);
            }

            closedir(pDir);
            pDir = NULL;
        }

        // Actually remove the thing:
        ABC_CHECK_SYS(!remove(szFilename), "remove");
    }

exit:
    if (pDir) closedir(pDir);
    ABC_FREE(szPath);

    return cc;
}

/**
 * Finds the time the file was last modified
 *
 * @param pTime Location to store mode time measured in
 *              seconds since 00:00:00 UTC, Jan. 1, 1970
 *
 */
tABC_CC ABC_FileIOFileModTime(const char *szFilename,
                              time_t *pTime,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    struct stat statInfo;

    ABC_CHECK_NULL(pTime);
    *pTime = 0;
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");

    // get stats on file
    if (0 != stat(szFilename, &statInfo))
    {
        ABC_RET_ERROR(ABC_CC_Error, "Could not stat file");
    }

    // assign the time
    *pTime = statInfo.st_mtime;

exit:
    return cc;
}

} // namespace abcd

