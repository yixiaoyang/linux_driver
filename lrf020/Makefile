#
# xiaoyang.yi 2012-3-20
# linux driver for LRF020(uz2440d core)
#

CC=arm-linux-gcc
PWD:=$(shell pwd)
KERNELDIR=/opt/linux/EmbedSky/linux-2.6.30.4
INSTALLDIR=./
INC=$(KERNELDIR)/include #$(PWD)/*.h
MOD_NAME=lrf020
DEBUG=y

#obj-m	+=	rf.o
#obj-m	:=	uz2400d.0
obj-m 	:=	$(MOD_NAME).o
lrf020-objs	:=	rf.o uz2400d.o lrf020_main.o

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules -I $(INC)
	@echo "compiled ok!"
uz2400d.o:uz2400d.c
	$(CC) uz2400d.c -I $(INC)
rf.o:rf.c
	$(CC) rf.c -I $(INC)
lrf020_main.o:lrf020.c
	$(CC) lrf020_main.c -I $(INC)
debug:
	cat /var/log/messages | tail
result:
	cat /proc/devices
help:
	@echo "install with 'mknod /dev/devname c dev_num 0' cmd"
clean:
	rm -rf *.o *.ko *.mod.c *.markers *.order *.symvers
