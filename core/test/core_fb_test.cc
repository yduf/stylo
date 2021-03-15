#include <iostream>
#include "../fb.cc"

using namespace std;

int main(int argc,char** argv) {
try {
    cerr << "Hello World" << endl;
    auto fb = FrameBuffer::open();
    fb.to_s();

    fb.fill( 0x00);
    fb.refresh();

    sleep( 4);

    fb.fill( 0xFF);
    fb.refresh();

    cerr << "done" << endl;
}
catch(const char* msg) {
    cerr << "ERROR : " << msg << endl;
}
catch(const string& msg) {
    cerr << "ERROR : " << msg << endl;
}
}