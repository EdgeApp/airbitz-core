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


static char gszRootDir[ABC_MAX_STRING_LENGTH + 1] = ".";

// sets the root directory to the string given
tABC_CC ABC_FileIOSetRootDir(const char *szRootDir,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szRootDir);
    strncpy(gszRootDir, szRootDir, ABC_MAX_STRING_LENGTH);
    gszRootDir[ABC_MAX_STRING_LENGTH] = '\0';
    
#if 0
    printf("Root dir: %s\n", gszRootDir);

    tABC_FileIOList *pFileList;
    ABC_FileIOCreateFileList(&pFileList, gszRootDir, NULL);
    for (int i = 0; i < pFileList->nCount; i++)
    {
        printf("%s", pFileList->apFiles[i]->szName);
        if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
        {
            printf(" (file)\n");
        }
        else if (pFileList->apFiles[i]->type == ABC_FILEIOFileType_Directory)
        {
            printf(" (directory)\n");
        }
        else
        {
            printf(" (unknown)\n");
        }
    }
    ABC_FileIOFreeFileList(pFileList, NULL);
#endif
    
exit:

    return cc;
}

// sets the given pointer to point to the c string of the root directory
tABC_CC ABC_FileIOGetRootDir(const char **pszRootDir,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pszRootDir);
    
    *pszRootDir = gszRootDir;
    
exit:

    return cc;
}

// creates a filelist structure for a specified directory
tABC_CC ABC_FileIOCreateFileList(tABC_FileIOList **ppFileList,
                                 const char *szDir,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppFileList);
    ABC_CHECK_NULL(szDir);
    
    tABC_FileIOList *pFileList = (tABC_FileIOList *) calloc(1, sizeof(tABC_FileIOList));
    
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
                pFileList->apFiles = (tABC_FileIOFileInfo **)malloc(sizeof(tABC_FileIOFileInfo *));
            }
            
            pFileList->apFiles[pFileList->nCount] = (tABC_FileIOFileInfo *) calloc(1, sizeof(tABC_FileIOFileInfo));
            
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

    return cc;
}

// frees a file list structure
tABC_CC ABC_FileIOFreeFileList(tABC_FileIOList *pFileList,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pFileList);
    
    if (pFileList->apFiles)
    {
        for (int i = 0; i < pFileList->nCount; i++)
        {
            if (pFileList->apFiles[i])
            {
                if (pFileList->apFiles[i]->szName)
                {
                    free(pFileList->apFiles[i]->szName);
                }
                free(pFileList->apFiles[i]);
            }
        }
    }

    
    free(pFileList);
    
exit:

    return cc;
}

// checks if a file exists
bool ABC_FileIOFileExist(const char *szFilename)
{
    bool bExists = false;
    
    if (szFilename != NULL)
    {
        if (access(szFilename, F_OK) != -1 )
        {
            bExists = true;
        }
    }
    
    return bExists;
}

// creates a directory
tABC_CC ABC_FileIOCreateDir(const char *szDir,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szDir);

    mode_t process_mask = umask(0);
    int result_code = mkdir(szDir, S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);
    
    if (0 != result_code)
    {
        ABC_RET_ERROR(ABC_CC_DirReadError, "Could not create directory");
    }
    
exit:

    return cc;
}

// writes the given data to the specified filename
tABC_CC ABC_FileIOWriteFile(const char *szFilename,
                            tABC_U08Buf Data,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    FILE *fp = NULL;

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
    *pszData = calloc(1, size + 1); // +1 for the '\0'
    
    // write the data
    if (fread(*pszData, 1, size, fp) != size)
    {
        free(pszData);
        ABC_RET_ERROR(ABC_CC_FileReadError, "Could not read from file");
    }
    
exit:
    if (fp) fclose(fp);

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
    
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(ppJSON_Data);
    
    // if the file exists
    if (true == ABC_FileIOFileExist(szFilename))
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
    if (szData_JSON) free(szData_JSON);
    if (pJSON_Root) json_decref(pJSON_Root);
    
    return cc;
}


// **************************************************************************************
// FMEMOPEN - this is here because iOS does not have an fmemopen and some openssl 
//            functions need to use a FILE * so this is here to use memory for FILE *
// **************************************************************************************

struct fmem {
  size_t pos;
  size_t size;
  char *buffer;
};
typedef struct fmem fmem_t;

static int readfn(void *handler, char *buf, int size) {
  fmem_t *mem = handler;
  size_t available = mem->size - mem->pos;
  
  if (size > available) {
    size = (int) available;
  }
  memcpy(buf, mem->buffer + mem->pos, sizeof(char) * size);
  mem->pos += size;
  
  return size;
}

static int writefn(void *handler, const char *buf, int size) {
  fmem_t *mem = handler;
  size_t available = mem->size - mem->pos;

  if (size > available) {
    size = (int) available;
  }
  memcpy(mem->buffer + mem->pos, buf, sizeof(char) * size);
  mem->pos += size;

  return size;
}

static fpos_t seekfn(void *handler, fpos_t offset, int whence) {
  size_t pos;
  fmem_t *mem = handler;

  switch (whence) {
    case SEEK_SET: pos = (unsigned long) offset; break;
    case SEEK_CUR: pos = mem->pos + (unsigned long) offset; break;
    case SEEK_END: pos = mem->size + (unsigned long) offset; break;
    default: return -1;
  }

  if (pos > mem->size) {
    return -1;
  }

  mem->pos = pos;
  return (fpos_t)pos;
}

static int closefn(void *handler) 
{
  free(handler);
  return 0;
}

FILE *fmemopen(void *buf, size_t size, const char *mode) 
{
  // This data is released on fclose.
  fmem_t* mem = (fmem_t *) malloc(sizeof(fmem_t));

  // Zero-out the structure.
  memset(mem, 0, sizeof(fmem_t));

  mem->size = size;
  mem->buffer = buf;

  // funopen's man page: https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/funopen.3.html
  return funopen(mem, readfn, writefn, seekfn, closefn);
}
