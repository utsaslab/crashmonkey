TESTING := testing/log_on_off testing/test_get_log_ent_size
MODULES = cow_brd.c disk_wrapper.c
obj-m := $(addsuffix .o, $(basename $(MODULES)))
KDIR := /lib/modules/$(shell uname -r)/build
DEBUG_FLAGS += -g -DDEBUG
GPP = g++
GOPTS = -std=c++11
GOTPSSO = -shared -fPIC

cow_brd.ko disk_wrapper.ko default: $(MODULES)
	$(MAKE) -C $(KDIR) M=$(PWD) modules EXTRA_CFLAGS="$(DEBUG_FLAGS)"
	sudo cp $(addsuffix .ko, $(basename $(MODULES))) \
		/lib/modules/$(shell uname -r)/custom/

harness: harness/c_harness.cpp default utils/utils.h utils/utils.cpp \
		disk_wrapper_ioctl.h harness/Tester.cpp harness/Tester.h \
		permuter/Permuter.h permuter/RandomPermuter.so
	$(GPP) $(GOPTS) harness/c_harness.cpp harness/Tester.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -ldl -o c_harness

tests/%.so: tests/$(addsuffix .cpp, %) tests/BaseTestCase.h
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,$(notdir $@) \
		-o $@ $<
	
permuter/RandomPermuter.so: permuter/RandomPermuter.cpp \
		permuter/RandomPermuter.h utils/utils.h  utils/utils.cpp  \
		disk_wrapper_ioctl.h
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,RandomPermuter.so \
		-o permuter/RandomPermuter.so permuter/RandomPermuter.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/

run_no_log: harness tests/echo_sub_dir.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 \
		-e 10240 tests/echo_sub_dir.so

run_no_log_huge: harness tests/echo_sub_dir_huge.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 \
		-e 15360 -s $(NUM_TESTS) tests/echo_sub_dir_huge.so

run_no_log_big: harness tests/echo_sub_dir_big.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 \
		-e 10240 -s $(NUM_TESTS) tests/echo_sub_dir_big.so

run_no_log_direct: harness tests/sub_dir_odirect.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 \
		-e 10240 -s $(NUM_TESTS) tests/sub_dir_odirect.so

run_no_log_mmap: harness tests/sub_dir_mmap.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 \
		-e 10240 -s $(NUM_TESTS) tests/sub_dir_mmap.so

run_no_log_no_sync: harness tests/echo_sub_dir_no_sync.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 \
		-e 10240 tests/echo_sub_dir_no_sync.so

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(TESTING) tests/*.so c_harness permute *.o *.so permuter/*.so \
		ioctl_test
