Android Kernel Module
===

This kernel module is compiled to support linux kernel v3.4.0 used by Google Nexus4.

# Usage

1. check kernel source tree and cross compiler path first!
2. the kernel should be compiled with loadable module support
3. `$ make` 
4. `$ adb pull hello.o /path/to/put/the/module`
5. `$ adb shell insmod /path/to/put/the/module`
6. `$ adb shell rmmod /path/to/put/the/module`

