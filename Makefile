
REPO_ROOT := $(abspath .)

all: host.elf main.bin

deps:
	$(MAKE) -C $(REPO_ROOT)/lib
	$(MAKE) -C $(REPO_ROOT)/gui
	$(MAKE) -C $(REPO_ROOT)/engine
	$(MAKE) -C $(REPO_ROOT)/pos

$(REPO_ROOT)/host/xfb.host.elf: deps
	$(MAKE) -C $(REPO_ROOT)/host

host.elf: $(REPO_ROOT)/host/xfb.host.elf
	cp $< $@

main.bin: $(REPO_ROOT)/pos/main.bin
	cp $< $@

pi:
	$(MAKE) -C $(REPO_ROOT)/pos

clean:
	$(MAKE) -C $(REPO_ROOT)/lib clean
	$(MAKE) -C $(REPO_ROOT)/gui clean
	$(MAKE) -C $(REPO_ROOT)/host clean
	$(MAKE) -C $(REPO_ROOT)/pos clean
	$(MAKE) -C $(REPO_ROOT)/engine clean
	rm -f host.elf