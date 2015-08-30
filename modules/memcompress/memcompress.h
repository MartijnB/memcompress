#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "../shared/mem.h"
#include "../shared/util.h"
#include "../shared/sha256.h"

#define RETURN_CODE_OK                  0
#define RETURN_CODE_FAILED             -1
#define RETURN_CODE_OUTPUT_BUFFER_FULL -2

#define SRC_NULL_LENGTH 1024

#define SRC_BUFFER_SIZE 4 * 1024
#define DST_BUFFER_SIZE 32 * 1024

#define MAX_CHUNK_SIZE 100 * 1024 * 1024

#define KERNEL_CMD_LINE_SIZE 1024

// linux.c
const char* linux_get_cmdline(void);
void linux_append_cmdline(const char *format, ...);
void boot_linux(void);

// memcompres.c
extern uint64_t g_chunks;

// memcompress_{lzf|zlib}.c
int compress_mem_chunk(
    const uint64_t mem_read_start, 
    const uint64_t mem_read_length, 
    const uint64_t mem_output_start, 
    uint64_t* const mem_output_length, 
    struct compress_header** const header_address);