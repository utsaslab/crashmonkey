TESTING := testing/log_on_off
obj-m := hellow.o
KDIR := /lib/modules/$(shell uname -r)/build
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_log_on_off: testing/log_on_off.c
	gcc testing/log_on_off.c -o testing/log_on_off

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(TESTING)
