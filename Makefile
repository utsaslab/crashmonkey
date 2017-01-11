TESTING := testing/log_on_off testing/test_get_log_ent_size
obj-m := hellow.o
KDIR := /lib/modules/$(shell uname -r)/build
GPP = g++
GOPTS = -std=c++11
GOTPSSO = -shared -fPIC
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_log_on_off: testing/log_on_off.c
	gcc testing/log_on_off.c -o testing/log_on_off

test_get_ent_size: testing/test_get_log_ent_size.c
	gcc testing/test_get_log_ent_size.c -o testing/test_get_log_ent_size

harness: c_harness.cpp default utils.h utils.cpp hellow_ioctl.h \
	tests_echo_sub_dir
	$(GPP) $(GOPTS) c_harness.cpp utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -ldl -o c_harness

tests_c_harness: tests/test_get_log_ent_size.c
	gcc tests/test_get_log_ent_size.c -o tests/test_get_log_ent_size

tests_echo_root_dir: tests/echo_root_dir.cpp
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,echo_root_dir.so \
		-o tests/echo_root_dir.so tests/echo_root_dir.cpp
	
tests_echo_sub_dir: tests/echo_sub_dir.cpp
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,echo_sub_dir.so \
		-o tests/echo_sub_dir.so tests/echo_sub_dir.cpp

permute: permute.cpp utils.h utils.cpp hellow_ioctl.h
	$(GPP) $(GOPTS) permute.cpp utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -o permute

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(TESTING) tests/*.so c_harness purmute
