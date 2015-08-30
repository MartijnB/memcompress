#ifndef _SHARED_MEM_H
#define _SHARED_MEM_H

#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "platform.h"

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

#define MEM_ORDER_OFFSET       1
#define MEM_ORDER_SIZE         2
#define MEM_ORDER_SIZE_REVERSE 3

#define MEM_READ_INT8(p)   (*((int8_t*)   TO_N_PTR(p)))
#define MEM_READ_INT16(p)  (*((int16_t*)  TO_N_PTR(p)))
#define MEM_READ_INT32(p)  (*((int32_t*)  TO_N_PTR(p)))
#define MEM_READ_INT64(p)  (*((int64_t*)  TO_N_PTR(p)))

#define MEM_READ_UINT8(p)  (*((uint8_t*)  TO_N_PTR(p)))
#define MEM_READ_UINT16(p) (*((uint16_t*) TO_N_PTR(p)))
#define MEM_READ_UINT32(p) (*((uint32_t*) TO_N_PTR(p)))
#define MEM_READ_UINT64(p) (*((uint64_t*) TO_N_PTR(p)))

struct compress_header {
    unsigned char marker[8];
    ptr_t start_address;
    ptr_t end_address;
    uint64_t compressed_length;
    uint64_t uncompressed_length;
    uint64_t skipped_length;
    int8_t checksum[32];
} __attribute__ ((packed));

#define TO_CMP_HDR_PTR(p) ((struct compress_header*) TO_N_PTR(p))

#define HAS_CHUNK_MARKER(p) \
    (((struct compress_header*) TO_N_PTR(p))->marker[0] == 'M' && \
     ((struct compress_header*) TO_N_PTR(p))->marker[1] == 'E' && \
     ((struct compress_header*) TO_N_PTR(p))->marker[2] == 'M' && \
     ((struct compress_header*) TO_N_PTR(p))->marker[3] == 0xf1 && \
     ((struct compress_header*) TO_N_PTR(p))->marker[4] == 0x88 && \
     ((struct compress_header*) TO_N_PTR(p))->marker[5] == 0x15 && \
     ((struct compress_header*) TO_N_PTR(p))->marker[6] == 0x08 && \
     ((struct compress_header*) TO_N_PTR(p))->marker[7] == 0x5c)

// Destroy markers in such a way that they can be recognized if something really goes wrong
#define DESTROY_CHUNK_MARKER(p) \
    { TO_CMP_HDR_PTR(p)->marker[1] = '3'; } // MEM -> M3M

#define print_memmap() print_sorted_memmap(MEM_ORDER_OFFSET)

extern struct e820_data mem_regions[128];
extern unsigned int mem_region_count;

void* to_native_pointer(ptr_t p);

volatile void* get_current_eip(void);

void load_memmap(void);
void print_sorted_memmap(short order);

short get_next_mem_region(struct e820_data** current_mem_region, short order);

const char* get_mem_size(uint64_t size);
const char* get_mem_ksize(uint64_t size);

ptr_t get_max_usable_mem_addres(void);

const struct e820_data* get_mem_region(ptr_t mem_address);

unsigned int is_readable_mem_address(ptr_t mem_address);
unsigned int is_usable_mem_address(ptr_t mem_address);

#endif /* _SHARED_MEM_H */