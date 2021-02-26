CXX = arm-linux-gnueabihf-g++
CFLAGS ?= -fPIC -g -static -std=gnu++17 -Werror=return-type

DEVICE_IP ?= '10.11.99.1'
DEVICE_HOST ?= root@$(DEVICE_IP)

core:

build: core
	$(CXX)  $(CFLAGS) core/fb.cc -o fb_test

deploy: build
	scp ./fb_test $(DEVICE_HOST):
	ssh $(DEVICE_HOST) '/opt/bin/rm2fb-client ./fb_test'