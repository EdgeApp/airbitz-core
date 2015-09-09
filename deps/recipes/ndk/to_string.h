#ifndef _TO_STRING_H_INCLUDED_
#define _TO_STRING_H_INCLUDED_

#include <stdio.h>

namespace std {

inline string to_string(double val)
{
    char temp[64];
    snprintf(temp, sizeof(temp), "%.16g", val);
    return string(temp);
}

}

#endif
