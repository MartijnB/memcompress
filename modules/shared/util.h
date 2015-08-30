#ifndef _SHARED_UTIL_H
#define _SHARED_UTIL_H

#include <syslinux/memscan.h>

#undef syslinux_dump_memmap

void syslinux_dump_memmap(struct syslinux_memmap *memmap);

#endif