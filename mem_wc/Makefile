# Out-of-tree build for the mem_wc write-combining /dev/mem driver.
#
# Build against the MiSTer 5.15.1 kernel source, cross-compiled with the same
# Arm 10.2-2020.11 toolchain the kernel itself was built with (per /proc/version).
#
#   make KDIR=/path/to/Linux-Kernel_MiSTer
#
# KDIR must be a kernel tree that has had `make modules_prepare` run against the
# device's /proc/config.gz, so vermagic matches (5.15.1-MiSTer SMP ...).

obj-m := mem_wc.o

# Kernel source / build tree. Override on the command line as needed.
KDIR ?= /lib/modules/$(shell uname -r)/build

ARCH          ?= arm
CROSS_COMPILE ?= /opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

.PHONY: all clean
