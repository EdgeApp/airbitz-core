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

std::recursive_mutex gFileMutex;

std::string
fileSlashify(const std::string &path)
{
    return path.back() == '/' ? path : path + '/';
}

Status
fileEnsureDir(const std::string &dir)
{
    AutoFileLock lock(gFileMutex);

    if (!fileExists(dir))
    {
        mode_t process_mask = umask(0);
        int e = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
        umask(process_mask);

        if (e)
            return ABC_ERROR(ABC_CC_DirReadError, "Could not create directory");
    }

    return Status();
}

bool
fileExists(const std::string &path)
{
    AutoFileLock lock(gFileMutex);

    return 0 == access(path.c_str(), F_OK);
}

bool
fileIsJson(const std::string &name)
{
    // Skip hidden files:
    if ('.' == name[0])
        return false;

    return 5 <= name.size() && std::equal(name.end() - 5, name.end(), ".json");
}

Status
fileLoad(DataChunk &result, const std::string &path)
{
    AutoFileLock lock(gFileMutex);

    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp)
        return ABC_ERROR(ABC_CC_FileOpenError, "Cannot open for reading: " + path);

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    result.resize(size);
    if (fread(result.data(), 1, size, fp) != size)
    {
        fclose(fp);
        return ABC_ERROR(ABC_CC_FileReadError, "Cannot read file: " + path);
    }

    fclose(fp);
    return Status();
}

Status
fileSave(DataSlice data, const std::string &path)
{
    AutoFileLock lock(gFileMutex);
    ABC_DebugLog("Writing file %s", path.c_str());

    FILE *fp = fopen(path.c_str(), "wb");
    if (!fp)
        return ABC_ERROR(ABC_CC_FileOpenError, "Cannot open for writing: " + path);

    if (1 != fwrite(data.data(), data.size(), 1, fp))
    {
        fclose(fp);
        return ABC_ERROR(ABC_CC_FileReadError, "Cannot write file: " + path);
    }

    fclose(fp);
    return Status();
}

static Status
fileDeleteRecursive(const std::string &path)
{
    // It would be better if we could use the POSIX `nftw` function here,
    // but that is not available on all platforms we support.

    // First, be sure the file exists:
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf))
        return Status();

    // If this is a directory, delete the contents:
    if (S_ISDIR(statbuf.st_mode))
    {
        DIR *dir = opendir(path.c_str());
        if (!dir)
            return ABC_ERROR(ABC_CC_SysError, "Cannot open directory for deletion");

        struct dirent *de;
        while (nullptr != (de = readdir(dir)))
        {
            // These two are not real entries:
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
                continue;

            Status s = fileDeleteRecursive(fileSlashify(path) + de->d_name);
            if (!s)
            {
                closedir(dir);
                return s.at(ABC_HERE());
            }
        }
        closedir(dir);
    }

    // Actually remove the thing:
    if (remove(path.c_str()))
        return ABC_ERROR(ABC_CC_SysError, "Cannot delete file");

    return Status();
}

Status
fileDelete(const std::string &path)
{
    ABC_DebugLog("Deleting %s", path.c_str());
    return fileDeleteRecursive(path);
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

