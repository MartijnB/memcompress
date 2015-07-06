topdir=../../syslinux
objdir=$(topdir)/bios

MAKEDIR=$(topdir)/mk

include $(MAKEDIR)/elf.mk

all: meminfo.c32

meminfo.elf: meminfo.o ../shared/mem.o $(LIBS) $(C_LIBS) $(COM32LD)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f meminfo.o
	rm -f meminfo.elf