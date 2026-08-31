ARM := arm-none-eabi

MEMMAP ?= $(REPO_ROOT)/scripts/memmap.ld

COMMON_PIOBJS ?=
COMMON_OBJS ?=

# Borrowed from staff code
%.bin: %.host.o $(COMMON_PIOBJS) $(MEMMAP)
	@printf "\033[32mLinking %s...\033[0m\n" "$(abspath $@)"
	@printf "\033[32mCommon OBJs %s...\033[0m\n" "$(COMMON_PIOBJS)"
	# $* is the shared name: for target 1-blink.bin, $* == 1-blink
	# $< is the first input file (the .o built from 1-blink.c)
	$(PICC) $(PICFLAGS) $(COMMON_PIOBJS) $< -T $(MEMMAP) -o $*.pi.elf
	$(ARM)-objcopy $*.pi.elf -O binary $@
	$(ARM)-objdump -D $*.pi.elf > $*.list
	printf "\033[32mCreated %s, %s, and %s.\033[0m\n" "$@" "$*.pi.elf" "$*.list"

%.host.elf: %.host.o $(COMMON_OBJS)
	@printf "\033[32mLinking %s...\033[0m\n" "$(abspath $@)"
	$(CC) $(CFLAGS) $(COMMON_OBJS) $< -o $@ $(LDLIBS)
