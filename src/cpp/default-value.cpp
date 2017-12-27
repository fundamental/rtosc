#include <cassert>
#include <cstring>
#include <rtosc/pretty-format.h>
#include <rtosc/ports.h>
#include <rtosc/ports-runtime.h>
#include <rtosc/default-value.h>

namespace rtosc {

const char* get_default_value(const char* port_name, const Ports& ports,
                              void* runtime, const Port* port_hint,
                              int32_t idx, int recursive)
{
    constexpr std::size_t buffersize = 8192;
    char buffer[buffersize];
    char loc[buffersize] = "";

    assert(recursive >= 0); // forbid recursing twice

    char default_annotation[20] = "default";
//    if(idx > 0)
//        snprintf(default_annotation + 7, 13, "[%" PRId32 "]", idx);
    const char* const dependent_annotation = "default depends";
    const char* return_value = nullptr;

    if(!port_hint)
        port_hint = ports.apropos(port_name);
    assert(port_hint); // port must be found
    const Port::MetaContainer metadata = port_hint->meta();

    // Let complex cases depend upon a marker variable
    // If the runtime is available the exact preset number can be found
    // This generalizes to envelope types nicely if envelopes have a read
    // only port which indicates if they're amplitude/frequency/etc
    const char* dependent = metadata[dependent_annotation];
    if(dependent)
    {
        char* dependent_port = buffer;
        *dependent_port = 0;

        assert(strlen(port_name) + strlen(dependent_port) + 5 < buffersize);
        strncat(dependent_port, port_name,
                buffersize - strlen(dependent_port) - 1);
        strncat(dependent_port, "/../",
                buffersize - strlen(dependent_port) - 1);
        strncat(dependent_port, dependent,
                buffersize - strlen(dependent_port) - 1);
        dependent_port = Ports::collapsePath(dependent_port);

        // TODO: collapsePath bug?
        // Relative paths should not start with a slash after collapsing ...
        if(*dependent_port == '/')
            ++dependent_port;

        const char* dependent_value =
            runtime
            ? helpers::get_value_from_runtime(runtime, ports,
                                              buffersize, loc,
                                              dependent_port,
                                              buffersize-1, 0)
            : get_default_value(dependent_port, ports,
                                runtime, NULL, recursive-1);

        assert(strlen(dependent_value) < 16); // must be an int

        char* default_variant = buffer;
        *default_variant = 0;
        assert(strlen(default_annotation) + 1 + 16 < buffersize);
        strncat(default_variant, default_annotation,
                buffersize - strlen(default_variant));
        strncat(default_variant, " ", buffersize - strlen(default_variant));
        strncat(default_variant, dependent_value,
                buffersize - strlen(default_variant));

        return_value = metadata[default_variant];
    }

    // If return_value is NULL, this can have two meanings:
    //   1. there was no depedent annotation
    //     => check for a direct (non-dependent) default value
    //        (a non existing direct default value is OK)
    //   2. there was a dependent annotation, but the dependent value has no
    //      mapping (mapping for default_variant was NULL)
    //     => check for the direct default value, which acts as a default
    //        mapping for all presets; a missing default value indicates an
    //        error in the metadata
    if(!return_value)
    {
        return_value = metadata[default_annotation];
        assert(!dependent || return_value);
    }

    return return_value;
}

int get_default_value(const char* port_name, const char* port_args,
                      const Ports& ports, void* runtime, const Port* port_hint,
                      int32_t idx, std::size_t n, rtosc_arg_val_t* res,
                      char* strbuf, size_t strbufsize)
{
    const char* pretty = get_default_value(port_name, ports, runtime, port_hint,
                                           idx, 0);

    int nargs;
    if(pretty)
    {
        nargs = rtosc_count_printed_arg_vals(pretty);
        assert(nargs > 0); // parse error => error in the metadata?
        assert((size_t)nargs < n);

        rtosc_scan_arg_vals(pretty, res, nargs, strbuf, strbufsize);

        {
            // TODO: port_hint could be NULL here!
            int errs_found = canonicalize_arg_vals(res,
                                                   nargs,
                                                   port_args,
                                                   port_hint->meta());
            if(errs_found)
                fprintf(stderr, "Could not canonicalize %s for port %s\n",
                        pretty, port_name);
            assert(!errs_found); // error in the metadata?
        }
    }
    else
        nargs = -1;

    return nargs;
}

}
