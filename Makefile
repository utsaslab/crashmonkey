TESTING := testing/log_on_off testing/test_get_log_ent_size
obj-m := hellow.o
KDIR := /lib/modules/$(shell uname -r)/build
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_log_on_off: testing/log_on_off.c
	gcc testing/log_on_off.c -o testing/log_on_off

test_get_ent_size: testing/test_get_log_ent_size.c
	gcc testing/test_get_log_ent_size.c -o testing/test_get_log_ent_size

harness: c_harness.c
	gcc c_harness.c -o c_harness

tests_c_harness: tests/test_get_log_ent_size.c
	gcc tests/test_get_log_ent_size.c -o tests/test_get_log_ent_size

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(TESTING)
