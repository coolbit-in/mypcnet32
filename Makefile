PWD = $(shell pwd)
KERNEL_SRC = /usr/src/linux-2.6.25.14

obj-m := mypcnet32.o
module-objs := mypcner32.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

