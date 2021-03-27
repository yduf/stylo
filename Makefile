CXX = arm-linux-gnueabihf-g++
# DO NOT USE -static => it prevent rm2fb-client patching
CFLAGS ?= -fPIC -g --std=gnu++17 -Werror=return-type

DEVICE_IP ?= '10.11.99.1'
DEVICE_HOST ?= root@$(DEVICE_IP)

 
CAIRO ?= /home/yves/DEV/reMarkable/cairo
CAIROLIB ?= $(CAIRO)/build-arm

LDFLAGS ?= -L $(CAIROLIB)/src/ -lstdc++fs


core:

build: core
	$(CXX)  $(CFLAGS) core/test/core_fb_test.cc   -o fb_test
	$(CXX)  $(CFLAGS) core/test/input_test.cc     -o input_test $(LDFLAGS) 
#	$(CXX)  $(CFLAGS) -I $(CAIRO)/src -I $(CAIROLIB)/src core/test/animation_test.cc -l cairo -o animation_test $(LDFLAGS) 

deploy-lib:
	scp $(CAIROLIB)/src/libcairo.so.2 $(DEVICE_HOST):
	scp $(CAIROLIB)/subprojects/pixman/pixman/libpixman-1.so.0 $(DEVICE_HOST):

deploy: build
	scp ./animation_test $(DEVICE_HOST):
	ssh -t $(DEVICE_HOST) 'LD_LIBRARY_PATH=$LD_LIBRARY_PATH:. /opt/bin/rm2fb-client ./animation_test'


input_test: build
	scp ./input_test $(DEVICE_HOST):
	ssh -t $(DEVICE_HOST) './input_test'

