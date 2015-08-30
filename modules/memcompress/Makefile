topdir=../../syslinux
objdir=$(topdir)/bios

MAKEDIR=$(topdir)/mk

include $(MAKEDIR)/elf.mk

MODULE=memcompress

all: $(MODULE).c32

$(MODULE).elf: $(MODULE).o linux.o memcompress_zlib.o ../shared/mem.o ../shared/util.o ../shared/sha256.o $(LIBS) $(C_LIBS) $(COM32LD)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f ../shared/*.o

	rm -f *.o
	rm -f $(MODULE).elf
	rm -f $(MODULE).c32