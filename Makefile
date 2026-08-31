
REPO_ROOT := $(abspath .)

all: host.elf

deps:
	$(MAKE) -C $(REPO_ROOT)/lib
	$(MAKE) -C $(REPO_ROOT)/gui

host: deps
	$(MAKE) -C $(REPO_ROOT)/host

host.elf: $(REPO_ROOT)/host/xfb.host.elf
	cp $< $@

pi:
	$(MAKE) -C $(REPO_ROOT)/pos

clean:
	$(MAKE) -C $(REPO_ROOT)/lib clean
	$(MAKE) -C $(REPO_ROOT)/gui clean
	$(MAKE) -C $(REPO_ROOT)/host clean
	$(MAKE) -C $(REPO_ROOT)/pos clean
	rm -f host.elf