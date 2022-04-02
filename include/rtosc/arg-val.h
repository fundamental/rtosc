#ifndef ARGVAL_H
#define ARGVAL_H

#include <rtosc/rtosc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @see rtosc_message()
 */
size_t rtosc_avmessage(char        *buffer,
		       size_t       len,
		       const char  *address,
		       size_t       nargs,
		       const rtosc_arg_val_t *args);

#ifdef __cplusplus
}
#endif

#endif // ARGVAL_H
