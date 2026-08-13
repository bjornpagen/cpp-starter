/* PR target/126782 */
/* Explicit __need_rsize_t must define rsize_t in C++ as well.  */
/* { dg-do compile } */

#define __need_rsize_t
#include <stddef.h>

#ifdef __need_rsize_t
#error "__need_rsize_t was not consumed"
#endif

rsize_t value;

extern rsize_t *p;
extern __SIZE_TYPE__ *p;
