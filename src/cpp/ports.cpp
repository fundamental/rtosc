#include "../util.h"
#include "../../include/rtosc/ports.h"
#include "../../include/rtosc/ports-runtime.h"
#include "../../include/rtosc/bundle-foreach.h"

#include <ostream>
#include <cassert>
#include <limits>
#include <cstring>
#include <string>

/* Compatibility with non-clang compilers */
#ifndef __has_feature
# define __has_feature(x) 0
#endif
#ifndef __has_extension
# define __has_extension __has_feature
#endif

/* Check for C++11 support */
#if defined(HAVE_CPP11_SUPPORT)
# if HAVE_CPP11_SUPPORT
#  define DISTRHO_PROPER_CPP11_SUPPORT
# endif
#elif __cplusplus >= 201103L || (defined(__GNUC__) && defined(__GXX_EXPERIMENTAL_CXX0X__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 405) || __has_extension(cxx_noexcept)
# define DISTRHO_PROPER_CPP11_SUPPORT
# if (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 407 && ! defined(__clang__)) || (defined(__clang__) && ! __has_extension(cxx_override_control))
#  define override // gcc4.7+ only
#  define final    // gcc4.7+ only
# endif
#endif

using namespace rtosc;

static inline void scat(char *dest, const char *src)
{
    while(*dest) dest++;
    while(*src && *src!=':') *dest++ = *src++;
    *dest = 0;
}

RtData::RtData(void)
    :loc(NULL), loc_size(0), obj(NULL), matches(0), message(NULL)
{
    for(size_t i=0; i<sizeof(idx)/sizeof(int); ++i)
        idx[i] = 0;
}

void RtData::push_index(int ind)
{
    for(size_t i=1; i<sizeof(idx)/sizeof(int); ++i)
        idx[i] = idx[i-1];
    idx[0] = ind;
}

void RtData::pop_index(void)
{
    int n = sizeof(idx)/sizeof(int);
    for(int i=n-2; i >= 0; --i)
        idx[i] = idx[i+1];
    idx[n-1] = 0;
}

void RtData::replyArray(const char *path, const char *args,
        rtosc_arg_t *vals)
{
    (void) path;
    (void) args;
    (void) vals;
}
void RtData::reply(const char *path, const char *args, ...)
{
    va_list va;
    va_start(va,args);
    char buffer[1024];
    rtosc_vmessage(buffer,1024,path,args,va);
    reply(buffer);
    va_end(va);
}
void RtData::reply(const char *msg)
{(void)msg;}
void RtData::chain(const char *path, const char *args, ...)
{
    (void) path;
    (void) args;
}

void RtData::chain(const char *msg)
{
    (void) msg;
}
void RtData::chainArray(const char *path, const char *args,
        rtosc_arg_t *vals)
{
    (void) path;
    (void) args;
    (void) vals;
};
void RtData::broadcast(const char *path, const char *args, ...)
{
    va_list va;
    va_start(va,args);
    char buffer[1024];
    rtosc_vmessage(buffer,1024,path,args,va);
    broadcast(buffer);
    va_end(va);
}
void RtData::broadcast(const char *msg)
{reply(msg);};
void RtData::broadcastArray(const char *path, const char *args,
        rtosc_arg_t *vals)
{
    (void) path;
    (void) args;
    (void) vals;
}

void RtData::forward(const char *rational)
{
    (void) rational;
}

void metaiterator_advance(const char *&title, const char *&value)
{
    if(!title || !*title) {
        value = NULL;
        return;
    }

    //Try to find "\0=" after title string
    value = title;
    while(*value)
        ++value;
    if(*++value != '=')
        value = NULL;
    else
        value++;
}

Port::MetaIterator::MetaIterator(const char *str)
    :title(str), value(NULL)
{
    metaiterator_advance(title, value);
}

Port::MetaIterator& Port::MetaIterator::operator++(void)
{
    if(!title || !*title) {
        title = NULL;
        return *this;
    }
    //search for next parameter start
    //aka "\0:" unless "\0\0" is seen
    char prev = 0;
    while(prev || (*title && *title != ':'))
        prev = *title++;

    if(!*title)
        title = NULL;
    else
        ++title;

    metaiterator_advance(title, value);
    return *this;
}

Port::MetaIterator::operator bool(void) const
{
    return title;
}

Port::MetaContainer::MetaContainer(const char *str_)
:str_ptr(str_)
{}

Port::MetaIterator Port::MetaContainer::begin(void) const
{
    if(str_ptr && *str_ptr == ':')
        return Port::MetaIterator(str_ptr+1);
    else
        return Port::MetaIterator(str_ptr);
}

Port::MetaIterator Port::MetaContainer::end(void) const
{
    return MetaIterator(NULL);
}

Port::MetaIterator Port::MetaContainer::find(const char *str) const
{
    for(const auto x : *this)
        if(!strcmp(x.title, str))
            return x;
    return NULL;
}

size_t Port::MetaContainer::length(void) const
{
        if(!str_ptr || !*str_ptr)
            return 0;
        char prev = 0;
        const char *itr = str_ptr;
        while(prev || *itr)
            prev = *itr++;
        return 2+(itr-str_ptr);
}

const char *Port::MetaContainer::operator[](const char *str) const
{
    for(const auto x : *this)
        if(!strcmp(x.title, str))
            return x.value;
    return NULL;
}
//Match the arg string or fail
inline bool arg_matcher(const char *pattern, const char *args)
{
    //match anything if now arg restriction is present (ie the ':')
    if(*pattern++ != ':')
        return true;

    const char *arg_str = args;
    bool      arg_match = *pattern || *pattern == *arg_str;

    while(*pattern && *pattern != ':')
        arg_match &= (*pattern++==*arg_str++);

    if(*pattern==':') {
        if(arg_match && !*arg_str)
            return true;
        else
            return arg_matcher(pattern, args); //retry
    }

    return arg_match;
}

inline bool scmp(const char *a, const char *b)
{
    while(*a && *a == *b) a++, b++;
    return a[0] == b[0];
}

typedef std::vector<std::string>  words_t;
typedef std::vector<std::string>  svec_t;
typedef std::vector<const char *> cvec_t;
typedef std::vector<int> ivec_t;
typedef std::vector<int> tuple_t;
typedef std::vector<tuple_t> tvec_t;

namespace rtosc{
class Port_Matcher
{
    public:
        bool *enump;
        svec_t fixed;
        cvec_t arg_spec;
        ivec_t pos;
        ivec_t assoc;
        ivec_t remap;

        bool rtosc_match_args(const char *pattern, const char *msg)
        {
            //match anything if no arg restriction is present
            //(ie the ':')
            if(*pattern++ != ':')
                return true;

            const char *arg_str = rtosc_argument_string(msg);
            bool      arg_match = *pattern || *pattern == *arg_str;

            while(*pattern && *pattern != ':')
                arg_match &= (*pattern++==*arg_str++);

            if(*pattern==':') {
                if(arg_match && !*arg_str)
                    return true;
                else
                    return rtosc_match_args(pattern, msg); //retry
            }

            return arg_match;
        }

        bool hard_match(int i, const char *msg)
        {
            if(strncmp(msg, fixed[i].c_str(), fixed[i].length()))
                return false;
            if(arg_spec[i])
                return rtosc_match_args(arg_spec[i], msg);
            else
                return true;
        }
};

}


tvec_t do_hash(const words_t &strs, const ivec_t &pos)
{
    tvec_t  tvec;
    for(auto &s:strs) {
        tuple_t tuple;
        tuple.push_back(s.length());
        for(const auto &p:pos)
            if(p < (int)s.size())
                tuple.push_back(s[p]);
        tvec.push_back(std::move(tuple));
    }
    return tvec;
}

template<class T>
int count_dups(std::vector<T> &t)
{
    int dups = 0;
    int N = t.size();
    bool mark[t.size()];
    memset(mark, 0, N);
    for(int i=0; i<N; ++i) {
        if(mark[i])
            continue;
        for(int j=i+1; j<N; ++j) {
            if(t[i] == t[j]) {
                dups++;
                mark[j] = true;
            }
        }
    }
    return dups;
}

template<class T, class Z>
bool has(T &t, Z&z)
{
    for(auto tt:t)
        if(tt==z)
            return true;
    return false;
}

static int int_max(int a, int b) { return a<b?b:a;}

static ivec_t find_pos(words_t &strs)
{
    ivec_t pos;
    int current_dups = strs.size();
    int N = 0;
    for(auto w:strs)
        N = int_max(N,w.length());

    int pos_best = -1;
    int pos_best_val = std::numeric_limits<int>::max();
    while(true)
    {
        for(int i=0; i<N; ++i) {
            ivec_t npos = pos;
            if(has(pos, i))
                continue;
            npos.push_back(i);
            auto hashed = do_hash(strs, npos);
            int d = count_dups(hashed);
            if(d < pos_best_val) {
                pos_best_val = d;
                pos_best = i;
            }
        }
        if(pos_best_val >= current_dups)
            break;
        current_dups = pos_best_val;
        pos.push_back(pos_best);
    }
    auto hashed = do_hash(strs, pos);
    int d = count_dups(hashed);
    //printf("Total Dups: %d\n", d);
    if(d != 0)
        pos.clear();
    return pos;
}

static ivec_t do_hash(const words_t &strs, const ivec_t &pos, const ivec_t &assoc)
{
    ivec_t ivec;
    ivec.reserve(strs.size());
    for(auto &s:strs) {
        int t = s.length();
        for(auto p:pos)
            if(p < (int)s.size())
                t += assoc[s[p]];
        ivec.push_back(t);
    }
    return ivec;
}

static ivec_t find_assoc(const words_t &strs, const ivec_t &pos)
{
    ivec_t assoc;
    int current_dups = strs.size();
    int N = 127;
    std::vector<char> useful_chars;
    for(auto w:strs)
        for(auto c:w)
            if(!has(useful_chars, c))
                useful_chars.push_back(c);

    for(int i=0; i<N; ++i)
        assoc.push_back(0);

    int assoc_best = -1;
    int assoc_best_val = std::numeric_limits<int>::max();;
    for(int k=0; k<4; ++k)
    {
        for(int i:useful_chars) {
            assoc_best_val = std::numeric_limits<int>::max();
            for(int j=0; j<100; ++j) {
                //printf(".");
                assoc[i] = j;
                auto hashed = do_hash(strs, pos, assoc);
                //for(int i=0; i<hashed.size(); ++i)
                //    printf("%d ", hashed[i]);
                //printf("\n");
                int d = count_dups(hashed);
                //printf("dup %d\n",d);
                if(d < assoc_best_val) {
                    assoc_best_val = d;
                    assoc_best = j;
                }
            }
            assoc[i] = assoc_best;
        }
        if(assoc_best_val >= current_dups)
            break;
        current_dups = assoc_best_val;
    }
    auto hashed = do_hash(strs, pos, assoc);
    //int d = count_dups(hashed);
    //printf("Total Dups Assoc: %d\n", d);
    return assoc;
}

static ivec_t find_remap(words_t &strs, ivec_t &pos, ivec_t &assoc)
{
    ivec_t remap;
    auto hashed = do_hash(strs, pos, assoc);
    //for(int i=0; i<strs.size(); ++i)
    //    printf("%d) '%s'\n", hashed[i], strs[i].c_str());
    int N = 0;
    for(auto h:hashed)
        N = int_max(N,h+1);
    for(int i=0; i<N; ++i)
        remap.push_back(0);
    for(int i=0; i<(int)hashed.size(); ++i)
        remap[hashed[i]] = i;

    return remap;
}

static void generate_minimal_hash(std::vector<std::string> str, Port_Matcher &pm)
{
    if(str.empty())
        return;
    pm.pos   = find_pos(str);
    if(pm.pos.empty()) {
        fprintf(stderr, "rtosc: Failed to generate minimal hash\n");
        return;
    }
    pm.assoc = find_assoc(str, pm.pos);
    pm.remap = find_remap(str, pm.pos, pm.assoc);
}

static void generate_minimal_hash(Ports &p, Port_Matcher &pm)
{
    svec_t keys;
    cvec_t args;

    bool enump = false;
    for(unsigned i=0; i<p.ports.size(); ++i)
        if(strchr(p.ports[i].name, '#'))
            enump = true;
    if(enump)
        return;
    for(unsigned i=0; i<p.ports.size(); ++i)
    {
        std::string tmp = p.ports[i].name;
        const char *arg = NULL;
        int idx = tmp.find(':');
        if(idx > 0) {
            arg = p.ports[i].name+idx;
            tmp = tmp.substr(0,idx);
        }
        keys.push_back(tmp);
        args.push_back(arg);

    }
    pm.fixed    = keys;
    pm.arg_spec = args;

    generate_minimal_hash(keys, pm);
}

Ports::Ports(std::initializer_list<Port> l)
    :ports(l), impl(NULL)
{
    refreshMagic();
}

Ports::~Ports()
{
    delete []impl->enump;
    delete impl;
}

#if !defined(__GNUC__)
#define __builtin_expect(a,b) a
#endif

void Ports::dispatch(const char *m, rtosc::RtData &d, bool base_dispatch) const
{
    // rRecur*Cb have already set d.loc to the required pointer
    // in case no port will match, d.loc will not be touched
    // this enables returning the address of a runtime object

    void *obj = d.obj;

    //handle the first dispatch layer
    if(base_dispatch) {
        d.matches = 0;
        d.message = m;
        if(m && *m == '/')
            m++;
        if(d.loc)
            d.loc[0] = 0;
    }

    //simple case
    if(!d.loc || !d.loc_size) {
        for(const Port &port: ports) {
            if(rtosc_match(port.name,m, NULL))
                d.port = &port, port.cb(m,d), d.obj = obj;
        }
    } else {

        //TODO this function is certainly buggy at the moment, some tests
        //are needed to make it clean
        //XXX buffer_size is not properly handled yet
        if(__builtin_expect(d.loc[0] == 0, 0)) {
            memset(d.loc, 0, d.loc_size);
            d.loc[0] = '/';
        }

        char *old_end = d.loc;
        while(*old_end) ++old_end;

        if(impl->pos.empty()) { //No perfect minimal hash function
            for(unsigned i=0; i<elms; ++i) {
                const Port &port = ports[i];
                const char* m_end;
                if(!rtosc_match(port.name, m, &m_end))
                    continue;
                if(!port.ports)
                    d.matches++;

                //Append the path
                if(strchr(port.name,'#')) {
                    const char *msg = m;
                    char       *pos = old_end;
                    while(*msg && msg != m_end)
                        *pos++ = *msg++;
                    *pos = '\0';
                } else
                    scat(d.loc, port.name);

                d.port = &port;

                //Apply callback
                port.cb(m,d), d.obj = obj;

                //Remove the rest of the path
                char *tmp = old_end;
                while(*tmp) *tmp++=0;
            }
        } else {

            //Define string to be hashed
            unsigned len=0;
            const char *tmp = m;

            while(*tmp && *tmp != '/')
                tmp++;
            if(*tmp == '/')
                tmp++;
            len = tmp-m;

            //Compute the hash
            int t = len;
            for(auto p:impl->pos)
                if(p < (int)len)
                    t += impl->assoc[m[p]];
            if(t >= (int)impl->remap.size() && !default_handler)
                return;
            else if(t >= (int)impl->remap.size() && default_handler) {
                d.matches++;
                default_handler(m,d), d.obj = obj;
                return;
            }

            int port_num = impl->remap[t];

            //Verify the chosen port is correct
            if(__builtin_expect(impl->hard_match(port_num, m), 1)) {
                const Port &port = ports[impl->remap[t]];
                if(!port.ports)
                    d.matches++;

                //Append the path
                if(impl->enump[port_num]) {
                    const char *msg = m;
                    char       *pos = old_end;
                    while(*msg && *msg != '/')
                        *pos++ = *msg++;
                    if(strchr(port.name, '/'))
                        *pos++ = '/';
                    *pos = '\0';
                } else
                    memcpy(old_end, impl->fixed[port_num].c_str(),
                            impl->fixed[port_num].length()+1);

                d.port = &port;

                //Apply callback
                port.cb(m,d), d.obj = obj;

                //Remove the rest of the path
                old_end[0] = '\0';
            } else if(default_handler) {
                d.matches++;
                default_handler(m,d), d.obj = obj;
            }
        }
    }
}

int rtosc::canonicalize_arg_vals(rtosc_arg_val_t* av, size_t n,
                                 const char* port_args,
                                 Port::MetaContainer meta)
{
    const char* first0 = port_args;
    int errors_found = 0;

    // skip "[]:"
    for( ; *first0 && (*first0 == ':' || *first0 == '[' || *first0 == ']');
           ++first0) ;

    size_t arr_size;
    size_t max;
    bool is_array;
    rtosc_arg_val_t* start = av;
    if(av->type == 'a') {
        arr_size = av->val.a.len;
        ++av;
        max = 1; // only one element per bundle element
                 // TODO: multiple may be possible
        is_array = true;
    }
    else {
        arr_size = 1;
        max = n;
        is_array = false;
    }

    for(size_t a = 0; a < arr_size; ++a)
    {
        const char* first = first0;
        for(size_t i = 0; i < max; ++i, ++first, ++av)
        {
            // skip "[]"
            for( ; *first && (*first == '[' || *first == ']'); ++first) ;

            assert(!strchr(first0, '#'));

//            if(is_array) // TODO: currently, only one element per bundle element
//                assert(first[1] == 0);

            if(!*first || *first == ':')
            {
                // (n-i) arguments left, but we have no recipe to convert them
                return n-i;
            }

            if(av->type == 'S' && *first == 'i')
            {
                int val = enum_key(meta, av->val.s);
                if(val == std::numeric_limits<int>::min())
                    ++errors_found;
                else
                {
                    av->type = 'i';
                    av->val.i = val;
                }
            }
        }
    }
    if(is_array && arr_size)
        start->val.a.type = av[-1].type;

    return errors_found;
}

void rtosc::map_arg_vals(rtosc_arg_val_t* av, size_t n,
                         Port::MetaContainer meta)
{
    char mapbuf[20] = "map ";

    for(size_t i = 0; i < n; ++i, ++av)
    {
        if(av->type == 'i')
        {
            snprintf(mapbuf + 4, 16, "%d", av->val.i);
            const char* val = meta[mapbuf];
            if(val)
            {
                av->type = 'S';
                av->val.s = val;
            }
        }
    }
}

/*
 * Miscellaneous
 */

const Port *Ports::operator[](const char *name) const
{
    for(const Port &port:ports) {
        const char *_needle = name,
              *_haystack = port.name;
        while(*_needle && *_needle==*_haystack)_needle++,_haystack++;

        if(*_needle == 0 && (*_haystack == ':' || *_haystack == '\0')) {
            return &port;
        }
    }
    return NULL;
}

static msg_t snip(msg_t m)
{
    while(*m && *m != '/') ++m;
    return m+1;
}

const Port *Ports::apropos(const char *path) const
{
    if(path && path[0] == '/')
        ++path;

    for(const Port &port: ports)
        if(strchr(port.name,'/') && rtosc_match_path(port.name,path, NULL))
            return (strchr(path,'/')[1]==0) ? &port :
                port.ports->apropos(snip(path));

    //This is the lowest level, now find the best port
    for(const Port &port: ports)
        if(*path && (strstr(port.name, path)==port.name ||
                    rtosc_match_path(port.name, path, NULL)))
            return &port;

    return NULL;
}

static bool parent_path_p(char *read, char *start)
{
    if(read-start<2)
        return false;
    return read[0]=='.' && read[-1]=='.' && read[-2]=='/';
}

static void read_path(char *&r, char *start)
{
    while(1)
    {
        if(r<start)
            break;
        bool doBreak = *r=='/';
        r--;
        if(doBreak)
            break;
    }
}

static void move_path(char *&r, char *&w, char *start)
{
    while(1)
    {
        if(r<start)
            break;
        bool doBreak = *r=='/';
        *w-- = *r--;
        if(doBreak)
            break;
    }
}


char *Ports::collapsePath(char *p)
{
    //obtain the pointer to the last non-null char
    char *p_end = p;
    while(*p_end) p_end++;
    p_end--;

    //number of subpaths to consume
    int consuming = 0;

    char *write_pos = p_end;
    char *read_pos = p_end;
    while(read_pos >= p) {
        //per path chunk either
        //(1) find a parent ref and inc consuming
        //(2) find a normal ref and consume
        //(3) find a normal ref and write through
        bool ppath = parent_path_p(read_pos, p);
        if(ppath) {
            read_path(read_pos, p);
            consuming++;
        } else if(consuming) {
            read_path(read_pos, p);
            consuming--;
        } else
            move_path(read_pos, write_pos, p);
    }
    //return last written location, not next to write
    return write_pos+1;
};

void Ports::refreshMagic()
{
    delete impl;
    impl = new Port_Matcher;
    generate_minimal_hash(*this, *impl);
    impl->enump = new bool[ports.size()];
    for(int i=0; i<(int)ports.size(); ++i)
        impl->enump[i] = strchr(ports[i].name, '#');

    elms = ports.size();
}

ClonePorts::ClonePorts(const Ports &ports_,
        std::initializer_list<ClonePort> c)
    :Ports({})
{
    for(auto &to_clone:c) {
        const Port *clone_port = NULL;
        for(auto &p:ports_.ports)
            if(!strcmp(p.name, to_clone.name))
                clone_port = &p;
        if(!clone_port && strcmp("*", to_clone.name)) {
            fprintf(stderr, "Cannot find a clone port for '%s'\n",to_clone.name);
            assert(false);
        }

        if(clone_port) {
            ports.push_back({clone_port->name, clone_port->metadata,
                    clone_port->ports, to_clone.cb});
        } else {
            default_handler = to_clone.cb;
        }
    }

    refreshMagic();
}
MergePorts::MergePorts(std::initializer_list<const rtosc::Ports*> c)
    :Ports({})
{
    //XXX TODO remove duplicates in some sane and documented way
    //e.g. repeated ports override and remove older ones
    for(auto *to_clone:c) {
        assert(to_clone);
        for(auto &p:to_clone->ports) {
            bool already_there = false;
            for(auto &pp:ports)
                if(!strcmp(pp.name, p.name))
                    already_there = true;

            if(!already_there)
                ports.push_back(p);
        }
    }

    refreshMagic();
}

/**
 * @brief Check if the port @p port is enabled
 * @param port The port to be checked. Usually of type rRecur* or rSelf.
 * @param loc The absolute path of @p port
 * @param loc_size The maximum usable size of @p loc
 * @param ports The Ports object containing @p port
 * @param runtime The runtime object (optional)
 * @return True if no runtime is provided or @p port has no enabled property.
 *         Otherwise, the state of the "enabled by" toggle
 */
bool port_is_enabled(const Port* port, char* loc, size_t loc_size,
                     const Ports& base, void *runtime)
{
    // TODO: this code should be improved
    if(port && runtime)
    {
        const char* enable_port = port->meta()["enabled by"];
        if(enable_port)
        {
            /*
                find out which Ports object to dispatch at
                (the current one or its child?)
             */
            const char* n = port->name;
            const char* e = enable_port;
            for( ; *n && (*n == *e) && *n != '/' && *e != '/'; ++n, ++e) ;

            bool subport = (*e == '/' && *n == '/');

            const char* ask_port_str = subport
                                       ? e+1
                                       : enable_port;

            const Ports& ask_ports = subport ? *base[port->name]->ports
                                             : base;

            assert(!strchr(ask_port_str, '/'));
            const Port* ask_port = ask_ports[ask_port_str];
            assert(ask_port);

            rtosc_arg_val_t rval;

            /*
                concatenate the location string
             */
            int loclen = strlen(loc);
            char loc_copy[loc_size];
            strcpy(loc_copy, loc);
            if(subport)
                strncat(loc_copy, "/../", loc_size - loclen - 1);
            strncat(loc_copy, enable_port, loc_size - loclen - 4 - 1);

            char* collapsed_loc = Ports::collapsePath(loc_copy);
            loc_size -= (collapsed_loc - loc_copy);

            /*
                receive the "enabled" property
             */
            char buf[loc_size];
            // TODO: pass a parameter portname_from_base, since Ports might
            //       also be of type a#N/b
            const char* last_slash = strrchr(collapsed_loc, '/');
            fast_strcpy(buf, last_slash ? last_slash + 1 : collapsed_loc,
                        loc_size);

            helpers::get_value_from_runtime(runtime,
                *ask_port, loc_size, collapsed_loc, ask_port_str,
                buf, 0, 1, &rval);
            assert(rval.type == 'T' || rval.type == 'F');
            return rval.type == 'T';
        }
        else // Port has no "enabled" property, so it is always enabled
            return true;
    }
    else // no runtime provided, so run statically through all subports
        return true;
}

// TODO: copy the changes into walk_ports_2
void rtosc::walk_ports(const Ports  *base,
                       char         *name_buffer,
                       size_t        buffer_size,
                       void         *data,
                       port_walker_t walker,
                       bool          expand_bundles,
                       void*         runtime)
{
    auto walk_ports_recurse = [](const Port& p, char* name_buffer,
                                 size_t buffer_size, const Ports& base,
                                 void* data, port_walker_t walker,
                                 void* runtime, const char* old_end,
                                 bool expand_bundles)
    {
        // TODO: all/most of these checks must also be done for the
        // first, non-recursive call
        bool enabled = true;
        if(runtime)
        {
            enabled = (p.meta().find("no walk") == p.meta().end());
            if(enabled)
            {
                // get child runtime and check if it's NULL
                RtData r;
                r.obj = runtime;
                r.port = &p;

                char buf[1024] = "";
                fast_strcpy(buf, old_end, sizeof(buf));
                // there is no "pointer" callback. thus, there will be nothing
                // dispatched, but the rRecur*Cb already have set r.obj
                // that way, we get our pointer
                strncat(buf, "pointer", sizeof(buf) - strlen(buf) - 1);
                assert(1024 - strlen(buf) >= 8);
                fast_strcpy(buf + strlen(buf) + 1, ",", 2);

                p.cb(buf, r);
                // if there is runtime information (see above), but this pointer
                // is NULL, the port is not enabled
                enabled = (bool) r.obj; // r.obj = the next runtime object
                if(enabled)
                {
                    // check if the port is disabled by a switch
                    enabled = port_is_enabled(&p, name_buffer, buffer_size,
                                              base, runtime);
                }
                runtime = r.obj; // callback has stored the child pointer here
            }
        }
        if(enabled)
            rtosc::walk_ports(p.ports, name_buffer, buffer_size,
                              data, walker, expand_bundles, runtime);
    };

    //only walk valid ports
    if(!base)
        return;

    assert(name_buffer);
    //XXX buffer_size is not properly handled yet
    if(name_buffer[0] == 0)
        name_buffer[0] = '/';

    char *old_end         = name_buffer;
    while(*old_end) ++old_end;

    if(port_is_enabled((*base)["self:"], name_buffer, buffer_size, *base,
                       runtime))
    for(const Port &p: *base) {
        //if(strchr(p.name, '/')) {//it is another tree
        if(p.ports) {//it is another tree
            if(strchr(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"%d",i);

                    //Ensure the result is a path
                    if(strrchr(name_buffer, '/')[1] != '/')
                        strcat(name_buffer, "/");

                    //Recurse
                    walk_ports_recurse(p, name_buffer, buffer_size,
                                       *base, data, walker, runtime, old_end,
                                       expand_bundles);
                }
            } else {
                //Append the path
                const char* old_end = name_buffer + strlen(name_buffer);
                scat(name_buffer, p.name);

                //Recurse
                walk_ports_recurse(p, name_buffer, buffer_size,
                                   *base, data, walker, runtime, old_end,
                                   expand_bundles);
            }
        } else {
            if(strchr(p.name,'#')) {
                bundle_foreach(p, p.name, old_end, name_buffer, *base,
                               data, runtime, walker, expand_bundles);
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Apply walker function
                walker(&p, name_buffer, old_end, *base, data, runtime);
            }
        }

        //Remove the rest of the path
        char *tmp = old_end;
        while(*tmp) *tmp++=0;
    }
}

void walk_ports2(const rtosc::Ports *base,
                 char         *name_buffer,
                 size_t        buffer_size,
                 void         *data,
                 rtosc::port_walker_t walker)
{
    if(!base)
        return;

    assert(name_buffer);
    //XXX buffer_size is not properly handled yet
    if(name_buffer[0] == 0)
        name_buffer[0] = '/';

    char *old_end         = name_buffer;
    while(*old_end) ++old_end;

    for(const rtosc::Port &p: *base) {
        if(strchr(p.name, '/')) {//it is another tree
            if(strchr(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                //for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"[0,%d]",max-1);

                    //Ensure the result is a path
                    if(strrchr(name_buffer, '/')[1] != '/')
                        strcat(name_buffer, "/");

                    //Recurse
                    walk_ports2(p.ports, name_buffer, buffer_size,
                                data, walker);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Recurse
                walk_ports2(p.ports, name_buffer, buffer_size,
                        data, walker);
            }
        } else {
            if(strchr(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                //for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"[0,%d]",max-1);

                    //Apply walker function
                    walker(&p, name_buffer, old_end, *base, data, nullptr);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Apply walker function
                walker(&p, name_buffer, old_end, *base, data, nullptr);
            }
        }

        //Remove the rest of the path
        char *tmp = old_end;
        while(*tmp) *tmp++=0;
    }
}

void rtosc::path_search(const rtosc::Ports& root,
                        const char *str, const char* needle,
                        char *types, std::size_t max_types,
                        rtosc_arg_t* args, std::size_t max_args)
{
    using rtosc::Ports;
    using rtosc::Port;

    if(!needle)
        needle = "";

    // the last char of "types" is being used for a terminating 0
    std::size_t  max         = std::min(max_types - 1, max_args);
    size_t       pos         = 0;
    const Ports *ports       = nullptr;
    const Port  *single_port = nullptr;

    //zero out data
    memset(types, 0, max + 1);
    memset(args,  0, max);

    if(!*str) {
        ports = &root;
    } else {
        const Port *port = root.apropos(str);
        if(port) {
            if(port->ports) {
                ports = port->ports;
            } else {
                single_port = port;
            }
        }
    }

    const auto fn = [&pos,&needle,&types,&args,&max](const Port& p)
    {
        assert(pos < max);
        if(p.name && strstr(p.name, needle) == p.name)
        {
            types[pos]    = 's';
            args[pos++].s = p.name;
            types[pos]    = 'b';
            if(p.metadata && *p.metadata) {
                args[pos].b.data = (unsigned char*) p.metadata;
                auto tmp = rtosc::Port::MetaContainer(p.metadata);
                args[pos++].b.len  = tmp.length();
            } else {
                args[pos].b.data = (unsigned char*) NULL;
                args[pos++].b.len  = 0;
            }
        }
    };

    if(ports)
        for(const Port &p:*ports) fn(p);
    else if(single_port)
        fn(*single_port);
}

std::size_t rtosc::path_search(const Ports &root, const char *m,
                               std::size_t max_ports,
                               char *msgbuf, std::size_t bufsize)
{
    const char *str    = rtosc_argument(m,0).s;
    const char *needle = rtosc_argument(m,1).s;
    size_t max_args    = max_ports << 1;
    size_t max_types   = max_args + 1;
    char types[max_types];
    rtosc_arg_t args[max_args];

    path_search(root, str, needle, types, max_types, args, max_args);
    size_t length = rtosc_amessage(msgbuf, bufsize,
                                   "/paths", types, args);
    return length;
}

static void units(std::ostream &o, const char *u)
{
    if(!u)
        return;
    o << " units=\"" << u << "\"";
}

using std::ostream;
using std::string;
static int enum_min(Port::MetaContainer meta)
{
    int min = 0;
    for(auto m:meta)
        if(strstr(m.title, "map "))
            min = atoi(m.title+4);

    for(auto m:meta)
        if(strstr(m.title, "map "))
            min = min>atoi(m.title+4) ? atoi(m.title+4) : min;

    return min;
}

static int enum_max(Port::MetaContainer meta)
{
    int max = 0;
    for(auto m:meta)
        if(strstr(m.title, "map "))
            max = atoi(m.title+4);

    for(auto m:meta)
        if(strstr(m.title, "map "))
            max = max<atoi(m.title+4) ? atoi(m.title+4) : max;

    return max;
}

int rtosc::enum_key(Port::MetaContainer meta, const char* value)
{
    int result = std::numeric_limits<int>::min();

    for(auto m:meta)
    if(strstr(m.title, "map "))
    if(!strcmp(m.value, value))
    {
        result = atoi(m.title+4);
        break;
    }

    return result;
}

static ostream &add_options(ostream &o, Port::MetaContainer meta)
{
    string sym_names = "xyzabcdefghijklmnopqrstuvw";
    int sym_idx = 0;
    bool has_options = false;
    for(auto m:meta)
        if(strstr(m.title, "map "))
            has_options = true;
    for(auto m:meta)
        if(strcmp(m.title, "documentation") &&
                strcmp(m.title, "parameter") &&
                strcmp(m.title, "max") &&
                strcmp(m.title, "min"))
        printf("m.title = <%s>\n", m.title);

    if(!has_options)
        return o;

    o << "    <hints>\n";
    for(auto m:meta) {
        if(strstr(m.title, "map ")) {
            o << "      <point symbol=\"" << sym_names[sym_idx++] << "\" value=\"";
            o << m.title+4 << "\">" << m.value << "</point>\n";
        }
    }
    o << "    </hints>\n";

    return o;
}
static ostream &dump_t_f_port(ostream &o, string name, string doc)
{
    o << " <message_in pattern=\"" << name << "\" typetag=\"T\">\n";
    o << "  <desc>Enable " << doc << "</desc>\n";
    o << "  <param_T symbol=\"x\"/>\n";
    o << " </message_in>\n";
    o << " <message_in pattern=\"" << name << "\" typetag=\"F\">\n";
    o << "  <desc>Disable "  << doc << "</desc>\n";
    o << "  <param_F symbol=\"x\"/>\n";
    o << " </message_in>\n";
    o << " <message_in pattern=\"" << name << "\" typetag=\"\">\n";
    o << "  <desc>Get state of " << doc << "</desc>\n";
    o << " </message_in>\n";
    o << " <message_out pattern=\"" << name << "\" typetag=\"T\">\n";
    o << "  <desc>Value of " << doc << "</desc>\n";
    o << "  <param_T symbol=\"x\"/>";
    o << " </message_out>\n";
    o << " <message_out pattern=\"" << name << "\" typetag=\"F\">\n";
    o << "  <desc>Value of " <<  doc << "</desc>\n";
    o << "  <param_F symbol=\"x\"/>";
    o << " </message_out>\n";
    return o;
}
static ostream &dump_any_port(ostream &o, string name, string doc)
{
    o << " <message_in pattern=\"" << name << "\" typetag=\"*\">\n";
    o << "  <desc>" << doc << "</desc>\n";
    o << " </message_in>\n";
    return o;
}

static ostream &dump_generic_port(ostream &o, string name, string doc, string type)
{
    const char *t = type.c_str();
    string arg_names = "xyzabcdefghijklmnopqrstuvw";

    //start out with argument separator
    if(*t++ != ':')
        return o;
    //now real arguments (assume [] don't exist)
    string args;
    while(*t && *t != ':')
        args += *t++;

    o << " <message_in pattern=\"" << name << "\" typetag=\"" << args << "\">\n";
    o << "  <desc>" << doc << "</desc>\n";

    assert(args.length()<arg_names.length());
    for(unsigned i=0; i<args.length(); ++i)
        o << "  <param_" << args[i] << " symbol=\"" << arg_names[i] << "\"/>\n";
    o << " </message_in>\n";

    if(*t == ':')
        return dump_generic_port(o, name, doc, t);
    else
        return o;
}

void dump_ports_cb(const rtosc::Port *p, const char *name,const char*,
                   const Ports&,void *v, void*)
{
    std::ostream &o  = *(std::ostream*)v;
    auto meta        = p->meta();
    const char *args = strchr(p->name, ':');
    auto mparameter  = meta.find("parameter");
    auto mdoc        = meta.find("documentation");
    string doc;

    if(mdoc != p->meta().end())
        doc = mdoc.value;
    if(meta.find("internal") != meta.end()) {
        doc += "[INTERNAL]";
    }

    if(mparameter != p->meta().end()) {
        char type = 0;
        if(args) {
            if(strchr(args, 'f'))
                type = 'f';
            else if(strchr(args, 'i'))
                type = 'i';
            else if(strchr(args, 'c'))
                type = 'c';
            else if(strchr(args, 'T'))
                type = 't';
            else if(strchr(args, 's'))
                type = 's';
        }

        if(!type) {
            fprintf(stderr, "rtosc port dumper: Cannot handle '%s'\n", name);
            fprintf(stderr, "    args = <%s>\n", args);
            return;
        }

        if(type == 't') {
            dump_t_f_port(o, name, doc);
            return;
        }

        o << " <message_in pattern=\"" << name << "\" typetag=\"" << type << "\">\n";
        o << "  <desc>Set Value of " << doc << "</desc>\n";
        if(meta.find("min") != meta.end() && meta.find("max") != meta.end() && type != 'c') {
            o << "  <param_" << type << " symbol=\"x\"";
            units(o, meta["unit"]);
            o << ">\n";
            o << "   <range_min_max " << (type == 'f' ? "lmin=\"[\" lmax=\"]\"" : "");
            o << " min=\"" << meta["min"] << "\"  max=\"" << meta["max"] << "\"/>\n";
            o << "  </param_" << type << ">";
        } else if(meta.find("enumerated") != meta.end()) {
            o << "  <param_" << type << " symbol=\"x\">\n";
            o << "    <range_min_max min=\"" << enum_min(meta) << "\" max=\"";
            o << enum_max(meta) << "\">\n";
            add_options(o, meta);
            o << "    </range_min_max>\n";
            o << "  </param_" << type << ">\n";
        } else {
            o << "  <param_" << type << " symbol=\"x\"";
            units(o, meta["unit"]);
            o << "/>\n";
        }
        o << " </message_in>\n";
        o << " <message_in pattern=\"" << name << "\" typetag=\"\">\n";
        o << "  <desc>Get Value of " << doc << "</desc>\n";
        o << " </message_in>\n";
        o << " <message_out pattern=\"" << name << "\" typetag=\"" << type << "\">\n";
        o << "  <desc>Value of " << doc << "</desc>\n";
        if(meta.find("min") != meta.end() && meta.find("max") != meta.end() && type != 'c') {
            o << "  <param_" << type << " symbol=\"x\"";
            units(o, meta["unit"]);
            o << ">\n";
            o << "   <range_min_max " << (type == 'f' ? "lmin=\"[\" lmax=\"]\"" : "");
            o << " min=\"" << meta["min"] << "\"  max=\"" << meta["max"] << "\"/>\n";
            o << "  </param_" << type << ">\n";
        } else if(meta.find("enumerated") != meta.end()) {
            o << "  <param_" << type << " symbol=\"x\">\n";
            o << "    <range_min_max min=\"" << enum_min(meta) << "\" max=\"";
            o << enum_max(meta) << "\">\n";
            add_options(o, meta);
            o << "    </range_min_max>\n";
            o << "  </param_" << type << ">\n";
        } else {
            o << "  <param_" << type << " symbol=\"x\"";
            units(o, meta["unit"]);
            o << "/>\n";
        }
        o << " </message_out>\n";
    } else if(mdoc != meta.end() && (!args || args == std::string(""))) {
        dump_any_port(o, name, doc);
    } else if(mdoc != meta.end() && args) {
        dump_generic_port(o, name, doc, args);
    } else if(mdoc != meta.end()) {
        fprintf(stderr, "Skipping \"%s\"\n", name);
        if(args) {
            fprintf(stderr, "    type = %s\n", args);
        }
    } else
        fprintf(stderr, "Skipping [UNDOCUMENTED] \"%s\"\n", name);
}

std::ostream &rtosc::operator<<(std::ostream &o, rtosc::OscDocFormatter &formatter)
{
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    o << "<osc_unit format_version=\"1.0\">\n";
    o << " <meta>\n";
    o << "  <name>" << formatter.prog_name << "</name>\n";
    o << "  <uri>" << formatter.uri << "</uri>\n";
    o << "  <doc_origin>" << formatter.doc_origin << "</doc_origin>\n";
    o << "  <author><firstname>" << formatter.author_first;
    o << "</firstname><lastname>" << formatter.author_last << "</lastname></author>\n";
    o << " </meta>\n";
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    walk_ports2(formatter.p, buffer, 1024, &o, dump_ports_cb);
    o << "</osc_unit>\n";
    return o;
}

