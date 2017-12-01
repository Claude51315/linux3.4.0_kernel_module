KERNEL_DIR=/home/esslab/android/msm
obj-m := trace.o

trace-objs := trace_main.o sha1.o crc.o adler.o

PWD := $(shell pwd)

ARCH=arm
CROSS_COMPILE=/home/esslab/android/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-

EXTRA_CFLAGS=-fno-pic

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld

all:
	make -C $(KERNEL_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(pwd) clean
	rm -rf *.o .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers 
	
