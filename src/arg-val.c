#include "util.h"
#include <rtosc/arg-val.h>
#include <rtosc/arg-val-itr.h>

size_t rtosc_avmessage(char                  *buffer,
		       size_t                 len,
		       const char            *address,
		       size_t                 nargs,
		       const rtosc_arg_val_t *args)
{
    rtosc_arg_val_itr itr;
    rtosc_arg_val_itr_init(&itr, args);

    int val_max;
    {
	// equivalent to the for loop below, in order to
	// find out the array size
	rtosc_arg_val_itr itr2 = itr;
	for(val_max = 0; itr2.i < nargs; ++val_max)
	    rtosc_arg_val_itr_next(&itr2);
    }

    STACKALLOC(rtosc_arg_t, vals, val_max);
    STACKALLOC(char, argstr,val_max+1);

    int i;
    for(i = 0; i < val_max; ++i)
    {
	rtosc_arg_val_t av_buffer;
	const rtosc_arg_val_t* cur = rtosc_arg_val_itr_get(&itr, &av_buffer);
	vals[i] = cur->val;
	argstr[i] = cur->type;
	rtosc_arg_val_itr_next(&itr);
    }
    argstr[i] = 0;

    return rtosc_amessage(buffer, len, address, argstr, vals);
}
