#ifndef ARGVALITR_H
#define ARGVALITR_H

#include <rtosc/rtosc.h>

/*
 * arg val iterators
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Iterator over arg values
 *
 * Always use this iterator for iterating because it automatically skips
 * to the next offset, even in case of arrays etc.
 * Also, it walks through ranges, as if they were not existing.
 */
typedef struct
{
    const rtosc_arg_val_t* av; //!< the arg val referenced
    size_t i;                  //!< position of this arg val
    int range_i;               //!< position of this arg val in its range
} rtosc_arg_val_itr;

void rtosc_arg_val_itr_init(rtosc_arg_val_itr* itr,
			    const rtosc_arg_val_t* av);
//! this usually just returns the value from operand, except for range operands,
//! where the value is being interpolated
//! @param buffer Temporary. Don't access it afterwards.
const rtosc_arg_val_t* rtosc_arg_val_itr_get(
    const rtosc_arg_val_itr* itr,
    rtosc_arg_val_t* buffer);
//! @warning will loop forever on infinite ranges!
void rtosc_arg_val_itr_next(rtosc_arg_val_itr* itr);

#ifdef __cplusplus
};
#endif

#endif // ARGVALITR_H
