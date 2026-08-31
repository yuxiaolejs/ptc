REPO_ROOT := $(abspath ..)
TARGETS := xfb.host.elf

LDLIBS ?=
LDLIBS += -lX11 -lXext

COMMON_OBJS = $(shell find $(REPO_ROOT) -type f -name '*.host.o' -not -path '*/host/*')

include $(REPO_ROOT)/scripts/Makefile.gcc
include $(REPO_ROOT)/scripts/Makefile.ld

all: $(TARGETS)
