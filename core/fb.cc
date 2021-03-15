// FrameBuffer implementation
#include <cerrno>
#include <cstdint>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <string>
#include <iostream>

#include "mxcfb.h"


struct Point {
    int x, y;
};

struct Rect {
    Point topLeft;
    Point bottomRight;

    int height() const {
        return bottomRight.y - topLeft.y;
    }

    int width() const {
        return bottomRight.x - topLeft.x;
    }
};

class FrameBuffer: public Rect {
public:
    int device;
    std::string path;

    int frame_length;
    uint8_t* mem_map;

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

        if( fb.vinfo.bits_per_pixel != 16)
            throw "FrameBuffer: expected 16 bits per pixel, but got " + std::to_string( fb.vinfo.bits_per_pixel) + "\n"
                   + " If running on rM2, you should use rmf2b\n";


        fb.finfo = fb.get_finfo();
       

        // map the memory
        fb.frame_length = fb.finfo.line_length * fb.vinfo.yres;
        fb.mem_map = (uint8_t*) mmap( 0, fb.frame_length, PROT_READ | PROT_WRITE, MAP_SHARED, fb.device, 0);
        if( fb.mem_map == nullptr)
            throw "FrameBuffer: unable to mmap frame '" + fb.path + "'\n";

        return fb;
    }

    void to_s() const {
        std::cerr << "opened '" << path << "' - " << vinfo.xres << "x" << vinfo.yres 
             << ", depth: " << vinfo.bits_per_pixel << " bits"
             << ", row: "   << finfo.line_length    << " bytes"
             << ", addr: " << (void*) mem_map << ", size:" << frame_length << " bytes"
             << std::endl;
    }

    // Not sure we should implement this
    // kernel should take of that => and we should not copy that object
    ~FrameBuffer() {
        std::cerr << "closed" << std::endl;
       // ::close( device);
    }

    // raw info
    fb_var_screeninfo raw_vinfo() const {
        // Fetch the size of the display.
        fb_var_screeninfo screenInfo;
        if( ioctl(device, FBIOGET_VSCREENINFO, &screenInfo))
            throw "FrameBuffer: could not FBIOGET_VSCREENFINO '" + path + "'.\n";

        return screenInfo;
    }

    // update info
    void raw_vinfo( const fb_var_screeninfo& screenInfo) const {
        if( ioctl(device, FBIOPUT_VSCREENINFO, &screenInfo))
            throw "FrameBuffer: could not FBIOPUT_VSCREENINFO '" + path + "'.\n";
    }

    // update vinfo
    // on rm2/rm2buffer raw output: opened '/dev/fb0' -  260x1408, depth: 32bits
    // using rmfb2      once fixed: opened '/dev/fb0' - 1404x1872, depth: 16 bits, row: 2808 bytes, addr: 0x752f5000, size:5256576 bytes
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

    // basic drawing
    void fill( uint8_t color ) {
        std::memset( mem_map, color, frame_length );
    }

    // full refresh
    void refresh() const {
        mxcfb_update_data whole{
            mxcfb_rect{0,0,vinfo.xres,vinfo.yres},
            6, //0,                    //waveform,
            0, // UPDATE_MODE_FULL,       // mode,
            0x2a, // marker,
            0x0018, //TEMP_USE_AMBIENT, // temp,
            0,  // flags
            0, //dither_mode,
            0, // quant_bit,
            mxcfb_alt_buffer_data{}
        };

        int status;
        if( status = ioctl( device, MXCFB_SEND_UPDATE, &whole))
            throw "FrameBuffer: failed to MXCFB_SEND_UPDATE: " + std::to_string(status) 
                    + " (errno= " + std::to_string(errno) + ") \n";
    }

    // fast refresh of selected area
    void refresh( const Rect& r) const {
        mxcfb_update_data whole{
            mxcfb_rect{ r.topLeft.y, r.topLeft.x, r.width(), r.height()},
            6, //0,                    //waveform,
            0, // UPDATE_MODE_FULL,       // mode,
            0x2a, // marker,
            0x0018, //TEMP_USE_AMBIENT, // temp,
            0,  // flags
            0, //dither_mode,
            0, // quant_bit,
            mxcfb_alt_buffer_data{}
        };

        int status;
        if( status = ioctl( device, MXCFB_SEND_UPDATE, &whole))
            throw "FrameBuffer: failed to MXCFB_SEND_UPDATE: " + std::to_string(status) 
                    + " (errno= " + std::to_string(errno) + ") \n";
    }

    // animation: fast refresh of selected areas
    void refresh( const Rect& r1, const Rect& r2) {
        refresh( r1);
        refresh( r2);
    }
};
