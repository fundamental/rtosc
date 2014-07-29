#include "../../include/rtosc/ports.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <string>

using namespace rtosc;

static inline void scat(char *dest, const char *src)
{
    while(*dest) dest++;
    if(*dest) dest++;
    while(*src && *src!=':') *dest++ = *src++;
    *dest = 0;
}

RtData::RtData(void)
    :loc(NULL), loc_size(0), obj(NULL), matches(0)
{}

void RtData::reply(const char *path, const char *args, ...){(void)path;(void)args;};
void RtData::reply(const char *msg){(void)msg;};
void RtData::broadcast(const char *path, const char *args, ...){(void)path;(void)args;};
void RtData::broadcast(const char *msg){(void)msg;};

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
            //match anything if now arg restriction is present
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

int max(int a, int b) { return a<b?b:a;}

ivec_t find_pos(words_t &strs)
{
    ivec_t pos;
    int current_dups = strs.size();
    int N = 0;
    for(auto w:strs)
        N = max(N,w.length());

    int pos_best = -1;
    int pos_best_val = INT_MAX;
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

ivec_t do_hash(const words_t &strs, const ivec_t &pos, const ivec_t &assoc)
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

ivec_t find_assoc(const words_t &strs, const ivec_t &pos)
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
    int assoc_best_val = INT_MAX;
    for(int k=0; k<4; ++k)
    {
        for(int i:useful_chars) {
            assoc_best_val = INT_MAX;
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
    int d = count_dups(hashed);
    //printf("Total Dups Assoc: %d\n", d);
    return assoc;
}

ivec_t find_remap(words_t &strs, ivec_t &pos, ivec_t &assoc)
{
    ivec_t remap;
    auto hashed = do_hash(strs, pos, assoc);
    //for(int i=0; i<strs.size(); ++i)
    //    printf("%d) '%s'\n", hashed[i], strs[i].c_str());
    int N = 0;
    for(auto h:hashed)
        N = max(N,h+1);
    for(int i=0; i<N; ++i)
        remap.push_back(0);
    for(int i=0; i<(int)hashed.size(); ++i)
        remap[hashed[i]] = i;

    return remap;
}

void generate_minimal_hash(std::vector<std::string> str, Port_Matcher &pm)
{
    pm.pos   = find_pos(str);
    if(pm.pos.empty()) {
        fprintf(stderr, "rtosc: Failed to generate minimal hash\n");
        return;
    }
    pm.assoc = find_assoc(str, pm.pos);
    pm.remap = find_remap(str, pm.pos, pm.assoc);
}

void generate_minimal_hash(Ports &p, Port_Matcher &pm)
{
    svec_t keys;
    cvec_t args;

    bool enump = false;
    for(unsigned i=0; i<p.ports.size(); ++i)
        if(index(p.ports[i].name, '#'))
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
    :ports(l), impl(new Port_Matcher)
{
    generate_minimal_hash(*this, *impl);
    impl->enump = new bool[ports.size()];
    for(int i=0; i<(int)ports.size(); ++i)
        impl->enump[i] = index(ports[i].name, '#');

    elms = ports.size();
}

Ports::~Ports()
{
    delete []impl->enump;
    delete impl;
}

#if !defined(__GNUC__)
#define __builtin_expect(a,b) a
#endif

void Ports::dispatch(const char *m, rtosc::RtData &d) const
{
    void *obj = d.obj;
    //simple case
    if(!d.loc || !d.loc_size) {
        for(const Port &port: ports) {
            if(rtosc_match(port.name,m))
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
                if(!rtosc_match(port.name, m))
                    continue;
                if(!port.ports)
                    d.matches++;

                //Append the path
                if(index(port.name,'#')) {
                    const char *msg = m;
                    char       *pos = old_end;
                    while(*msg && *msg != '/')
                        *pos++ = *msg++;
                    if(index(port.name, '/'))
                        *pos++ = '/';
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
            if(t >= (int)impl->remap.size())
                return;
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
                    if(index(port.name, '/'))
                        *pos++ = '/';
                    *pos = '\0';
                } else
                    memcpy(old_end, impl->fixed[port_num].c_str(),
                            impl->fixed[port_num].length());

                d.port = &port;

                //Apply callback
                port.cb(m,d), d.obj = obj;

                //Remove the rest of the path
                old_end[0] = '\0';
            }
        }
    }
}

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

msg_t snip(msg_t m)
{
    while(*m && *m != '/') ++m;
    return m+1;
}

const Port *Ports::apropos(const char *path) const
{
    if(path && path[0] == '/')
        ++path;

    for(const Port &port: ports)
        if(index(port.name,'/') && rtosc_match_path(port.name,path))
            return (index(path,'/')[1]==0) ? &port :
                port.ports->apropos(snip(path));

    //This is the lowest level, now find the best port
    for(const Port &port: ports)
        if(*path && strstr(port.name, path)==port.name)
            return &port;

    return NULL;
}

void rtosc::walk_ports(const Ports *base,
                       char         *name_buffer,
                       size_t        buffer_size,
                       void         *data,
                       port_walker_t walker)
{
    assert(name_buffer);
    //XXX buffer_size is not properly handled yet
    if(name_buffer[0] == 0)
        name_buffer[0] = '/';

    char *old_end         = name_buffer;
    while(*old_end) ++old_end;

    for(const Port &p: *base) {
        if(index(p.name, '/')) {//it is another tree
            if(index(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"%d",i);

                    //Ensure the result is a path
                    if(rindex(name_buffer, '/')[1] != '/')
                        strcat(name_buffer, "/");

                    //Recurse
                    rtosc::walk_ports(p.ports, name_buffer, buffer_size,
                            data, walker);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Recurse
                rtosc::walk_ports(p.ports, name_buffer, buffer_size,
                        data, walker);
            }
        } else {
            if(index(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"%d",i);

                    //Apply walker function
                    walker(&p, name_buffer, data);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Apply walker function
                walker(&p, name_buffer, data);
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
    assert(name_buffer);
    //XXX buffer_size is not properly handled yet
    if(name_buffer[0] == 0)
        name_buffer[0] = '/';

    char *old_end         = name_buffer;
    while(*old_end) ++old_end;

    for(const rtosc::Port &p: *base) {
        if(index(p.name, '/')) {//it is another tree
            if(index(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                //for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"[0,%d]",max);

                    //Ensure the result is a path
                    if(rindex(name_buffer, '/')[1] != '/')
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
            if(index(p.name,'#')) {
                const char *name = p.name;
                char       *pos  = old_end;
                while(*name != '#') *pos++ = *name++;
                const unsigned max = atoi(name+1);

                //for(unsigned i=0; i<max; ++i)
                {
                    sprintf(pos,"[0,%d]",max);

                    //Apply walker function
                    walker(&p, name_buffer, data);
                }
            } else {
                //Append the path
                scat(name_buffer, p.name);

                //Apply walker function
                walker(&p, name_buffer, data);
            }
        }

        //Remove the rest of the path
        char *tmp = old_end;
        while(*tmp) *tmp++=0;
    }
}

void units(const char*u)
{
    if(!u)
        return;
    printf(" units=\"%s\"",u);
}

void dump_ports_cb(const rtosc::Port *p, const char *name, void*)
{
    auto meta = p->meta();
    if(meta.find("parameter") != p->meta().end()) {
        char type = 0;
        const char *foo = index(p->name, ':');
        if(index(foo, 'f'))
            type = 'f';
        else if(index(foo, 'i'))
            type = 'i';
        else if(index(foo, 'c'))
            type = 'c';
        else if(index(foo, 'T'))
            type = 't';
        if(!type) {
            printf("Cannot handle '%s'\n", p->name);
            return;
        }

        if(type == 't')
        {
            printf(" <message_in pattern=\"%s\" typetag=\"T\">\n", name);
            printf("  <desc>Enable %s</desc>\n", p->meta()["documentation"]);
            printf("  <param_T symbol=\"x\"/>");
            printf(" </message_in>\n");
            printf(" <message_in pattern=\"%s\" typetag=\"F\">\n", name);
            printf("  <desc>Disable %s</desc>\n", p->meta()["documentation"]);
            printf("  <param_F symbol=\"x\"/>");
            printf(" </message_in>\n");
            printf(" <message_in pattern=\"%s\" typetag=\"\">\n", name);
            printf("  <desc>Get state of %s</desc>\n", p->meta()["documentation"]);
            printf(" </message_in>\n");
            printf(" <message_out pattern=\"%s\" typetag=\"T\">\n", name);
            printf("  <desc>Value of %s</desc>\n", p->meta()["documentation"]);
            printf("  <param_T symbol=\"x\"/>");
            printf(" </message_out>\n");
            printf(" <message_out pattern=\"%s\" typetag=\"F\">\n", name);
            printf("  <desc>Value of %s</desc>\n", p->meta()["documentation"]);
            printf("  <param_F symbol=\"x\"/>");
            printf(" </message_out>\n");
            return;
        }
        //if(type == 'c')
        //    type = 'i';

        printf(" <message_in pattern=\"%s\" typetag=\"%c\">\n", name, type);
        printf("  <desc>Set Value of %s</desc>\n", p->meta()["documentation"]);
        if(meta.find("min") != meta.end() && meta.find("max") != meta.end() && type != 'c')
        {
            printf("  <param_%c symbol=\"x\"", type);units(meta["unit"]);printf(">\n");
            printf("   <range_min_max %s min=\"%s\" max=\"%s\"/>", type == 'f' ? "lmin=\"[\" lmax=\"]\"" : "", meta["min"], meta["max"]);
            printf("  </param_%c>", type);
        } else {
            printf("  <param_%c symbol=\"x\"",type);units(meta["unit"]); printf("/>\n");
        }
        printf(" </message_in>\n");
        printf(" <message_in pattern=\"%s\" typetag=\"\">\n", name);
        printf("  <desc>Get Value of %s</desc>\n", p->meta()["documentation"]);
        printf(" </message_in>\n");
        printf(" <message_out pattern=\"%s\" typetag=\"%c\">\n", name, type);
        printf("  <desc>Value of %s</desc>\n", p->meta()["documentation"]);
        if(meta.find("min") != meta.end() && meta.find("max") != meta.end() && type != 'c')
        {
            printf("  <param_%c symbol=\"x\"", type);units(meta["unit"]);printf(">\n");
            printf("   <range_min_max %s min=\"%s\"  max=\"%s\"/>", type == 'f' ? "lmin=\"[\" lmax=\"]\"" : "", meta["min"], meta["max"]);
            printf("  </param_%c>", type);
        } else {
            printf("  <param_%c symbol=\"x\"",type);units(meta["unit"]); printf("/>\n");
        }
        printf(" </message_out>\n");
    }// else if(meta.find("documentation") != meta.end())
    //    fprintf(stderr, "Skipping \"%s\"\n", name);
    //else
    //    fprintf(stderr, "Skipping [UNDOCUMENTED] \"%s\"\n", name);
}

void dump_ports(Ports *p)
{
    puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    puts("<osc_unit format_version=\"1.0\">");
    puts(" <meta>");
    puts("  <name>ZynAddSubFX</name>");
    puts("  <uri>http://zynaddsubfx.sf.net/</uri>");
    puts("  <doc_origin>http://where.to/?find=this&amp;xml_file</doc_origin>");
    puts("  <author><firstname>Mark</firstname><lastname>McCurry</lastname></author>");
    puts(" </meta>");
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    walk_ports2(p, buffer, 1024, NULL, dump_ports_cb);
    printf("</osc_unit>\n");
    exit(1);
}


