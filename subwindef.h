/* Minimal Windows type definitions for this program.  This is
   primarily intended to assist in platform independence. */

#ifndef WINVER
#ifndef SUBWINDEF_H
#define SUBWINDEF_H

typedef int BOOL;

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

typedef long LONG;

typedef struct tagPOINT
{
    LONG  x;
    LONG  y;
} POINT;

#ifndef MAX_PATH
#define MAX_PATH 260;
#endif

#endif /* SUBWINDEF_H */
#endif /* WINVER */
