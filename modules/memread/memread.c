#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "../shared/mem.h"

int main(void)
{
	void* esp_p = NULL;

    load_memmap();

    uint64_t mem_max_address = get_max_usable_mem_addres();

    printf("Current address: %8p\n", get_current_eip());
    printf("Current ESP: %8p\n", (void*)&esp_p);
    printf("Max addres: %016" PRIx64 "\n", mem_max_address);

    uint64_t valid_bytes = 0;
    uint64_t invalid_bytes = 0;

    uint64_t mem_trashed_start = ~0LL;
    uint64_t mem_trashed_end = ~0LL;

    for (uint64_t p = 0; p < mem_max_address && (IS_64BIT() || p <= ~0u); p += sizeof(int)) {
        if (*((int*)p) == 0x78563412) {
        //if (*((int*)p) == 0xF0F0F0F0) {
            valid_bytes += sizeof(int);

            if (mem_trashed_start != ~0ULL) {
                printf("Found trashed memory: %016" PRIx64 " - %016"PRIx64"\n", mem_trashed_start, mem_trashed_end);

                mem_trashed_start = ~0ULL;
            }
        }
        else {
            invalid_bytes += sizeof(int);

            if (mem_trashed_start == ~0ULL) {
                mem_trashed_start = p;
            }

            mem_trashed_end = p + sizeof(int);
        }
    }

    printf("Valid bytes: %016" PRIx64 " (%s)\n", valid_bytes, get_mem_size(valid_bytes));
    printf("Invalid bytes: %016" PRIx64 " (%s)\n", invalid_bytes, get_mem_size(invalid_bytes));


    return 0;
}
