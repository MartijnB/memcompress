#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "../shared/mem.h"

#define MAGIC_VALUE 0x78563412
#define MAGIC_VALUE_SIZE sizeof(int)

// Module to scan the memory of a system for a certain magic value.
// Print how many bytes are equal to the magic value and how many are not.
int main(void)
{
	void* esp_p = NULL;

    load_memmap();

    ptr_t mem_max_address = get_max_usable_mem_addres();

    printf("Current address: %8p\n", get_current_eip());
    printf("Current ESP: %8p\n", (void*)&esp_p);
    printf("Max addres: %016" PRIx64 "\n", mem_max_address);

    uint64_t valid_bytes = 0;
    uint64_t invalid_bytes = 0;

    // Use UINT64_MAX_VALUE to indicate the start of end is not (yet) found
    ptr_t mem_trashed_start = UINT64_MAX_VALUE;
    ptr_t mem_trashed_end = UINT64_MAX_VALUE;

    for (ptr_t p = 0; p < (mem_max_address - MAGIC_VALUE_SIZE) && IS_VALID_POINTER(p); p += MAGIC_VALUE_SIZE) {
        if (MEM_READ_UINT32(p) == MAGIC_VALUE) {
            valid_bytes += MAGIC_VALUE_SIZE;

            if (mem_trashed_start != UINT64_MAX_VALUE) {
                printf("Found trashed memory: %016" PRIx64 " - %016"PRIx64"\n", mem_trashed_start, mem_trashed_end);

                mem_trashed_start = UINT64_MAX_VALUE;
            }
        }
        else {
            invalid_bytes += MAGIC_VALUE_SIZE;

            if (mem_trashed_start == UINT64_MAX_VALUE) {
                mem_trashed_start = p;
            }

            mem_trashed_end = p + MAGIC_VALUE_SIZE;
        }
    }

    printf("Valid bytes: %016" PRIx64 " (%s)\n", valid_bytes, get_mem_size(valid_bytes));
    printf("Invalid bytes: %016" PRIx64 " (%s)\n", invalid_bytes, get_mem_size(invalid_bytes));


    return 0;
}
