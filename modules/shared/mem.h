#ifndef _MOD_MEM_H
#define _MOD_MEM_H

#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

struct e820_data {
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t extattr;
} __attribute__ ((packed));

#define E820_TYPE_USABLE       0
#define E820_TYPE_RESERVED     1
#define E820_TYPE_ACPI_RECLAIM 2
#define E820_TYPE_ACPI_NVS     3
#define E820_TYPE_UNUSABLE     4

#define IS_32BIT() (sizeof(void*) == 4)
#define IS_64BIT() (sizeof(void*) == 8)

#define MEM_ORDER_OFFSET       1
#define MEM_ORDER_SIZE         2
#define MEM_ORDER_SIZE_REVERSE 3

struct compress_header {
    unsigned char marker[8];
    uint64_t start_address;
    uint64_t end_address;
    uint64_t compressed_length;
    uint64_t uncompressed_length;
    uint64_t skipped_length;
    unsigned char checksum[32];
} __attribute__ ((packed));

#define print_memmap() print_sorted_memmap(MEM_ORDER_OFFSET)

extern struct e820_data mem_regions[128];
extern unsigned int mem_region_count;

volatile void* get_current_eip(void);

void load_memmap(void);
void print_sorted_memmap(short order);

short get_next_mem_region(struct e820_data** current_mem_region, short order);

const char* get_mem_size(uint64_t size);
const char* get_mem_ksize(uint64_t size);

uint64_t get_max_usable_mem_addres(void);

const struct e820_data* get_mem_region(uint64_t mem_address);

unsigned int is_readable_mem_address(uint64_t mem_address);
unsigned int is_usable_mem_address(uint64_t mem_address);

#include <syslinux/memscan.h>
void sl_dump_memmap(struct syslinux_memmap *memmap);

#endif /* _MOD_MEM_H */