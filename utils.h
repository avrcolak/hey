#ifndef _UTILS_H
#define _UTILS_H

#define ARRAYSIZE(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define min(a, b)  (((a) < (b)) ? (a) : (b))

#endif
