// FrameBuffer implementation
#include <cstdint>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

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

        /*
        if( fb.vinfo.bits_per_pixel != 16)
            throw "FrameBuffer: expected 16 bits per pixel, but got " + std::to_string( fb.vinfo.bits_per_pixel) + "\n";
        */

        fb.finfo = fb.get_finfo();
       

        // map the memory
        fb.frame_length = fb.finfo.line_length * fb.vinfo.yres;
        fb.mem_map = (uint8_t*) mmap( 0, fb.frame_length, PROT_READ | PROT_WRITE, MAP_SHARED, fb.device, (off_t) 0);
        if( fb.mem_map == nullptr)
            throw "FrameBuffer: unable to mmap frame '" + fb.path + "'\n";

        return fb;
    }

    void to_s() const {
        std::cerr << "opened '" << path << "' - " << vinfo.xres << "x" << vinfo.yres 
             << ", depth: " << vinfo.bits_per_pixel << "bits"
             << ", row: "   << finfo.line_length    << "bytes"
             << std::endl;
    }

    // Not sure we should implement this
    // kernel should take of that => and we should not copy that object
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

    // basic drawing
    void fill( uint8_t color ) {
        std::memset( mem_map, color, frame_length );
    }

    uint32_t full_refresh(
        waveform_mode waveform,
        temperature: common::display_temp,
        dither_mode: common::dither_mode,
        quant_bit: i32,
        bool wait_completion = true
    ) const {
        let screen = common::mxcfb_rect {
            top: 0,
            left: 0,
            height: self.var_screen_info.yres,
            width: self.var_screen_info.xres,
        };
        let marker = self.marker.fetch_add(1, Ordering::Relaxed);
        mxcfb_update_data whole{
            update_mode: common::update_mode::UPDATE_MODE_FULL as u32,
            update_marker: marker as u32,
            waveform,
            temp: temperature as i32,
            flags: 0,
            quant_bit,
            dither_mode: dither_mode as i32,
            update_region: screen,
            ..Default::default()
        };

        if( ioctl( device, MXCFB_SEND_UPDATE, &whole))
            throw "FrameBuffer: failed to MXCFB_SEND_UPDATE\n";

        if( wait_completion) {
            let mut markerdata = mxcfb_update_marker_data {
                update_marker: whole.update_marker,
                collision_test: 0,
            };
            unsafe {
                if libc::ioctl(
                    self.device.as_raw_fd(),
                    common::MXCFB_WAIT_FOR_UPDATE_COMPLETE,
                    &mut markerdata,
                ) < 0
                {
                    warn!("WAIT_FOR_UPDATE_COMPLETE failed after a full_refresh(..)");
                }
            }
        }
        
        return whole.update_marker
    }

};


#include <iostream>

using namespace std;

int main(int argc,char** argv) {
try {
    cerr << "Hello World" << endl;
    auto fb = FrameBuffer::open();
    fb.to_s();

    fb.fill( 0);
    fb.full_refresh();

    sleep( 4);

    fb.fill( 255);
    fb.full_refresh();
 

    cerr << "done" << endl;
}
catch(const char* msg) {
    cerr << "ERROR : " << msg << endl;
}
catch(const string& msg) {
    cerr << "ERROR : " << msg << endl;
}
}