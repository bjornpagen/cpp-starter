/* Local fixinclude for the macOS SDK's sys/_types/_rsize_t.h.
 *
 * The SDK header tests __has_feature(modules) and, when true, assumes a
 * clang stddef.h that honors the __need_rsize_t protocol. GCC also reports
 * the modules feature under -fmodules, but its stddef.h does not implement
 * that clang-only protocol, so rsize_t was never defined and C11 Annex K
 * declarations in <_string.h> failed to parse. Always use the plain typedef.
 */
#ifndef _RSIZE_T
#define _RSIZE_T
#include <machine/types.h> /* __darwin_size_t */
typedef __darwin_size_t rsize_t;
#endif /* _RSIZE_T */
