#ifndef ACA_UTIL_H
#define ACA_UTIL_H

#include <stdlib.h>

static inline void aca_free(void *pointer)
{
    if (pointer != NULL)
    {
        free(pointer);
        pointer = NULL;
    }
}

#endif
