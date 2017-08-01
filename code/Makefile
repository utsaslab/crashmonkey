TESTING := testing/log_on_off testing/test_get_log_ent_size
MODULES = cow_brd.c disk_wrapper.c
obj-m := $(addsuffix .o, $(basename $(MODULES)))
KDIR := /lib/modules/$(shell uname -r)/build
DEBUG_FLAGS += -g -DDEBUG
GCC = gcc
GPP = g++
GOPTS = -std=c++11
GOTPSSO = -shared -fPIC
UTILS_OBJS = utils/utils_c.o utils/utils.o
BUILD_DIR = ../build

default: cow_brd.ko disk_wrapper.ko $(UTILS_OBJS) $(BUILD_DIR)/harness \
		permuter/RandomPermuter.so user_tools

cow_brd.ko disk_wrapper.ko: $(MODULES)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(BUILD_DIR)/harness: harness/c_harness.cpp harness/Tester.cpp \
		$(UTILS_OBJS) $(BUILD_DIR)/utils/communication/ServerSocket.o \
		$(BUILD_DIR)/utils/communication/ClientSocket.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o
	$(GPP) $(GOPTS) $^ -ldl -o $(BUILD_DIR)/$@

tests/%.so: tests/$(addsuffix .cpp, %) tests/BaseTestCase.h
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,$(notdir $@) \
		-o $@ $<
	
permuter/RandomPermuter.so: permuter/RandomPermuter.cpp permuter/RandomPermuter.h \
		disk_wrapper_ioctl.h $(UTILS_OBJS)
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,RandomPermuter.so \
		-o permuter/RandomPermuter.so permuter/RandomPermuter.cpp $(UTILS_OBJS)

utils/utils.o: utils/utils.h utils/utils.cpp disk_wrapper_ioctl.h
	$(GPP) $(GOPTS) -fPIC -o utils/utils.o -c utils/utils.cpp

utils/utils_c.o: utils/utils_c.h utils/utils_c.c disk_wrapper_ioctl.h
	$(GCC) -o utils/utils_c.o -c utils/utils_c.c \
		-I /usr/src/linux-headers-$(shell uname -r)/include/

user_tools/%: user_tools/$(addsuffix .cpp, %) \
		$(BUILD_DIR)/utils/communication/ClientSocket.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o \
		$(BUILD_DIR)/utils/communication/ClientCommandSender.o
	mkdir -p $(BUILD_DIR)/$(dir $@)
	$(GPP) $(GOPTS) -o $(BUILD_DIR)/$(dir $@)/$* $^

#user_tools: user_tools/begin_setup user_tools/end_setup user_tools/begin_log
user_tools: user_tools/begin_log user_tools/end_log user_tools/begin_tests

$(BUILD_DIR)/utils/communication/%.o: utils/communication/%.cpp
	mkdir -p $(dir $@)
	$(GPP) -c $(GOPTS) -o $@ $<

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
	rm -f $(TESTING) tests/*.so c_harness permute *.o *.so utils/*.o permuter/*.so \
		ioctl_test
	rm -rf $(BUILD_DIR)/*
