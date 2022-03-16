#makefile for kRtspProxyd
KDIR=/lib/modules/$(shell uname -r)/build

#CC=gcc
#CFLAGS= -D__KERNEL__ -DMODULE -DEXPORT_SYMTAB -DMODVERSIONS \
	$(SMPFLAGS) $(DEBUGFLAGS) -O2 -Wall \
	-Wstrict-prototypes -I/usr/src/linux/includeS
	#-include /usr/src/linux/include/linux/modversions.h

obj-m += krtspproxyd.o

krtspproxyd-objs := \
        accept.o \
        dataswitching.o \
        misc.o \
        security.o \
        sockets.o \
        sysctl.o \
        wait4sessionprocess.o \
        proxy.o \
        main.o

all :
	 make -C $(KDIR) M=$(PWD) modules

#all:	krtspproxyd.o

#krtspproxyd.o:	main.o accept.o dataswitching.o misc.o security.o\
		sockets.o sysctl.o wait4sessionprocess.o proxy.o

#	$(LD) -r $^ -o $@

install:	krtspproxyd.o
	-rmmod krtspproxyd
	insmod krtspproxyd.o
	lsmod

clean:
	rm -f *.o *~ *.bak *.orig *.rej

