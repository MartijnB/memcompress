#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "../shared/mem.h"

// Scan for and print the locations of compressed chunks
int main(void)
{
    load_memmap();

    ptr_t mem_max_address = get_max_usable_mem_addres();

    printf("Max addres: %016" PRIx64 "\n", mem_max_address);

    unsigned int chunks_found = 0;
    for (ptr_t p = 0; p < mem_max_address && IS_VALID_POINTER(p); p++) {
        if (is_readable_mem_address(p) && HAS_CHUNK_MARKER(p)) {
            struct compress_header* h = (struct compress_header*) TO_N_PTR(p);

            printf("%016" PRIx64 "-%016" PRIx64, h->start_address, h->end_address);
            printf(" %s", get_mem_size(h->uncompressed_length));
            printf(" (%s) @ %016" PRIx64 "\n", get_mem_size(h->compressed_length), p);

            chunks_found++;
        }
    }

    printf("%u Chunks found.\n", chunks_found);

    return 0;
}
