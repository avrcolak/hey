#ifndef _WINDOWS_SHIMS_H
#define _WINDOWS_SHIMS_H

#define ARRAYSIZE(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

typedef long LONG;

typedef struct tagRECT
{
    LONG    left;
    LONG    top;
    LONG    right;
    LONG    bottom;
} RECT;

void InflateRect(RECT* lprc, int dx, int dy);

#endif
