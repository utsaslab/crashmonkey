TESTING := testing/log_on_off testing/test_get_log_ent_size
obj-m := disk_wrapper.o cow_brd.o
KDIR := /lib/modules/$(shell uname -r)/build
DEBUG_FLAGS += -g -DDEBUG
GPP = g++
GOPTS = -std=c++11
GOTPSSO = -shared -fPIC
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules EXTRA_CFLAGS="$(DEBUG_FLAGS)"
	sudo cp $(addsuffix .ko, $(basename $(obj-m))) \
		/lib/modules/$(shell uname -r)/custom/

test_log_on_off: testing/log_on_off.c
	gcc testing/log_on_off.c -o testing/log_on_off

test_get_ent_size: testing/test_get_log_ent_size.c
	gcc testing/test_get_log_ent_size.c -o testing/test_get_log_ent_size

harness: harness/c_harness.cpp default utils/utils.h utils/utils.cpp disk_wrapper_ioctl.h \
	harness/Tester.cpp harness/Tester.h tests/echo_sub_dir.so tests/BaseTestCase.h \
	permuter/Permuter.h  permuter/RandomPermuter.so
	$(GPP) $(GOPTS) harness/c_harness.cpp harness/Tester.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -ldl -o c_harness

harness_huge: harness/c_harness.cpp default utils/utils.h utils/utils.cpp disk_wrapper_ioctl.h \
	harness/Tester.cpp harness/Tester.h tests/echo_sub_dir_huge.so tests/BaseTestCase.h \
	permuter/Permuter.h  permuter/RandomPermuter.so
	$(GPP) $(GOPTS) harness/c_harness.cpp harness/Tester.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -ldl -o c_harness

harness_big: harness/c_harness.cpp default utils/utils.h utils/utils.cpp disk_wrapper_ioctl.h \
	harness/Tester.cpp harness/Tester.h tests/echo_sub_dir_big.so tests/BaseTestCase.h \
	permuter/Permuter.h  permuter/RandomPermuter.so
	$(GPP) $(GOPTS) harness/c_harness.cpp harness/Tester.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -ldl -o c_harness

tests_c_harness: tests/test_get_log_ent_size.c
	gcc tests/test_get_log_ent_size.c -o tests/test_get_log_ent_size

tests_echo_root_dir: tests/echo_root_dir.cpp
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,echo_root_dir.so \
		-o tests/echo_root_dir.so tests/echo_root_dir.cpp
	
tests/echo_sub_dir.so: tests/echo_sub_dir.cpp
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,echo_sub_dir.so \
		-o tests/echo_sub_dir.so tests/echo_sub_dir.cpp

tests/echo_sub_dir_big.so: tests/echo_sub_dir_big.cpp
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,echo_sub_dir_big.so \
		-o tests/echo_sub_dir_big.so tests/echo_sub_dir_big.cpp

tests/echo_sub_dir_huge.so: tests/echo_sub_dir_huge.cpp
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,echo_sub_dir_huge.so \
		-o tests/echo_sub_dir_huge.so tests/echo_sub_dir_huge.cpp

permute: permute.cpp utils/utils.h utils/utils.cpp disk_wrapper_ioctl.h
	$(GPP) $(GOPTS) permute.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/ -o permute

permuter/RandomPermuter.so: permuter/RandomPermuter.cpp \
	permuter/RandomPermuter.h utils/utils.h  utils/utils.cpp  disk_wrapper_ioctl.h
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,RandomPermuter.so \
		-o permuter/RandomPermuter.so permuter/RandomPermuter.cpp utils/utils.cpp \
		-I /usr/src/linux-headers-$(shell uname -r)/include/

run_no_log: harness
	sudo insmod /lib/modules/$(shell uname -r)/kernel/drivers/block/brd.ko \
		rd_size=524288 rd_nr=1
	sudo ./c_harness -f /dev/vda -t ext4 -m barrier -d /dev/ram0 -i \
		tests/echo_sub_dir.so
	sudo rmmod brd

run_norm: harness
	sudo insmod /lib/modules/$(shell uname -r)/kernel/drivers/block/brd.ko \
		rd_size=524288 rd_nr=1
	sudo ./c_harness -f /dev/vda -t ext4 -m barrier -d /dev/ram0 -i \
		-l /home/ashmrtn/research/harness_profiles/$(LOG) tests/echo_sub_dir.so
	sudo rmmod brd

run_norm_log: harness
	sudo insmod /lib/modules/$(shell uname -r)/kernel/drivers/block/brd.ko \
		rd_size=524288 rd_nr=1
	sudo ./c_harness -f /dev/vda -t ext4 -m barrier -d /dev/ram0 -i \
		-r /home/ashmrtn/research/harness_profiles/$(LOG) tests/echo_sub_dir.so
	sudo rmmod brd

brd_test:
	sudo insmod /lib/modules/$(shell uname -r)/kernel/drivers/block/brd.ko \
		rd_size=524288 rd_nr=1
	sudo fdisk -l /dev/ram0
	sudo rmmod brd

cow_brd:
	sudo insmod cow_brd.ko rd_size=524288 rd_nr=1
	sudo fdisk -l /dev/ram0
	sudo rmmod cow_brd

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(TESTING) tests/*.so c_harness permute *.o *.so permuter/*.so
