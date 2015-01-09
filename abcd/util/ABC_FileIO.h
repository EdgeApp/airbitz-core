/**
 * @file
 * AirBitz File I/O function prototypes
 *
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
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_FileIO_h
#define ABC_FileIO_h

#include <jansson.h>
#include <time.h>
#include "ABC.h"
#include "ABC_Util.h"

namespace abcd {

#define ABC_FILEIO_MAX_PATH_LENGTH 2048

    typedef enum eABC_FileIOFileType
    {
        ABC_FileIOFileType_Unknown,
        ABC_FileIOFileType_Regular,
        ABC_FILEIOFileType_Directory
    } tABC_FileIOFileType;

    typedef struct sABC_FileIOFileInfo
    {
        tABC_FileIOFileType type;
        char *szName;
    } tABC_FileIOFileInfo;

    typedef struct sABC_FileIOFileList
    {
        int nCount;
        tABC_FileIOFileInfo **apFiles;
    } tABC_FileIOList;

    tABC_CC ABC_FileIOInitialize(tABC_Error *pError);

    void ABC_FileIOTerminate();

    tABC_CC ABC_FileIOSetRootDir(const char *szRootDir,
                                 tABC_Error *pError);

    tABC_CC ABC_FileIOGetRootDir(char **pszRootDir,
                                 tABC_Error *pError);

    tABC_CC ABC_FileIOCreateFileList(tABC_FileIOList **ppFileList,
                                     const char *szDir,
                                     tABC_Error *pError);

    void ABC_FileIOFreeFileList(tABC_FileIOList *pFileList);

    tABC_CC ABC_FileIOFileExists(const char *szFilename,
                                 bool *pbExists,
                                 tABC_Error *pError);

    tABC_CC ABC_FileIOCreateDir(const char *szDir,
                                tABC_Error *pError);

    tABC_CC ABC_FileIOWriteFile(const char *szFilename,
                                tABC_U08Buf Data,
                                tABC_Error *pError);

    tABC_CC ABC_FileIOWriteFileStr(const char *szFilename,
                                   const char *szData,
                                   tABC_Error *pError);

    tABC_CC ABC_FileIOReadFileStr(const char  *szFilename,
                                  char        **pszData,
                                  tABC_Error  *pError);

    tABC_CC ABC_FileIOReadFile(const char  *szFilename,
                               void        **pszData,
                               size_t      *nSize,
                               tABC_Error  *pError);

    tABC_CC ABC_FileIOReadFileObject(const char  *szFilename,
                                     json_t **ppJSON_Data,
                                     bool bMustExist,
                                     tABC_Error  *pError);

    tABC_CC ABC_FileIODeleteFile(const char *szFilename,
                                 tABC_Error *pError);

    tABC_CC ABC_FileIODeleteRecursive(const char *szFilename,
                                      tABC_Error *pError);

    tABC_CC ABC_FileIOFileModTime(const char *szFilename,
                                  time_t *pTime,
                                  tABC_Error *pError);

    tABC_CC ABC_FileIOMutexLock(tABC_Error *pError);

    tABC_CC ABC_FileIOMutexUnlock(tABC_Error *pError);


} // namespace abcd

#endif
