#include "util.h"

// Originally part of the Syslinux source code
// However, this function is only added to debug builds of Syslinux
void syslinux_dump_memmap(struct syslinux_memmap *memmap)
{
    printf("%10s %10s %10s\n" "--------------------------------\n", "Start", "Length", "Type");
    
    while (memmap->next) {
        printf("0x%08x 0x%08x %10d\n", memmap->start, memmap->next->start - memmap->start, memmap->type);

        memmap = memmap->next;
    }
}