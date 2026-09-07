
REPO_ROOT := $(abspath ..)
include $(REPO_ROOT)/scripts/Makefile.gcc

# Find all .c files recursively
SRCC := $(shell find . -name '*.c')
SRCS := $(shell find . -name '*.S')

# Convert .c → .o
OBJS := $(SRCC:.c=.host.o)
OBJS += $(SRCS:.S=.host.o)

PIOBJS := $(SRCC:.c=.pi.o)
PIOBJS += $(SRCS:.S=.pi.o)


all: $(PIOBJS)

clean:
	rm -f $(OBJS) $(PIOBJS)

.PHONY: all clean