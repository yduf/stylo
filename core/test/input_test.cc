#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>

#include <iostream>
#include <array>

using namespace std;

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
    int pen;

public:
    Input( const char* pen_device = "/dev/input/event1" ) {
        pen = ::open( pen_device, O_RDONLY);

        // define event name
        ev_type[EV_SYN] = "EV_SYN";
        ev_type[EV_KEY] = "EV_KEY";
        ev_type[EV_ABS] = "EV_ABS";

        // SYN code
        syn_code[SYN_REPORT] = "SYN_REPORT";

        // define ABS code
        abs_code[ABS_X] = "ABS_X";
        abs_code[ABS_Y] = "ABS_Y";
        abs_code[ABS_DISTANCE] = "ABS_DISTANCE";
        abs_code[ABS_PRESSURE] = "ABS_PRESSURE";
        abs_code[ABS_TILT_X] = "ABS_TILT_X";
        abs_code[ABS_TILT_Y] = "ABS_TILT_Y";

        // btn code
        btn_code[BTN_TOOL_PEN]    = "BTN_TOOL_PEN";
        btn_code[BTN_TOOL_RUBBER] = "BTN_TOOL_RUBBER";

        for(;;) {
            input_event event;
            ::read( pen, &event, sizeof( event));
            cerr << "event at " << event.time.tv_sec << ":" << event.time.tv_usec 
                    << " " << ::to_s( event.type, ev_type, "type");

            switch( event.type) {
                default:
                    cerr << " code=" << event.code;
                    break;

                case EV_SYN:
                    cerr << " " << ::to_s( event.code, syn_code, "code");
                    break;                

                case EV_KEY:
                    cerr << " " << ::to_s( event.code, btn_code, "code");
                    break;

                case EV_ABS:
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