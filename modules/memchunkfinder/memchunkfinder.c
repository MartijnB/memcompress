#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "../shared/mem.h"

int main(void)
{
    load_memmap();

    uint64_t mem_max_address = get_max_usable_mem_addres();

    printf("Max addres: %016" PRIx64 "\n", mem_max_address);

    unsigned int chunks_found = 0;
    for (uint64_t p = 0; p < mem_max_address && (IS_64BIT() || p <= ~0u); p++) {
        struct compress_header* tmp_ch_p = (struct compress_header*)p;

        if (is_readable_mem_address(p) &&
            tmp_ch_p->marker[0] == 'M' && 
            tmp_ch_p->marker[1] == 'E' && 
            tmp_ch_p->marker[2] == 'M' && 
            tmp_ch_p->marker[3] == 0xf1 && 
            tmp_ch_p->marker[4] == 0x88 && 
            tmp_ch_p->marker[5] == 0x15 && 
            tmp_ch_p->marker[6] == 0x08 && 
            tmp_ch_p->marker[7] == 0x5c) 
        {
            printf("%016" PRIx64 "-%016" PRIx64, tmp_ch_p->start_address,tmp_ch_p->end_address);
            printf(" %s", get_mem_size(tmp_ch_p->uncompressed_length));
            printf(" (%s) @ %016" PRIx64 "\n", get_mem_size(tmp_ch_p->compressed_length), p);

            chunks_found++;
        }
    }

    printf("%u Chunks found.\n", chunks_found);

    return 0;
}
