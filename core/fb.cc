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

// #include "mxcfb.h" // use libremarkable/legacy-c-impl/libremarkable/lib.h which is cleaner&complete


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

// from libremarkable/legacy-c-impl/libremarkable/lib.h
// 0x4048 is the Remarkable Prefix
// 'F' (0x46) is the namespace
#define REMARKABLE_PREFIX(x) (0x40484600 | x)
typedef enum _eink_ioctl_command {
  MXCFB_SET_WAVEFORM_MODES	           = 0x2B, // takes struct mxcfb_waveform_modes
  MXCFB_SET_TEMPERATURE		             = 0x2C, // takes int32_t
  MXCFB_SET_AUTO_UPDATE_MODE           = 0x2D, // takes __u32
  MXCFB_SEND_UPDATE                    = 0x2E, // takes struct mxcfb_update_data
  MXCFB_WAIT_FOR_UPDATE_COMPLETE       = 0x2F, // takes struct mxcfb_update_marker_data
  MXCFB_SET_PWRDOWN_DELAY              = 0x30, // takes int32_t
  MXCFB_GET_PWRDOWN_DELAY              = 0x31, // takes int32_t
  MXCFB_SET_UPDATE_SCHEME              = 0x32, // takes __u32
  MXCFB_GET_WORK_BUFFER                = 0x34, // takes unsigned long
  MXCFB_SET_TEMP_AUTO_UPDATE_PERIOD    = 0x36, // takes int32_t
  MXCFB_DISABLE_EPDC_ACCESS            = 0x35,
  MXCFB_ENABLE_EPDC_ACCESS             = 0x36
} eink_ioctl_command;

typedef enum _update_mode
{
  UPDATE_MODE_PARTIAL   = 0,
  UPDATE_MODE_FULL      = 1
} update_mode;

typedef enum _waveform_mode {
  WAVEFORM_MODE_INIT         = 0x0,	                 /* Screen goes to white (clears) */
  WAVEFORM_MODE_GLR16			   = 0x4,                  /* Basically A2 (so partial refresh shouldnt be possible here) */
  WAVEFORM_MODE_GLD16			   = 0x5,                  /* Official -- and enables Regal D Processing */

  // Unsupported?
  WAVEFORM_MODE_DU           = 0x1,	                 /* [Direct Update] Grey->white/grey->black  -- remarkable uses this for drawing */
  WAVEFORM_MODE_GC16         = 0x2,	                 /* High fidelity (flashing) */
  WAVEFORM_MODE_GC4          = WAVEFORM_MODE_GC16,   /* For compatibility */
  WAVEFORM_MODE_GC16_FAST    = 0x3,                  /* Medium fidelity  -- remarkable uses this for UI */
  WAVEFORM_MODE_GL16_FAST    = 0x6,                  /* Medium fidelity from white transition */
  WAVEFORM_MODE_DU4          = 0x7,	                 /* Medium fidelity 4 level of gray direct update */
  WAVEFORM_MODE_REAGL	       = 0x8,	                 /* Ghost compensation waveform */
  WAVEFORM_MODE_REAGLD       = 0x9,	                 /* Ghost compensation waveform with dithering */
  WAVEFORM_MODE_GL4		       = 0xA,	                 /* 2-bit from white transition */
  WAVEFORM_MODE_GL16_INV		 = 0xB,	                 /* High fidelity for black transition */
  WAVEFORM_MODE_AUTO			   = 257                   /* Official */
} waveform_mode;

typedef enum _display_temp {
  TEMP_USE_AMBIENT           = 0x1000,
  TEMP_USE_PAPYRUS           = 0X1001,
  TEMP_USE_REMARKABLE_DRAW   = 0x0018,
  TEMP_USE_MAX               = 0xFFFF
} display_temp;

typedef struct {
  uint32_t top;
  uint32_t left;
  uint32_t width;
  uint32_t height;
} mxcfb_rect;

typedef struct {
	uint32_t update_marker;
	uint32_t collision_test;
} mxcfb_update_marker_data;

typedef struct {
	uint32_t phys_addr;
	uint32_t width;                   /* width of entire buffer */
	uint32_t height;	                /* height of entire buffer */
	mxcfb_rect alt_update_region;	    /* region within buffer to update */
} mxcfb_alt_buffer_data;

typedef struct {
	mxcfb_rect update_region;
  uint32_t waveform_mode;

  // Choose between FULL and PARTIAL
  uint32_t update_mode;

  // Checkpointing
  uint32_t update_marker;

  int temp;                         // 0x1001 = TEMP_USE_PAPYRUS
  unsigned int flags;               // 0x0000


  /*
   * Dither mode is entirely unused since the following means v1 is used not v2
   *
   * arch/arm/configs/zero-gravitas_defconfig
      173:CONFIG_FB_MXC_EINK_PANEL=y

     firmware/Makefile
      68:fw-shipped-$(CONFIG_FB_MXC_EINK_PANEL) += \

     drivers/video/fbdev/mxc/mxc_epdc_fb.c
      4969:#ifdef CONFIG_FB_MXC_EINK_AUTO_UPDATE_MODE
      5209:#ifdef CONFIG_FB_MXC_EINK_AUTO_UPDATE_MODE

     drivers/video/fbdev/mxc/mxc_epdc_v2_fb.c
      5428:#ifdef CONFIG_FB_MXC_EINK_AUTO_UPDATE_MODE
      5662:#ifdef CONFIG_FB_MXC_EINK_AUTO_UPDATE_MODE

     drivers/video/fbdev/mxc/Makefile
      10:obj-$(CONFIG_FB_MXC_EINK_PANEL)      += mxc_epdc_fb.o
      11:obj-$(CONFIG_FB_MXC_EINK_V2_PANEL)   += mxc_epdc_v2_fb.o
   *
   */
  int dither_mode;
	int quant_bit; // used only when dither_mode is > PASSTHROUGH and < MAX

  mxcfb_alt_buffer_data alt_buffer_data;  // not used when flags is 0x0000
} mxcfb_update_data;


// end from libremarkable/legacy-c-impl/libremarkable/lib.h

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
            WAVEFORM_MODE_GL16_FAST,                    //waveform,
            UPDATE_MODE_PARTIAL, // UPDATE_MODE_FULL,       // mode,
            0x2a, // marker,
            0x0018, //TEMP_USE_AMBIENT, // temp,
            0,  // flags
            0, //dither_mode,
            0, // quant_bit,
            mxcfb_alt_buffer_data{}
        };

        int status;
        if( status = ioctl( device, REMARKABLE_PREFIX(MXCFB_SEND_UPDATE), &whole))
            throw "FrameBuffer: failed to MXCFB_SEND_UPDATE: " + std::to_string(status) 
                    + " (errno= " + std::to_string(errno) + ") \n";
    }

    // fast refresh of selected area
    void refresh( const Rect& r) const {
        mxcfb_update_data whole{
            mxcfb_rect{ r.topLeft.y, r.topLeft.x, r.width(), r.height()},
            WAVEFORM_MODE_DU,                    //waveform,
            UPDATE_MODE_PARTIAL, // UPDATE_MODE_FULL,       // mode,
            0x2a, // marker,
            0x0018, //TEMP_USE_AMBIENT, // temp,
            0,  // flags
            0, //dither_mode,
            0, // quant_bit,
            mxcfb_alt_buffer_data{}
        };

        int status;
        if( status = ioctl( device, REMARKABLE_PREFIX(MXCFB_SEND_UPDATE), &whole))
            throw "FrameBuffer: failed to MXCFB_SEND_UPDATE: " + std::to_string(status) 
                    + " (errno= " + std::to_string(errno) + ") \n";
    }

    // animation: fast refresh of selected areas
    void refresh( const Rect& r1, const Rect& r2) {
        refresh( r1);
        refresh( r2);
    }
};
