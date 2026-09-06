REPO_ROOT := $(abspath ..)
TARGETS := main.bin

LDLIBS ?=

COMMON_PIOBJS = $(shell find $(REPO_ROOT) -type f -name '*.pi.o' -not -path '*/pos/*')

include $(REPO_ROOT)/scripts/Makefile.gcc
include $(REPO_ROOT)/scripts/Makefile.ld

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(COMMON_PIOBJS)
