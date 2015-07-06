#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#define DEBUG
#include "syslinux/movebits.h"

#include "../shared/mem.h"

static int test_scan_memory(scan_memory_callback_t callback, void *data)
{
    int i, rv;

    printf("Called\n");

    addr_t start = 0x12345670;
    size_t size = 0x10;

    rv = callback(data, start, size, SMT_RESERVED);

    return 0;
}

#include <linux/list.h>

#define list_for_each_entry( pos, head, member )                  \
    for ( pos = list_entry ( (head)->next, typeof ( *pos ), member );     \
          &pos->member != (head);                         \
          pos = list_entry ( pos->member.next, typeof ( *pos ), member ) )

int main(void)
{
    void* esp_p = NULL;

    load_memmap();
    print_memmap();

    printf("Current HEAP address: %8p\n", malloc(1));
    //printf("Current HEAP address: %8p\n", malloc(0xFFFFF));
    printf("Current address: %8p\n", get_current_eip());
    printf("Current ESP: %8p\n", (void*)&esp_p);
    printf("Max addres: %016" PRIx64 "\n", get_max_usable_mem_addres());

    //sl_dump_memmap(syslinux_memory_map());

    //print_sorted_memmap(MEM_ORDER_SIZE);
    //print_sorted_memmap(MEM_ORDER_SIZE_REVERSE);
}
