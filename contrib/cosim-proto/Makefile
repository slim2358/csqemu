#
# Makefile to build a very simple COSIM prototype
#

CROSS_COMPILE      = $(RISCV_GCC)

# CFLAGS             = -mcmodel=medany -ffunction-sections -fdata-sections
# LDFLAGS            = -nostartfiles -nostdlib -nostdinc -static -lgcc \
                     -Wl,--nmagic -Wl,--gc-sections

# INCLUDES           = -I../common

#CC_gcc             = $(CROSS_COMPILE)gcc
#CC_gcc             = $(CROSS_COMPILE)
#CFLAGS_gcc         = -Os -march=rv64g_zicsr -mabi=lp64 -I../include
#LDFLAGS_gcc        = -T ../common/default.lds ../lib/crt.o ../lib/setup.o -L../lib -lfemto 

all: cosim

cosim: cosim.o
	gcc -o cosim cosim.o -ldl -L/home/slyubski/QEMU-rv/qemu-rv/csqemu/install-local/bin/qemu-system-riscv64.so -lpthread 

cosim.o: cosim.c
	gcc -c cosim.c -o cosim.o 
