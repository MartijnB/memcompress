#include "mem.h"

static const char *const e820_types[] = {
    "usable",
    "reserved",
    "ACPI reclaim",
    "ACPI NVS",
    "unusable",
};

struct e820_data mem_regions[128];
unsigned int mem_region_count = 0;

volatile void* get_current_eip(void)
{
    return __builtin_return_address(0);
}

const char* get_mem_size(uint64_t size)
{
  static char mem_size[20];

  if(!size) {
    sprintf(mem_size, "  0");
  }
  else if(size >> 30L) {
    sprintf(mem_size, "%3llu GB", size >> 30);
  }
  else if(size >> 20L) {
    sprintf(mem_size, "%3llu MB", size >> 20);
  }
  else if(size >> 10) {
    sprintf(mem_size, "%3llu KB", size >> 10);
  }
  else {
    sprintf(mem_size, " %3llu b", size);
  }

  return mem_size;
}

const char* get_mem_ksize(uint64_t size)
{
  static char mem_size[20];

  if(!size) {
    sprintf(mem_size, "0");
  }
  else if(size >> 10) {
    sprintf(mem_size, "%lluK", (size >> 10) + 1);
  }
  else {
    sprintf(mem_size, "1K");
  }

  return mem_size;
}

void load_memmap(void)
{
    com32sys_t ireg, oreg;
    struct e820_data ed;
    void *low_ed;

    low_ed = lmalloc(sizeof ed);
    if (!low_ed)
        return;

    memset(&ireg, 0, sizeof ireg);

    ireg.eax.w[0] = 0xe820;
    ireg.edx.l = 0x534d4150;
    ireg.ecx.l = sizeof(struct e820_data);
    ireg.edi.w[0] = OFFS(low_ed);
    ireg.es = SEG(low_ed);

    memset(&ed, 0, sizeof ed);
    ed.extattr = 1;

    unsigned int mem_region_i = 0;

    do {
        if (mem_region_i == sizeof(mem_regions) / sizeof(struct e820_data)) {
            printf("%s\n", "mem_regions overflow!");
            break;
        }

        memcpy(low_ed, &ed, sizeof ed);

        __intcall(0x15, &ireg, &oreg);
        if (oreg.eflags.l & EFLAGS_CF || oreg.eax.l != 0x534d4150 || oreg.ecx.l < 20) {
            break;
        }

        memcpy(&ed, low_ed, sizeof ed);
        memcpy(&mem_regions[mem_region_i], &ed, sizeof ed);

        mem_regions[mem_region_i].type--;

        mem_region_count = ++mem_region_i;

        if (oreg.ecx.l < 24) {
            ed.extattr = 1;
        }

        ireg.ebx.l = oreg.ebx.l;
    } while (ireg.ebx.l);

    lfree(low_ed);
}

const struct e820_data* get_mem_region(ptr_t mem_address)
{
    for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
        if (mem_regions[mem_region_i].base <= mem_address && mem_address < mem_regions[mem_region_i].base + mem_regions[mem_region_i].len) {
            return &mem_regions[mem_region_i];
        }
    }

    return 0;
}

short get_next_mem_region(struct e820_data** current_mem_region, short order)
{
    if (order == MEM_ORDER_OFFSET) {
        bool next = (*current_mem_region == 0) ? true : false;

        for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
            if (next) {
                *current_mem_region = &mem_regions[mem_region_i];

                return 1;
            }
            else if (*current_mem_region == &mem_regions[mem_region_i]) {
                next = true;
            }
        }
    }
    else if (order == MEM_ORDER_SIZE && mem_region_count > 0) {
        uint64_t current_len = (*current_mem_region == 0) ? 0 : (*current_mem_region)->len;
        uint64_t current_base = (*current_mem_region == 0) ? 0 : (*current_mem_region)->base;

        uint64_t smallest_len = ~0ULL;
        unsigned int smallest_region_nr = ~0u;

        for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
            if (current_len == mem_regions[mem_region_i].len && current_base < mem_regions[mem_region_i].base) {
                smallest_len = mem_regions[mem_region_i].len;
                smallest_region_nr = mem_region_i;

                break;
            }
            else if (current_len < mem_regions[mem_region_i].len && mem_regions[mem_region_i].len < smallest_len) {
                smallest_len = mem_regions[mem_region_i].len;
                smallest_region_nr = mem_region_i;
            }
        }

        if (smallest_region_nr < ~0u && *current_mem_region != &mem_regions[smallest_region_nr]) {
            *current_mem_region = &mem_regions[smallest_region_nr];

            return 1;
        }
    }
    else if (order == MEM_ORDER_SIZE_REVERSE && mem_region_count > 0) {
        uint64_t current_len = (*current_mem_region == 0) ? ~0ULL : (*current_mem_region)->len;
        uint64_t current_base = (*current_mem_region == 0) ? 0 : (*current_mem_region)->base;

        uint64_t biggest_len = 0;
        unsigned int biggest_region_nr = ~0u;

        for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
            if (current_len == mem_regions[mem_region_i].len && current_base < mem_regions[mem_region_i].base) {
                biggest_len = mem_regions[mem_region_i].len;
                biggest_region_nr = mem_region_i;

                break;
            }
            else if (mem_regions[mem_region_i].len < current_len && biggest_len < mem_regions[mem_region_i].len) {
                biggest_len = mem_regions[mem_region_i].len;
                biggest_region_nr = mem_region_i;
            }
        }

        if (biggest_region_nr < ~0u && *current_mem_region != &mem_regions[biggest_region_nr]) {
            *current_mem_region = &mem_regions[biggest_region_nr];

            return 1;
        }
    }
    
    return 0;
}

void print_sorted_memmap(short order)
{
    struct e820_data* current_mem_region = 0;

    unsigned int mem_region_i = 0;
    while (get_next_mem_region(&current_mem_region, order)) {
        if (current_mem_region->extattr != 1) {
            printf("%2x %016" PRIx64 " - %016" PRIx64 " %s %d [%x]", 
                mem_region_i,
                current_mem_region->base, 
                current_mem_region->base + current_mem_region->len, 
                get_mem_size(current_mem_region->len),
                current_mem_region->type,
                current_mem_region->extattr);
        } else {
            printf("%2x %016" PRIx64 " - %016" PRIx64 " %s %d [-]", 
                mem_region_i, 
                current_mem_region->base, 
                current_mem_region->base + current_mem_region->len, 
                get_mem_size(current_mem_region->len),
                current_mem_region->type);
        }

        if (current_mem_region->type < sizeof(e820_types) / sizeof(e820_types[0])) {
            printf(" %s", e820_types[current_mem_region->type]);
        }

        putchar('\n');

        mem_region_i++;
    }
}

struct e820_data* g_last_used_mem_region;

unsigned int is_readable_mem_address(ptr_t mem_address)
{
    //Optimalisation so we dont have to lookup the correct mem region every call
    if (g_last_used_mem_region->base <= mem_address && mem_address < g_last_used_mem_region->base + g_last_used_mem_region->len) {
        if (g_last_used_mem_region->type != E820_TYPE_UNUSABLE && g_last_used_mem_region->type != E820_TYPE_RESERVED) {
            return 1;
        }
    }

    for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
        if (mem_regions[mem_region_i].base <= mem_address && mem_address < mem_regions[mem_region_i].base + mem_regions[mem_region_i].len) {
            if (mem_regions[mem_region_i].type != E820_TYPE_UNUSABLE && mem_regions[mem_region_i].type != E820_TYPE_RESERVED) {
                g_last_used_mem_region = &mem_regions[mem_region_i];

                return 1;
            }
        }
    }

    return 0;
}

unsigned int is_usable_mem_address(ptr_t mem_address)
{
    //Optimalisation so we dont have to lookup the correct mem region every call
    if (g_last_used_mem_region->base <= mem_address && mem_address < g_last_used_mem_region->base + g_last_used_mem_region->len) {
        if (g_last_used_mem_region->type == E820_TYPE_USABLE) {
            return 1;
        }
    }

    if (mem_address <= 0x4FF) { // Real Mode IVT + BDA
        return 0;
    } 

    if (mem_address < 0x100000) {
        return 0;
    }

    for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
        if (mem_regions[mem_region_i].base <= mem_address && mem_address < mem_regions[mem_region_i].base + mem_regions[mem_region_i].len) {
            if (mem_regions[mem_region_i].type == E820_TYPE_USABLE) {
                g_last_used_mem_region = &mem_regions[mem_region_i];

                return 1;
            }
        }
    }

    return 0;
}

ptr_t get_max_usable_mem_addres(void)
{
    static ptr_t max_address = 0;

    if (max_address == 0) {
        for (unsigned int mem_region_i = 0; mem_region_i < mem_region_count; mem_region_i++) {
            if (max_address < mem_regions[mem_region_i].base + mem_regions[mem_region_i].len) {
                if (mem_regions[mem_region_i].type == E820_TYPE_USABLE) {
                    max_address = mem_regions[mem_region_i].base + mem_regions[mem_region_i].len;
                }
            }
        }
    }

    return max_address;
}