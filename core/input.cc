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


struct Event {
    timeval time;       // time of events
    int device;         // device from which the event is coming
    int tool;           // tool detected / or button

    union {
        struct {
            Point pos;          // position of event
        };
    };

};

// event + loop support
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
    };

    // Event loop
    // call lambda on every event gathered
    template<class Fn>
    void loop(Fn callback) {
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

                        readEvent( pfd.fd, callback);
                    }
                }
            }
        }
    }

    // convert linux input_event to Event
    // and call calback when Event is completly defined
    template<class Fn>
    void readEvent( int fd, Fn callback) {
        Event res;

        // let's assume we get a complete set of linux event in buffer each time
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

            switch( event.type) {
                default:
                    cerr << "Unknown event type=" << event.type;
                    break;

                case EV_SYN:
                    // cerr << " " << ::to_s( event.code, syn_code, "code");
                    callback( res);
                    break;                

                case EV_KEY:       // hardware button + pen type
                    // cerr << " " << ::to_s( event.code, btn_code, "code");
                    break;

                case EV_ABS:        // pen + multitouch
                    ABS_event( res, event);
                    break;
            }
        }
    }

#define WACOMWIDTH 15725.0
#define WACOMHEIGHT 20967.0
#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872.0
#define WACOM_X_SCALAR (float(DISPLAYWIDTH) / float(WACOMWIDTH))
#define WACOM_Y_SCALAR (float(DISPLAYHEIGHT) / float(WACOMHEIGHT))

    //
    void ABS_event( Event& res, const input_event& event ) {
        switch( event.code ) {
            default: 
                cerr << "Unknown event code=" << event.code;
                break;

            case ABS_X: res.pos.x = event.value * WACOM_X_SCALAR;    break;
            case ABS_Y: res.pos.y = event.value * WACOM_Y_SCALAR;    break;

        } 
    }
};