#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Table_Row.H>
#include <rtosc/ports.h>
#include <vector>
#include <string>
#include <algorithm>

#define MAX_ROWS 100
#define MAX_COLS 100

struct port_view
{
    std::string name;
    rtosc::Port port;
    int CC;
    int Chan;
};

bool operator<(port_view a, port_view b)
{
    return a.name < b.name;
}


class PortTable: public Fl_Table_Row
{
    int secondary_selection[4];           // secondary selection left/right/top/bottom values
    // (left/right are column #'s, top/bot are row #'s)
    // Draw the row/col headings
    //    Make this a dark thin upbox with the text inside.
    //
    void DrawHeader(const char *s, int X, int Y, int W, int H, int COL) {
        if(COL != -1 && fl_width(s) + 10 > col_width(COL)) {
            col_width(COL,fl_width(s)+10);
            damage();
        }
        fl_push_clip(X,Y,W,H);
        fl_draw_box(FL_FLAT_BOX, X,Y,W,H, row_header_color());
        fl_color(FL_BLACK);
        fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
        fl_pop_clip();
    }
    public:
        std::vector<port_view> ports;
        //std::vector<rtosc::Port> knowledge;
        //std::vector<std::string> names;
        PortTable(int X, int Y, int W, int H, const char *L=0)
            :Fl_Table_Row(X,Y,W,H,L)
        {
            rows(50);
            //Address Type Unit Min Max
            cols(5);
            col_header(1);
            col_resize(1);
            row_header(0);
            row_resize(0);
        }

        void draw_cell(TableContext context, int ROW=0, int COL=0, int X=0, int Y=0, int W=0, int H=0) {
            static char s[40];
            switch ( context ) {
                case CONTEXT_STARTPAGE:                   // before page is drawn..
                    fl_font(FL_HELVETICA, 16);              // set the font for our drawing operations
                    return;
                case CONTEXT_COL_HEADER:                  // Draw column headers
                    if(COL == 0)
                        DrawHeader("Port Name",X,Y,W,H,COL);
                    else if(COL == 1) 
                        DrawHeader("Minimum",X,Y,W,H,COL);
                    else if(COL == 2) 
                        DrawHeader("Maximum",X,Y,W,H,COL);
                    else if(COL == 3) 
                        DrawHeader("MIDI CC",X,Y,W,H,COL);
                    else if(COL == 4) 
                        DrawHeader("Documentation",X,Y,W,H,COL);
                    else {
                        sprintf(s,"%c",'A'+COL);                // "A", "B", "C", etc.
                        DrawHeader(s,X,Y,W,H,COL);
                    }
                    return;
                case CONTEXT_ROW_HEADER:                  // Draw row headers
                    sprintf(s,"%03d:",ROW);                 // "001:", "002:", etc
                    DrawHeader(s,X,Y,W,H,-1);
                    return;
                case CONTEXT_CELL:{                       // Draw data in cells
                    sprintf(s,"%d",ROW*MAX_COLS+COL);       // whatever..
                    // Handle coloring of cells
                    int fgcol = FL_BLACK;
                    int bgcol = fl_color_average(FL_WHITE, FL_BLUE, ROW%2 ? 0.9 : 1);
                    if ( row_selected(ROW) ) {
                        fgcol = FL_WHITE;
                        bgcol = 0xaa4444;   // lt blue
                    }
                    //// Handle secondary selection
                    if ( COL >= secondary_selection[0] && COL <= secondary_selection[1] &&
                            ROW >= secondary_selection[2] && ROW <= secondary_selection[3] ) {
                        fgcol = FL_WHITE;
                        bgcol = 0x44aa44;   // lt green
                    }
                    fl_draw_box(FL_FLAT_BOX, X,Y,W,H, bgcol);
                    fl_color(fgcol);
                    if(COL == 0) {
                        if(fl_width(ports[ROW].name.c_str()) + 10 > col_width(0)) {//might be slower than need be
                            col_width(0,fl_width(ports[ROW].name.c_str())+10);
                            damage();
                        }
                        fl_draw(ports[ROW].name.c_str(), X,Y,W,H, FL_ALIGN_LEFT);
                    } else if(COL == 1) {
                        fl_draw(ports[ROW].port.meta()["min"], X,Y,W,H, FL_ALIGN_LEFT);
                    } else if(COL == 2) {
                        fl_draw(ports[ROW].port.meta()["max"], X,Y,W,H, FL_ALIGN_LEFT);
                    } else if(COL == 3) {
                        if(ports[ROW].Chan == -1)
                            return;
                        sprintf(s, "%d:%d", ports[ROW].Chan, ports[ROW].CC);
                        fl_draw(s, X,Y,W,H, FL_ALIGN_LEFT);
                    } else if(COL == 4) {
                        fl_draw(ports[ROW].port.meta()["documentation"], X,Y,W,H, FL_ALIGN_LEFT);
                    } else
                        fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
                    return;
            }
                default:
                    return;
            }
        }
};


class Fl_Osc_View: public Fl_Double_Window
{
    PortTable *table;

    public:
    Fl_Osc_View(void)
        :Fl_Double_Window(400,400,"View of Osc Activity")
    {
        table = new PortTable(0,0,400,400);
        box(FL_FLAT_BOX);
        resizable(table);
        end();
    }

    void populate(const rtosc::Ports *p) {
        table->ports.clear();
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        rtosc::walk_ports(p, buffer, sizeof(buffer), table,
                [](const rtosc::Port *p, const char *name, void*t) {
                PortTable *table = (PortTable*)t;

                port_view tmp = {name, *p, -1, -1};
                table->ports.push_back(tmp);
                });
        std::sort(table->ports.begin(), table->ports.end());
        table->rows(table->ports.size());
        table->damage();
    }

    void add_midi_cc(const char *path, int chan, int cc)
    {
        for(port_view &p : table->ports) {
            if(p.name == path) {
                p.Chan = chan;
                p.CC   = cc;
            }
        }
        table->damage();
    }
};
