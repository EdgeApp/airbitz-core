/**
 * @file
 * AirBitz Debug function prototypes
 *
 * See LICENSE for copy, modification, and use permissions 
 *
 * @author See AUTHORS
 * @version 1.0
 */
#ifndef ABC_Debug_h
#define ABC_Debug_h
#include <strings.h>
#include "ABC.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define ABC_DEBUG(cmd)      cmd
#else
#define ABC_DEBUG(cmd)
#endif 
    
void ABC_DebugLog(const char * format, ...);

#ifdef __cplusplus
}
#endif

#endif