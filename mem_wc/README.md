# mem_wc — write-combining `/dev/mem`

### What follows is ai slop

A small char driver that exposes `/dev/mem_wc`, an mmap-able device that maps
arbitrary physical memory into userspace with a **Normal Non-Cacheable
(write-combining)** attribute, instead of the **Strongly-Ordered / Device**
attribute a plain `/dev/mem` mmap gets on ARM.


## Ordering caveat

Write-combining removes the implicit ordering Device memory gave you. The CPU is
still self-consistent (your own reads see your own writes), but **other bus
masters (the FPGA) do not see posted writes until they drain.** Before signalling
the FPGA that a list/frame is ready, issue a barrier in the writer:

```c
asm volatile("dsb sy" ::: "memory");
```

## Usage

Identical to `/dev/mem` — just open the WC node instead:

```c
int fd = open("/dev/mem_wc", O_RDWR);
void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_base);
```


## Build

Requires the MiSTer 5.15.1 kernel source prepared for out-of-tree modules,
built with the same Arm 10.2-2020.11 toolchain the kernel used
(`arm-none-linux-gnueabihf-gcc 10.2.1`, per `/proc/version`).

```sh
# 1. Get the matching kernel source (correlate by build date Nov 8 2023 / config)
git clone https://github.com/MiSTer-devel/Linux-Kernel_MiSTer
cd Linux-Kernel_MiSTer
# check out the commit that produces 5.15.1-MiSTer

# 2. Prepare it with the device's own config so vermagic matches
zcat /proc/config.gz > .config          # from the DE10-Nano
export ARCH=arm
export CROSS_COMPILE=/opt/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
make olddefconfig
make modules_prepare

# 3. Build the module
cd /path/to/mem_wc
make KDIR=/path/to/Linux-Kernel_MiSTer
```

Produces `mem_wc.ko`.

The running kernel has `CONFIG_MODVERSIONS` off and `CONFIG_MODULE_SIG` off, so
no symbol-CRC matching and no signing are needed — only the vermagic string must
match (guaranteed by building against the same source + `.config`).

## Load

```sh
insmod mem_wc.ko

dmesg | tail                                        # confirm it loaded
ls -l /dev/mem_wc
```
