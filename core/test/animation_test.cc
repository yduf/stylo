#include <iostream>
#include <math.h>

#include "../fb.cc"

using namespace std;

#include "cairo.h"

// small wrapper around cairo
// for test (see cairomm as alternative)
/* Destroy a cairo surface */


struct Draw {
	cairo_surface_t* surface;
	cairo_t *cr;

    Draw( FrameBuffer& fb)
    {
        /* Create the cairo surface which will be used to draw to */
        surface = cairo_image_surface_create_for_data(  fb.mem_map,
                                                        CAIRO_FORMAT_RGB16_565,
                                                        fb.vinfo.xres,
                                                        fb.vinfo.yres,
                                                        cairo_format_stride_for_width(CAIRO_FORMAT_RGB16_565, fb.vinfo.xres));
        cairo_surface_set_user_data(surface, NULL, this,
                                    &cairo_linuxfb_surface_destroy);
	    cr = cairo_create(surface);

        /* 
        * We clear the cairo surface here before drawing
        * This is required in case something was drawn on this surface
        * previously, the previous contents would not be cleared without this.
        */
        //cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        //cairo_paint(cr);
        //cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, 10, 10);
        cairo_line_to(cr, 1000, 1500);

        cairo_stroke(cr);    
    }

    static
    void cairo_linuxfb_surface_destroy(void *device)
    {
    }

    Rect draw_at( int x, int y) {
        cairo_arc(cr, x, y, 10.0, 0.0, 2 * M_PI);
        cairo_fill_preserve(cr);
        cairo_stroke(cr);

        return Rect{ Point{x,y}, Point{x+10,y+10} };
    }

    Rect erase_at( int x, int y ) {
        return Rect{ Point{x,y}, Point{x+10,y+10} };
    }
};

int main(int argc,char** argv) {
try {
    cerr << "Hello World" << endl;
    auto fb = FrameBuffer::open();
    fb.to_s();

    // drawing tool
    Draw dr( fb);

    // orbit a figure around the center of the screen
    const Point center{ fb.width()/2, fb.height()/2};
    const double r = 500;
    double x = 0, y = 0;

    for( double a=0; a < 2*3.14; a += 1*3.14/180) 
    {
        //int a = 0;
        auto erase_rect = dr.erase_at( x, y);        // erase previous

        // move
        double x  = r*cos(a) + center.x;
        double y  = r*sin(a) + center.y;

        auto draw_rect = dr.draw_at( x, y);         // redraw

        // fast refresh
        fb.refresh( draw_rect, erase_rect);
    }


    cerr << "done" << endl;
}
catch(const char* msg) {
    cerr << "ERROR : " << msg << endl;
}
catch(const string& msg) {
    cerr << "ERROR : " << msg << endl;
}
}