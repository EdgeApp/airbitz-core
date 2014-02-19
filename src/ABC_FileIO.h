/**
 * @file
 * AirBitz File I/O function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_FileIO_h
#define ABC_FileIO_h

#include <jansson.h>
#include "ABC.h"
#include "ABC_Util.h"

#define ABC_FILEIO_MAX_PATH_LENGTH 2048

#ifdef __cplusplus
extern "C" {
#endif

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

    tABC_CC ABC_FileIOSetRootDir(const char *szRootDir,
                                 tABC_Error *pError);

    tABC_CC ABC_FileIOGetRootDir(const char **pszRootDir,
                                 tABC_Error *pError);

    tABC_CC ABC_FileIOCreateFileList(tABC_FileIOList **ppFileList,
                                     const char *szDir,
                                     tABC_Error *pError);

    tABC_CC ABC_FileIOFreeFileList(tABC_FileIOList *pFileList,
                                   tABC_Error *pError);

    bool ABC_FileIOFileExist(const char *szFilename);

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

    tABC_CC ABC_FileIOReadFileObject(const char  *szFilename,
                                     json_t **ppJSON_Data,
                                     bool bMustExist,
                                     tABC_Error  *pError);

    FILE *fmemopen(void *buf,
                   size_t size,
                   const char *mode);

#ifdef __cplusplus
}
#endif

#endif
