#ifndef _TO_STRING_H_INCLUDED_
#define _TO_STRING_H_INCLUDED_

#include <stdio.h>

namespace std {

inline string to_string(int val)
{
    char temp[64];
    sprintf(temp, "%d", val);
    return string(temp);
}

}

#endif
