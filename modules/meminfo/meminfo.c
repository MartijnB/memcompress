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
    print_memmap();

    printf("Current HEAP address: %8p\n", malloc(1));
    printf("Current address: %8p\n", get_current_eip());
    printf("Current ESP: %8p\n", (void*)&esp_p);
    printf("Max addres: %016" PRIx64 "\n", get_max_usable_mem_addres());
}
