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
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 -i \
		tests/echo_sub_dir.so

run_no_log_no_sync: harness tests/echo_sub_dir_no_sync.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 -i \
		tests/echo_sub_dir_no_sync.so

run_norm: harness tests/echo_sub_dir.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 -i \
		-l /home/ashmrtn/research/harness_profiles/$(LOG) tests/echo_sub_dir.so

run_norm_log: harness tests/echo_sub_dir.so
	sudo ./c_harness -f /dev/sda -t ext4 -m barrier -d /dev/cow_ram0 -i \
		-r /home/ashmrtn/research/harness_profiles/$(LOG) tests/echo_sub_dir.so

brd_test:
	sudo insmod /lib/modules/$(shell uname -r)/kernel/drivers/block/brd.ko \
		rd_size=524288 rd_nr=1
	sudo fdisk -l /dev/ram0
	sudo rmmod brd

cow_brd_test: cow_brd.ko ioctl_test
	sudo insmod cow_brd.ko disk_size=512000 num_disks=1 num_snapshots=1
	sudo mkfs -t ext4 /dev/cow_ram0
	-sudo mount /dev/cow_ram0 /mnt/snapshot
	-echo "hello world" | sudo tee /mnt/snapshot/bleh > /dev/null
	-sync
	-sudo fsck /dev/cow_ram_snapshot1_0
	-cat /mnt/snapshot/bleh
	-sudo mount /dev/cow_ram_snapshot1_0 /mnt/test_dir
	-cat /mnt/test_dir/bleh
	-echo "hi world" | sudo tee -a /mnt/test_dir/bleh > /dev/null
	-cat /mnt/test_dir/bleh
	-cat /mnt/snapshot/bleh
	-echo "hi world" | sudo tee /mnt/test_dir/bleh > /dev/null
	-cat /mnt/test_dir/bleh
	-cat /mnt/snapshot/bleh
	-sudo umount /mnt/test_dir
	-sudo ./ioctl_test
	-sudo mount /dev/cow_ram_snapshot1_0 /mnt/test_dir
	-cat /mnt/test_dir/bleh
	-sudo umount /mnt/test_dir
	-sudo umount /mnt/snapshot
	sudo rmmod cow_brd

ioctl_test: ioctl_test.c disk_wrapper_ioctl.h
	gcc $^ -o $@

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(TESTING) tests/*.so c_harness permute *.o *.so permuter/*.so \
		ioctl_test
