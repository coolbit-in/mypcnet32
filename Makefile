PWD = $(shell pwd)
KERNEL_SRC = /usr/src/linux-2.6.25.14

obj-m := mypcnet32.o
module-objs := mypcnet32.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules
clean:
	rm -f *.o
	rm -f *.ko
	rm -f modules.*
	rm -f Module.*
install:
	rmmod pcnet32
	insmod ./mypcnet32.ko
uninstall:
	rmmod mypcnet32
	modprobe pcnet32
