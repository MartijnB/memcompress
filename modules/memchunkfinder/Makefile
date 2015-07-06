topdir=../../syslinux
objdir=$(topdir)/bios

MAKEDIR=$(topdir)/mk

include $(MAKEDIR)/elf.mk

MODULE=memchunkfinder

all: $(MODULE).c32

$(MODULE).elf: $(MODULE).o ../shared/mem.o $(LIBS) $(C_LIBS) $(COM32LD)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f $(MODULE).o
	rm -f $(MODULE).elf