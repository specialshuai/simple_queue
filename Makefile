KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)
 
obj-m := simple_queue.o
default:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules
 
test: test.c
	gcc $< -o $@.o  -g
 
clean:
	rm -rf *.o *.ko *~ *.order *.symvers *.markers *.mod.c

