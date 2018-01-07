/*
 * Copyright (c) 2017 Johannes Lorenz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @file savefile.h
 * Functions for reading and loading rtosc savefiles
 *
 * @test default-value.cpp
 */

#ifndef RTOSC_SAVEFILE
#define RTOSC_SAVEFILE

#include <string>
#include <rtosc/rtosc.h>
#include <rtosc/rtosc-version.h>

namespace rtosc {

/**
 * Return a string list of all changed values
 *
 * Return a human readable list of the value that changed
 * corresponding to the rDefault macro
 * @param ports The static ports structure
 * @param runtime The runtime object
 * @note This function is not realtime save (It uses std::string), which is
 *   usually OK, since this function is being run inside a non-RT thread. If you
 *   need this to be realtime save, add a template parameter for a functor that
 *   replaces the std::string handling.
 * @return The list of ports and their changed values, linewise
 */
std::string get_changed_values(const struct Ports& ports, void* runtime);

//! @brief Class to modify and dispatch messages loaded from savefiles.
//! Objects of this class shall be passed to savefile loading routines. You can
//! inherit to change the behaviour, e.g. to modify or discard such messages.
class savefile_dispatcher_t
{
    const struct Ports* ports;
    void* runtime;
    char loc[1024];

protected:
    enum proceed {
        abort = -2, //!< the message shall lead to abort the savefile loading
        discard = -1 //!< the message shall not be dispatched
    };

    enum dependency_t {
        no_dependencies,  //! default values don't depend on others
        has_dependencies, //! default values do depend on others
        not_specified     //! it's not know which of the other enum values fit
    };

    rtosc_version rtosc_filever, //!< rtosc versinon savefile was written with
                  rtosc_curver, //!< rtosc version of this library
                  app_filever, //!< app version savefile was written with
                  app_curver; //!< current app version

    //! call this to dispatch a message
    bool operator()(const char* msg) { return do_dispatch(msg); }

    static int default_response(size_t nargs, bool first_round,
                                dependency_t dependency);

private:
    //! callback for when a message shall be dispatched
    //! implement this if you need to change a message
    virtual int on_dispatch(size_t portname_max, char* portname,
                            size_t maxargs, size_t nargs,
                            rtosc_arg_val_t* args,
                            bool round2, dependency_t dependency);
    //! call this to dispatch a message
    virtual bool do_dispatch(const char* msg);

    friend int dispatch_printed_messages(const char* messages,
                                         const struct Ports& ports,
                                         void* runtime,
                                         savefile_dispatcher_t *dispatcher);

    friend int load_from_file(const char* file_content,
                              const struct Ports& ports, void* runtime,
                              const char* appname,
                              rtosc_version appver,
                              savefile_dispatcher_t* dispatcher);
};

/**
 * Scan OSC messages from human readable format and dispatch them.
 * @param messages The OSC messages, whitespace-separated
 * @param ports The static ports structure
 * @param runtime The runtime object
 * @param dispatcher Object to modify messages prior to dispatching, or NULL.
 *   You can overwrite its virtual functions, and you should specify any of the
 *   version structs if needed. All other members shall not be initialized.
 * @return The number of messages read, or, if there was a read error,
 *   or the dispatcher did refuse to dispatch,
 *   the number of bytes read until the read error occured minus one
 */
int dispatch_printed_messages(const char* messages,
                              const struct Ports& ports, void* runtime,
                              savefile_dispatcher_t *dispatcher = NULL);

/**
 * Return a savefile containing all values that differ from the default values.
 * @param ports The static ports structure
 * @param runtime The runtime object
 * @param appname Name of the application calling this function
 * @param appver Version of the application calling this function
 * @return The resulting savefile as an std::sting
 */
std::string save_to_file(const struct Ports& ports, void* runtime,
                         const char* appname, rtosc_version appver);

/**
 * Read save file and dispatch contained parameters.
 * @param file_content The file as a C string
 * @param ports The static ports structure
 * @param runtime The runtime object
 * @param appname Name of the application calling this function; must
 *   match the file's application name
 * @param appver Version of the application calling this function
 * @param dispatcher Modifier for the messages; NULL if no modifiers are needed
 * @return The number of messages read, or, if there was a read error,
 *   the negated number of bytes read until the read error occured minus one
 */
int load_from_file(const char* file_content,
                   const struct Ports& ports, void* runtime,
                   const char* appname,
                   rtosc_version appver,
                   savefile_dispatcher_t* dispatcher = NULL);

}

#endif // RTOSC_SAVEFILE
