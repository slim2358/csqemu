# -*- Mode: makefile -*-
#
# This Makefile example is fairly independent from the main makefile
# so users can take and adapt it for their build. We only really
# include config-host.mak so we don't have to repeat probing for
# programs that the main configure has already done for us.
#

include config-host.mak

TOP_SRC_PATH = $(SRC_PATH)/../..
BUILD_DIR = $(PWD)

VPATH += $(SRC_PATH)

NAMES :=
NAMES += execlog
NAMES += hotblocks
NAMES += hotpages
NAMES += howvec
NAMES += lockstep
NAMES += hwprofile
NAMES += cache
NAMES += drcov
NAMES += sandbox

SONAMES := $(addsuffix .so,$(addprefix lib,$(NAMES)))

TARGET_LIB = $(findstring lib-softmmu, $(TARGET_DIRS))
ifeq ($(TARGET_LIB), lib-softmmu)
Q_DYNLIB = y
endif

# The main QEMU uses Glib extensively so it's perfectly fine to use it
# in plugins (which many example do).
PLUGIN_CFLAGS := $(shell $(PKG_CONFIG) --cflags glib-2.0)
PLUGIN_CFLAGS += -fPIC -Wall
PLUGIN_CFLAGS += -I$(TOP_SRC_PATH)/include/qemu

ifeq ($(Q_DYNLIB), y)
BUILD_DIR=$(PWD)

# Additional CFLAGS to compile extended plugins to access CPUArchState
PLUGIN_CFLAGS += -I$(TOP_SRC_PATH)/include
PLUGIN_CFLAGS += -I$(BUILD_DIR) -I$(TOP_SRC_PATH)/target/riscv
PLUGIN_CFLAGS += -DNEED_CPU_H -DCONFIG_TARGET="<$(TARGET_DIRS)-config-target.h>"

LD_PATH=$(BUILD_DIR)
endif

all: $(SONAMES)

%.o: %.c
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -c -o $@ $<

lib%.so: %.o
ifeq ($(CONFIG_DARWIN),y)
	$(CC) -bundle -Wl,-undefined,dynamic_lookup -o $@ $^ $(LDLIBS)
else
ifeq ($(Q_DYNLIB), y)
	$(CC) -shared -o $@ $^ $(LDLIBS) -L$(LD_PATH) -l:qemu-system-riscv64.so -Wl,-rpath=$(LD_PATH)
else
	$(CC) -shared -o $@ $^ $(LDLIBS)
endif
endif

clean:
	rm -f *.o *.so *.d
	rm -Rf .libs

.PHONY: all clean
.SECONDARY:
