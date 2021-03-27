#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>
#include <poll.h>

#include <iostream>
#include <array>
#include <vector>

using namespace std;

#include <filesystem>
namespace fs = std::filesystem;


// from https://github.com/rmkit-dev/rmkit/blob/master/src/rmkit/input/device_id.cpy
// glommed from evtest.c
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)


auto check_bit_set(int fd, int type, int i) {
    unsigned long bit[NBITS(KEY_MAX)];
    ioctl(fd, EVIOCGBIT(type, KEY_MAX), bit);
    return test_bit(i, bit);
}

std::string id_by_capabilities(int fd) {
    int version;
    // if we can't get version of the fd, its invalid
    if( ioctl(fd, EVIOCGVERSION, &version))
        return "INVALID";

    unsigned long bit[EV_MAX];
    ioctl(fd, EVIOCGBIT(0, EV_MAX), bit);
    if( test_bit(EV_KEY, bit)) {
        if( check_bit_set(fd, EV_KEY, BTN_STYLUS) 
            && test_bit(EV_ABS, bit))
            return "STYLUS";

        if( check_bit_set(fd, EV_KEY, KEY_POWER))
            return "BUTTONS";

        if( test_bit(EV_REL, bit)
            && check_bit_set(fd, EV_ABS, ABS_MT_SLOT))
            return "TOUCH";
    }

    return "UNKNOWN";
}
// end from https://github.com/rmkit-dev/rmkit/blob/master/src/rmkit/input/device_id.cpy


#define SYM(element) [element] = #element

array<const char*, EV_CNT> ev_type = {};
array<const char*, SYN_CNT> syn_code = {};
array<const char*, ABS_CNT> abs_code = {};
array<const char*, KEY_CNT> btn_code = {};

template< class T, size_t N>
auto to_s( int value, const array<T,N>& label, const string& type) {
    return value < N ?
        ( label[value] ? label[value]
                       : type + "=" + to_string( value))
        : type + "=" + to_string( value)
        ;
}

class Input {
    vector<pollfd> devices;

public:
    Input() {

        cerr << "scanning input devices" << endl;
        for(auto& p: fs::directory_iterator("/dev/input")) {
            if( !p.is_symlink() && p.is_character_file() ) {
                int dev = ::open( p.path().c_str(), O_RDONLY | O_NONBLOCK );

                std::cerr << p.path() << " " << id_by_capabilities(dev) << endl;

                pollfd pol = { dev, POLLIN };
                devices.push_back( pol);
            }
        }
        cerr << "end" << endl;

        // define event name
        ev_type[EV_SYN] = "EV_SYN";
        ev_type[EV_KEY] = "EV_KEY";
        ev_type[EV_ABS] = "EV_ABS";

        // SYN code
        syn_code[SYN_REPORT] = "SYN_REPORT";

        // define ABS code
        // pen
        abs_code[ABS_X] = "ABS_X";
        abs_code[ABS_Y] = "ABS_Y";
        abs_code[ABS_DISTANCE] = "ABS_DISTANCE";
        abs_code[ABS_PRESSURE] = "ABS_PRESSURE";
        abs_code[ABS_TILT_X] = "ABS_TILT_X";
        abs_code[ABS_TILT_Y] = "ABS_TILT_Y";

        // btn code
        btn_code[BTN_TOOL_PEN]    = "BTN_TOOL_PEN";
        btn_code[BTN_TOOL_RUBBER] = "BTN_TOOL_RUBBER";

        //
        btn_code[KEY_POWER] = "KEY_POWER";

        // multitouch
        abs_code[ABS_MT_SLOT] = "ABS_MT_SLOT";
        abs_code[ABS_MT_TOUCH_MAJOR] = "ABS_MT_TOUCH_MAJOR";
        abs_code[ABS_MT_TOUCH_MINOR] = "ABS_MT_TOUCH_MINOR";
        abs_code[ABS_MT_WIDTH_MAJOR] = "ABS_MT_WIDTH_MAJOR";
        abs_code[ABS_MT_WIDTH_MINOR] = "ABS_MT_WIDTH_MINOR";
        abs_code[ABS_MT_ORIENTATION] = "ABS_MT_ORIENTATION";
        abs_code[ABS_MT_POSITION_X] = "ABS_MT_POSITION_X";
        abs_code[ABS_MT_POSITION_Y] = "ABS_MT_POSITION_Y";
        abs_code[ABS_MT_TOOL_TYPE] = "ABS_MT_TOOL_TYPE";
        abs_code[ABS_MT_BLOB_ID] = "ABS_MT_BLOB_ID";
        abs_code[ABS_MT_TRACKING_ID] = "ABS_MT_TRACKING_ID";
        abs_code[ABS_MT_PRESSURE] = "ABS_MT_PRESSURE";


        for(;;) {
            int ret = poll( &devices[0], devices.size(), 10000 );
            // Check if poll actually succeed
            if ( ret == -1 ) {
                // report error and abort
                cerr << "poll failed" << endl;
                break;
            }
            else if ( ret == 0 ) {
                // timeout; no event detected
                cerr << "poll timeout" << endl;
                continue;
            }
            else {
                // If we detect the event, zero it out so we can reuse the structure
                for( int i = 0; i < devices.size(); ++i) {
                    auto& pfd = devices[i];

                    if( pfd.revents & POLLIN ) {  // input event on device
                        pfd.revents = 0;
                        cerr << "event on device " << i << endl;

                        read_event( pfd.fd);
                    }
                }
            }
        }
    }

    void read_event( int fd) {
        for(;;) {
            input_event event;
            int nbytes = ::read( fd, &event, sizeof( event));
            if( nbytes == sizeof(event)) {
                // ok
            }
            else {
                if (errno != EWOULDBLOCK) {
                    /* real version needs to handle EINTR correctly */
                        perror("read");
                        exit(EXIT_FAILURE);
                }

                return;     // enf of buffer
            }


            cerr << "event at " << event.time.tv_sec << ":" << event.time.tv_usec 
                    << " " << ::to_s( event.type, ev_type, "type");

            switch( event.type) {
                default:
                    cerr << " code=" << event.code;
                    break;

                case EV_SYN:
                    cerr << " " << ::to_s( event.code, syn_code, "code");
                    break;                

                case EV_KEY:       // hardware button + pen type
                    cerr << " " << ::to_s( event.code, btn_code, "code");
                    break;

                case EV_ABS:        // pen + multitouch
                    cerr << " " << ::to_s( event.code, abs_code, "code");
                    break;
            }
                    
            cerr 
                    << " value=" << event.value 
                    << endl;
        }
    }
};

int main(int argc,char** argv) {
try {

    // open wacom device
    cerr << "Hehllo" << endl;
    Input in;

}
catch(const char* msg) {
    cerr << "ERROR : " << msg << endl;
}
catch(const string& msg) {
    cerr << "ERROR : " << msg << endl;
}
}