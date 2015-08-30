topdir=../../syslinux
objdir=$(topdir)/bios

MAKEDIR=$(topdir)/mk

include $(MAKEDIR)/elf.mk

all: memwrite.c32

memwrite.elf: memwrite.o ../shared/mem.o $(LIBS) $(C_LIBS) $(COM32LD)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f ../shared/*.o

	rm -f *.o
	rm -f memwrite.elf
	rm -f memwrite.c32