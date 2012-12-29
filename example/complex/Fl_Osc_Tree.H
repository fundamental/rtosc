#pragma once
#include <rtosc/ports.h>
#include "Fl_Osc_Interface.H"
#include <FL/Fl_Tree.H>

class Fl_Osc_Tree:Fl_Tree
{
    public:
        Fl_Osc_Tree(int X, int Y, int W, int H, const char *L=0)
            :Fl_Tree(X,Y,W,H,L)
        {
            root_label("");
            add("nil");
            add("/nil/nil");
            close(first());
            callback(Fl_Osc_Tree::cb, NULL);
        }

        static void cb(Fl_Widget *w, void*)
        {
            using namespace rtosc;
            Fl_Osc_Tree *t=(Fl_Osc_Tree*)w;
            int reason = t->callback_reason();

            char pathname[1024];
            t->item_pathname(pathname, sizeof(pathname), t->callback_item());


            if(reason==1) {
                char *colon = index(pathname, ':');
                if(colon) {
                    *colon = 0;
                    t->osc->writeValue("/learn", string(pathname));
                }
            }

            if(reason==3) //Populate fields
            {
                const mPorts *p=Fl_Osc_Tree::subtree_lookup(t->root_ports,pathname+1);
                if(auto *i = t->find_item((std::string(pathname)+"/"+"nil").c_str()))
                    t->remove(i);
                for(unsigned i=0; i<p->nports(); ++i) {
                    bool subnodes = index(p->port(i).name,'/');
                    string path = std::string(pathname)+"/"+p->port(i).name;
                    if(subnodes) {
                        t->add(path.c_str());
                        t->add((path+"nil").c_str());
                        t->close(path.c_str());
                    } else if(index(p->port(i).metadata,'v')) {
                        t->add(path.c_str());
                    }
                }

            }
        }

        static const rtosc::mPorts *subtree_lookup(const rtosc::mPorts *p, std::string s)
        {
            using namespace rtosc;
            if(s=="")
                return p;

            for(unsigned i = 0; i < p->nports(); ++i) {
                const mPort &port = p->port(i);
                const char *name  = port.name;
                if(index(name,'/') && s.substr(0,(index(name,'/')-name)+1) == std::string(name).substr(0,strlen(name)-1)){
                    return subtree_lookup(port.ports, s.substr(index(name,'/')-name));
                }
            }

            //TODO else case
            return p;
        }

        rtosc::mPorts *root_ports;
        Fl_Osc_Interface *osc;
};
