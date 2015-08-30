topdir=../../syslinux
objdir=$(topdir)/bios

MAKEDIR=$(topdir)/mk

include $(MAKEDIR)/elf.mk

all: meminfo.c32

meminfo.elf: meminfo.o ../shared/mem.o $(LIBS) $(C_LIBS) $(COM32LD)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f ../shared/*.o

	rm -f *.o
	rm -f meminfo.elf
	rm -f meminfo.c32