REPO_ROOT := $(abspath ..)
TARGETS := main.bin

LDLIBS ?=

# Find all .c files recursively
SRCC := $(shell find $(REPO_ROOT)/pos/src -name '*.c')
SRCS := $(shell find $(REPO_ROOT)/pos/src -name '*.S')

PIOBJS := $(SRCC:.c=.pi.o)
PIOBJS += $(SRCS:.S=.pi.o)

COMMON_PIOBJS = $(shell find $(REPO_ROOT) -type f -name '*.pi.o' -not -path '*/pos/*')
COMMON_PIOBJS += $(PIOBJS)

include $(REPO_ROOT)/scripts/Makefile.gcc
include $(REPO_ROOT)/scripts/Makefile.ld

all: $(TARGETS) $(PIOBJS)

clean:
	rm -f $(TARGETS) $(COMMON_PIOBJS)
