/* The embedded loader must work when the host cannot create temporary files. */
#include <stdio.h>
#include <errno.h>
FILE *__wrap_tmpfile(void)
{
    errno = EACCES;
    return NULL;
}
#include "host_capability_test.c"
