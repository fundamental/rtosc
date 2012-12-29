#include <functional>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <rtosc/rtosc.h>

typedef const char *msg_t;

template<class T>
std::function<void(msg_t,T*)> parf(float T::*p)
{
    return [p](msg_t m, T*t){(t->*p)=rtosc_argument(m,0).f;};
}

const char *snip(const char *m);

template<class T, class TT>
std::function<void(msg_t,T*)> recur(TT T::*p)
{
    return [p](msg_t m, T*t){(t->*p).dispatch(snip(m));};
}

//template<class T, class TT>
//std::function<void(msg_t,T*)> recur_array(TT T::*p)
//{
//    return [p](msg_t m, T*t){
//        msg_t mm = m;
//        while(!isdigit(*mm))++mm;
//        (t->*p)[atoi(mm)].dispatch(snip(m));
//    };
//}

template<class T, class TT, int len>
std::function<void(msg_t,T*)> recur_array(TT (T::*p)[len])
{
    return [p](msg_t m, T*t){
        msg_t mm = m;
        while(!isdigit(*mm))++mm;
        (t->*p)[atoi(mm)].dispatch(snip(m));
    };
}

template<class T, class TT, class TTT>
std::function<void(msg_t,T*)> cast(TTT T::*p)
{
    return [p](msg_t m, T*t){TT *tt = dynamic_cast<TT*>(t->*p);
                             if(tt)tt->dispatch(snip(m));};
}
