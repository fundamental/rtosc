/*
 * Copyright (c) 2012 Mark McCurry
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
 * @file ports.h
 * Collection of functions for ports.
 * This includes dispatchin, reading metadata etc.
 */

#ifndef RTOSC_PORTS
#define RTOSC_PORTS

#include <vector>
#include <functional>
#include <initializer_list>
#include <rtosc/rtosc.h>
#include <string>
#include <cstdio>
#include <iosfwd>

namespace rtosc {

//First define all types
typedef const char *msg_t;

struct Port;
struct Ports;

//! data object for the dispatch routine
struct RtData
{
    RtData(void);

    /**
     * Location of where the dispatch routine is currently being called.
     * If non-NULL, the dispatch routine will update the port name here while
     * walking through the Ports tree
     */
    char *loc;
    size_t loc_size;
    void *obj;        //!< runtime object to dispatch this object to
    int  matches;     //!< number of matches returned from dispatch routine
    const Port *port; //!< dispatch will write the matching port's pointer here
    //! @brief Will be set to point to the full OSC message in case of
    //!   a base dispatch
    const char *message;

    int idx[16];
    void push_index(int ind);
    void pop_index(void);

    virtual void replyArray(const char *path, const char *args,
            rtosc_arg_t *vals);
    virtual void reply(const char *path, const char *args, ...);
    //!Reply if information has been requested
    virtual void reply(const char *msg);
    virtual void chain(const char *path, const char *args, ...);
    //!Bypass message to some kind of backend if the message can not be handled
    virtual void chain(const char *msg);
    virtual void chainArray(const char *path, const char *args,
            rtosc_arg_t *vals);
    //!Transmit initialization/change of a value to all listeners
    virtual void broadcast(const char *path, const char *args, ...);
    virtual void broadcast(const char *msg);
    virtual void broadcastArray(const char *path, const char *args,
            rtosc_arg_t *vals);

    virtual void forward(const char *rational=NULL);
};


/**
 * Port in rtosc dispatching hierarchy
 */
struct Port {
    const char  *name;    //!< Pattern for messages to match
    const char  *metadata;//!< Statically accessable data about port
    const Ports *ports;   //!< Pointer to further ports
    std::function<void(msg_t, RtData&)> cb;//!< Callback for matching functions

    class MetaIterator
    {
        public:
            MetaIterator(const char *str);

            //A bit odd to return yourself, but it seems to work for this
            //context
            const MetaIterator& operator*(void) const {return *this;}
            const MetaIterator* operator->(void) const {return this;}
            bool operator==(MetaIterator a) {return title == a.title;}
            bool operator!=(MetaIterator a) {return title != a.title;}
            MetaIterator& operator++(void);
            operator bool() const;

            const char *title;
            const char *value;
    };

    class MetaContainer
    {
        public:
            MetaContainer(const char *str_);

            MetaIterator begin(void) const;
            MetaIterator end(void) const;

            MetaIterator find(const char *str) const;
            size_t length(void) const;
            //!Return the key to the value @p str, or NULL if the key is
            //!invalid or if there's no value for that key.
            const char *operator[](const char *str) const;

            const char *str_ptr;
    };

    MetaContainer meta(void) const
    {
        if(metadata && *metadata == ':')
            return MetaContainer(metadata+1);
        else
            return MetaContainer(metadata);
    }

};

/**
 * Ports - a dispatchable collection of Port entries
 *
 * This structure makes it somewhat easier to perform actions on collections of
 * port entries and it is responsible for the dispatching of OSC messages to
 * their respective ports.
 * That said, it is a very simple structure, which uses a stl container to store
 * all data in a simple dispatch table.
 * All methods post-initialization are RT safe (assuming callbacks are RT safe)
 */
struct Ports
{
    std::vector<Port> ports;
    std::function<void(msg_t, RtData&)> default_handler;

    typedef std::vector<Port>::const_iterator itr_t;

    /**Forwards to builtin container*/
    itr_t begin() const {return ports.begin();}

    /**Forwards to builtin container*/
    itr_t end() const {return ports.end();}

    /**Forwards to builtin container*/
    size_t size() const {return ports.size();}

    /**Forwards to builtin container*/
    const Port &operator[](unsigned i) const {return ports[i];}

    Ports(std::initializer_list<Port> l);
    ~Ports(void);

    Ports(const Ports&) = delete;

    /**
     * @brief Dispatches message to all matching ports.
     *
     * This uses simple pattern matching available in rtosc::match().
     *
     * @invariant In each recursion, "d.loc + m" must make the port's full path.
     *   For further recursions, a call to scat() will append
     *   the toplevel of m to d.loc, and in the rRecur* callbacks, SNIP() will
     *   cut the toplevel from m. I.e.: loc="", m="a/b/c" => loc="/a/", m="b/c".
     *
     * @param m A valid OSC message. Note that the address part shall not
     *          contain any type specifier.
     * @param d The RtData object shall contain a path buffer (or null), the
     *          length of the buffer, a pointer to data. It must also contain
     *          the location if an answer is being expected.
     * @param base_dispatch Whether the OSC path is to be interpreted as a full
     *                      OSC path beginning at the root
     */
    void dispatch(const char *m, RtData &d, bool base_dispatch=false) const;

    /**
     * Retrieve local port by name
     * TODO implement full matching
     */
    const Port *operator[](const char *name) const;


    /**
     * @brief Find the best match for a given path
     *
     * @param path partial OSC path
     * @returns first path prefixed by the argument
     *
     * Example usage:
     * @code
     *    Ports p = {{"foo",0,0,dummy_method},
     *               {"flam",0,0,dummy_method},
     *               {"bar",0,0,dummy_method}};
     *    p.apropos("/b")->name;//bar
     *    p.apropos("/f")->name;//foo
     *    p.apropos("/fl")->name;//flam
     *    p.apropos("/gg");//NULL
     * @endcode
     */
    const Port *apropos(const char *path) const;

    /**
     * Collapse path with parent path identifiers "/.."
     *
     * e.g. /foo/bar/../baz => /foo/baz
     */
    static char *collapsePath(char *p);

    protected:
    void refreshMagic(void);
    private:
    //Performance hacks
    class Port_Matcher *impl;
    unsigned elms;
};

struct ClonePort
{
    const char *name;
    std::function<void(msg_t, RtData&)> cb;
};

struct ClonePorts:public Ports
{
    ClonePorts(const Ports &p,
               std::initializer_list<ClonePort> c);
};

struct MergePorts:public Ports
{
    MergePorts(std::initializer_list<const Ports*> c);
};

/**
 * Convert given argument values to their canonical representation.
 *
 * The ports first (or-wise) argument types are defined as canonical.
 * E.g. if passing two 'S' argument values, the
 * port could be `portname::ii:cc:SS` or `portname::ii:t`.
 *
 * @param av The input and output argument values
 * @param n The size of @p av
 * @param port_args The port arguments string, e.g. `::i:c:s`. The first
 *   non-colon letter sequence marks the canonical types
 * @param meta The port's metadata container
 * @return The number of argument values that should have need conversion,
 *   but failed, e.g. because of values missing in rMap.
 */
int canonicalize_arg_vals(rtosc_arg_val_t* av, size_t n,
                          const char* port_args, Port::MetaContainer meta);

/**
 * Convert each of the given arguments to their mapped symbol, if possible.
 * @param av The input and output argument values
 * @param n The size of @p av
 * @param meta The port's metadata container
 */
void map_arg_vals(rtosc_arg_val_t* av, size_t n,
                  Port::MetaContainer meta);

/*********************
 * Port walking code *
 *********************/
//typedef std::function<void(const Port*,const char*)> port_walker_t;
/**
 * Function pointer type for port walking.
 *
 * accepts:
 * - the currently walked port
 * - the port's absolute location
 * - the part of the location which makes up the port; this is usually the
 *   location's substring after the last slash, but it can also contain multiple
 *   slashes
 * - the port's base, i.e. it's parent Ports struct
 * - the custom data supplied to walk_ports
 * - the runtime object (which may be NULL if not known)
 */
typedef void(*port_walker_t)(const Port*,const char*,const char*,
                             const Ports&,void*,void*);

/**
 * Call a function on all ports and subports.
 * @param base The base port of traversing
 * @param name_buffer Buffer which will be filled with the port name; must be
 *   reset to zero over the full length!
 * @param buffer_size Size of name_buffer
 * @param data Data that should be available in the callback
 * @param walker Callback function
 * @param expand_bundles Whether walking over bundles without subports
 *   invokes walking over each of the bundle's port
 * @param runtime Runtime object corresponding to @param base. If given, checks
 *   the runtime object will be used stop recursion if the Ports are disabled
 *   (using the "enabled by" property) and it will be passed to the walker
 */
void walk_ports(const Ports *base,
                char          *name_buffer,
                size_t         buffer_size,
                void          *data,
                port_walker_t  walker,
                bool expand_bundles = true,
                void *runtime = NULL);

/**
 * Returns paths and metadata of all direct children of a port, or of the port
 * itself if that port has no children.
 * If you just want to generate a reply message, use the overloaded function.
 * @param root see @p m
 * @param str location under @p root to look up port, or empty-string to search
 *            directly at @p root
 * @param needle Only port names starting with this sting are returned (use
 *            empty-string or nullptr to match everything)
 * @param types A buffer where the OSC type string is being written
 * @param max_types should be @p max_args +1 for best performance,
 *   see @p max_args
 * @param args An array where the argument values are being written
 * @param max_args maximum number of arguments in array at @p args. Should be
 *   greater or equal than 2 * (maximum no. of child ports of your app's ports).
 */
void path_search(const rtosc::Ports& root, const char *str, const char *needle,
                 char *types, std::size_t max_types,
                 rtosc_arg_t* args, std::size_t max_args);

/**
 * Returns a messages of all paths and metadata of all direct children of a
 * port, or of the port itself if that port has no children.
 *
 * Your app should always have a port "path-search", calling this function
 * directly, and replying the resulting buffer @p msgbuf directly. That way, it
 * will be ready for oscprompt, port-checker etc.
 *
 * @param root See @p m
 * @param m a valid OSC message requesting the path search. The corresponding
 *   port args must be of types
 *   * "s" (location under @p root to look up port, or empty-string to search
 *          directly at @p root)
 *   * "s" (only port names starting with this sting are returned, use
 *          empty-string or nullptr to match everything)
 * @param max_ports Maximum number (or higher) of child ports in any of your
 *     app's ports.
 * @param msgbuf Buffer for the reply message
 * @param bufsize Size of the message buffer @p msgbuf
 * @return The length of the reply message (0 means error)
 */
std::size_t path_search(const rtosc::Ports& root, const char *m,
                        std::size_t max_ports,
                        char *msgbuf, std::size_t bufsize);

/**
 * Return the index with value @p value from the metadata's enumeration.
 * @param meta The metadata
 * @param value The value to search the key for
 * @return The first key holding value, or `std::numeric_limits<int>::min()`
 *   if none was found
 */
int enum_key(Port::MetaContainer meta, const char* value);

/*********************
 * Port Dumping code *
 *********************/

struct OscDocFormatter
{
    const Ports *p;
    std::string prog_name;
    std::string uri;
    std::string doc_origin;
    std::string author_first;
    std::string author_last;
    //TODO extend this some more
};

std::ostream &operator<<(std::ostream &o, OscDocFormatter &formatter);
};
#endif
