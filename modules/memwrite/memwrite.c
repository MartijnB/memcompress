#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/io.h>

#include "../shared/mem.h"

inline static void __attribute__((always_inline)) local_farcall(uint16_t cs, uint16_t ip, const com32sys_t * ireg, com32sys_t * oreg)
{
    com32sys_t xreg = *ireg;

    void __cdecl (*cs_farcall)(uint32_t, const com32sys_t *, com32sys_t *) = 0x100000;

    cs_farcall((cs << 16) + ip, &xreg, oreg);
}

// Copy of the syslinux_reboot function without external dependencies
inline static __noreturn __attribute__((always_inline)) local_reboot(int warm) // 70 bytes
{
    const com32sys_t zero_regs = {0};

    *((uint16_t *) 0x472) = warm ? 0x1234 : 0; //reboot flag
    local_farcall(0xf000, 0xfff0, &zero_regs, NULL);

    while (1)
	asm volatile ("hlt");
}

//#define REBOOT

int main(int argc, char *argv[])
{
    load_memmap();
    print_memmap();
/*
	void* esp_p = NULL;

    load_memmap();
    print_memmap();

    printf("Current address: %8p\n", get_current_eip());
    printf("Current ESP: %8p\n", (void*)&esp_p);
    printf("Mem region: %8p\n", mem_regions);
    printf("local_reboot address: %8p %x\n", local_reboot, sizeof(local_reboot));
    printf("Max addres: %016" PRIx64 "\n", get_max_usable_mem_addres());

    */

    uint64_t p = get_max_usable_mem_addres();

    printf("Start address: %8p\n", &&start);
    printf("Stop address: %8p\n", &&end + 55);
    printf("P address: %8p\n", &p);

    // Can't disable all interrupts as this would kill ctrl alt del
    //asm volatile ("cli");

    // Disable NMI
    outb(0x70, inb(0x70)|0x80);

start:;
    do {
    	p -= sizeof(int);

    	if (/*(IS_64BIT() || p <= ~0u) &&*/
            (p < &&start-4 || p > &&end + 100) &&
            p != &p
    	) {
    		*((int*)p) = 0x78563412;
    	}
    }
#ifdef REBOOT
    while(p > 0x1000A6);
#else
    while(p > 0x100000);
#endif

#ifdef REBOOT
    p = 0x100000;
#endif

    do {
        p -= sizeof(int);

        *((int*)p) = 0x78563412;
    }
#ifdef REBOOT
    while(p > 0x8a00);
#else
    while(p > 0x8900);
#endif

    p = 0x00;
    do {
        *((int*)p) = 0x78563412;

        p += sizeof(int);
    }
    while(p < 0x10);

    p = 0x80;
    do {
        *((int*)p) = 0x78563412;

        p += sizeof(int);
    }
    while(p < 0x3000);

/*
    p = 0x3900;
    do {
        *((int*)p) = 0x78563412;

        p += sizeof(int);
    }
    while(p < 0x3800);
    */

    p = 0x3c00;
    do {
        *((int*)p) = 0x78563412;

        p += sizeof(int);
    }
    while(p < 0x7e78);

#ifdef REBOOT
    local_reboot(1);
#endif

    p = &&start;
    do {
        *((int*)p) = 0x78563412;

        p += sizeof(int);
    }
    while(1);

    p = 0x78563412;
end:;

    return 0;
}
