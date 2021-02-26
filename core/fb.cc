// FrameBuffer implementation
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include <string>
#include <iostream>

struct Point {
    int x, y;
};

struct Rect {
    Point topLeft;
    Point bottomRight;
};

class FrameBuffer: public Rect {
    int device;
    std::string path;
    uint16_t* data;

    fb_var_screeninfo vinfo;
    fb_fix_screeninfo finfo;

public:
    static
    FrameBuffer open( const char* path = "/dev/fb0") {
        FrameBuffer fb;
        fb.path = path;

        fb.device = ::open( path, O_RDWR);
        if( fb.device < 1)  
            throw "FrameBuffer: could not open '" + fb.path + "'\n";

        fb.vinfo = fb.get_vinfo();
        fb.bottomRight = Point{ (int) fb.vinfo.xres, (int) fb.vinfo.yres};

        /*
        if( fb.vinfo.bits_per_pixel != 16)
            throw "FrameBuffer: expected 16 bits per pixel, but got " + std::to_string( fb.vinfo.bits_per_pixel) + "\n";
        */

       fb.finfo = fb.get_finfo();
       

        // TODO map the memory


        return fb;
    }

    void to_s() const {
        std::cerr << "opened '" << path << "' - " << vinfo.xres << "x" << vinfo.yres 
             << ", depth: " << vinfo.bits_per_pixel << "bits"
             << ", row: "   << finfo.line_length    << "bytes"
             << std::endl;
    }

    //
    ~FrameBuffer() {
        ::close( device);
    }

    // raw info
    fb_var_screeninfo raw_vinfo() const {
        // Fetch the size of the display.
        fb_var_screeninfo screenInfo;
        if( ioctl(device, FBIOGET_VSCREENINFO, &screenInfo) != 0)
            throw "FrameBuffer: could not FBIOGET_VSCREENFINO '" + path + "'.\n";

        return screenInfo;
    }

    // update info
    void raw_vinfo( const fb_var_screeninfo& screenInfo) const {
        if( ioctl(device, FBIOPUT_VSCREENINFO, &screenInfo) != 0)
            throw "FrameBuffer: could not FBIOPUT_VSCREENINFO '" + path + "'.\n";
    }

    // update vinfo
    // on rm2/rm2buffer raw output: opened '/dev/fb0' - 260x1408, depth: 32bits
    //                  once fixed: opened '/dev/fb0' - 1404x1872, depth: 32bits
    // taken from libremarkable rust code
    fb_var_screeninfo get_vinfo() const {
        auto var_screen_info = raw_vinfo();
        var_screen_info.xres = 1404;
        var_screen_info.yres = 1872;
        var_screen_info.rotate = 1;
        var_screen_info.width = 0xffff'ffff;
        var_screen_info.height = 0xffff'ffff;
        var_screen_info.pixclock = 6250;
        var_screen_info.left_margin = 32;
        var_screen_info.right_margin = 326;
        var_screen_info.upper_margin = 4;
        var_screen_info.lower_margin = 12;
        var_screen_info.hsync_len = 44;
        var_screen_info.vsync_len = 1;
        var_screen_info.sync = 0;
        var_screen_info.vmode = 0; // FB_VMODE_NONINTERLACED
        var_screen_info.accel_flags = 0;

        raw_vinfo( var_screen_info);    // fix device definition

        return raw_vinfo();
    }


    fb_fix_screeninfo get_finfo() const {
        fb_fix_screeninfo finfo;
        if( ioctl( device, FBIOGET_FSCREENINFO, &finfo))
            throw "FrameBuffer: could not FBIOGET_FSCREENINFO '" + path + "'\n";

        return finfo;
    }

};


#include <iostream>

using namespace std;

int main(int argc,char** argv) {
try {
    cerr << "Hello World" << endl;
    auto fb = FrameBuffer::open();
    fb.to_s();

}
catch(const char* msg) {
    cerr << "ERROR : " << msg << endl;
}
catch(const string& msg) {
    cerr << "ERROR : " << msg << endl;
}
}