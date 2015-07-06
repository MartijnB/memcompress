#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <console.h>

#include "zlib.h"

#include "../shared/mem.h"
#include "../shared/util.h"
#include "../shared/sha256.h"

#define LZF_STATE_ARG 1
#define INIT_HTAB 1

#define main main__
#include "liblzf-3.6/lzf_c.c"

#include "liblzf-3.6/lzf_d.c"
#undef main

#define DEBUG_PORT
#define DYNAMIC_DEBUG
#define DEBUG_CORE
#define CORE_DEBUG 1
#define DEBUG_STDIO

//#define syslinux_shuffle_boot_rm local_nop

#define syslinux_dump_memmap sl_dump_memmap

#include "../lib/syslinux/load_linux.c"

#define syslinux_boot_linux bios_boot_linux
#define find_argument linux_find_argument
#define main linuxc32_main
#include "../modules/linux.c"
#undef main

#define KERNEL_CMD_LINE_SIZE 1024

#define SRC_NULL_LENGTH 1024

#define SRC_BUFFER_SIZE 4 * 1024
#define DST_BUFFER_SIZE 32 * 1024

#define MAX_CHUNK_SIZE 100 * 1024 * 1024

#define RETURN_CODE_OK                  0
#define RETURN_CODE_FAILED             -1
#define RETURN_CODE_OUTPUT_BUFFER_FULL -2

#define _compress_mem_chunk compress_mem_chunk_lzf

uint64_t g_chunks = 0;

char g_cmdline_buffer[KERNEL_CMD_LINE_SIZE];
char* g_cmdline_buffer_p = g_cmdline_buffer;

LZF_STATE lzf_state_htable;

// Write src_buffer of length src_len to dst_buffer. Update dst_len with the amount of data written.
int write_buffer_to_buffer(const void* const src_buffer, const unsigned int src_len, const uint64_t dst_buffer, uint64_t* const dst_len) {
    uint64_t p_in = 0;
    uint64_t p_out = 0;
    for (; p_in < src_len && p_out < *dst_len && (IS_64BIT() || dst_buffer + p_out <= ~0u);) {
        if (is_usable_mem_address(dst_buffer + p_out)) {
            *((unsigned char*)(dst_buffer + (p_out++))) = *((unsigned char*)(src_buffer + (p_in++)));
        }
        else {
            p_out++;
        }
    }

    *dst_len = p_out;

    int return_code = 0;

    if (IS_32BIT() && dst_buffer + p_out > ~0u) {
        printf("WARNING: Output buffer reached end of address space!\n");
        
        return_code = RETURN_CODE_FAILED;
    }

    if (p_in < src_len) {
        printf("Failed to write all data!\n");
        
        return_code = RETURN_CODE_FAILED;
    }

    return return_code;
}

int compress_mem_chunk_lzf(
    const uint64_t mem_read_start, 
    const uint64_t mem_read_length, 
    const uint64_t mem_output_start, 
    uint64_t* const mem_output_length, 
    struct compress_header** const header_address)
{
    int return_code = 0;

    printf("Chunk %016" PRIx64 " - %016" PRIx64 " @ 0x%" PRIx64 " (%s)...\n", 
        mem_read_start, 
        mem_read_start + mem_read_length, 
        mem_output_start,
        get_mem_size(*mem_output_length)
    );

    unsigned char src_buffer[SRC_BUFFER_SIZE];
    unsigned char dst_buffer[DST_BUFFER_SIZE];

    unsigned short dst_buffer_available = DST_BUFFER_SIZE;

    unsigned int src_length = 0;
    
    uint64_t mem_read_pointer = mem_read_start;
    uint64_t mem_output_pointer = mem_output_start;

    context_sha256_t sha256_ctx;

    sha256_starts(&sha256_ctx);

    // Set header pointer to begin of the output buffer
    *header_address = (struct compress_header*) mem_output_pointer;
    mem_output_pointer += sizeof(struct compress_header);

    uint64_t bytes_read = 0;
    uint64_t bytes_write = 0;
    uint64_t bytes_skipped = 0;

    while ((mem_read_pointer < mem_read_start + mem_read_length && 
         mem_output_pointer + DST_BUFFER_SIZE < mem_output_start + *mem_output_length && 
         (IS_64BIT() || mem_read_pointer <= ~0u))) 
    {
        bool buffer_underrun = 0;

        if (dst_buffer_available < 1.5 * SRC_BUFFER_SIZE) {
            // We have less than 100 bytes in our scratchpad...
            // There are basicly 2 options left. Skip some data or crash.
            // Most likely is skipping some data the less evil of the two

            if (!buffer_underrun) {
                printf("WARNING: Buffer underrun!\n");

                buffer_underrun = 1;
            }

            unsigned int read_length = (mem_read_pointer + SRC_NULL_LENGTH) < mem_read_start + mem_read_length ? SRC_NULL_LENGTH : (mem_read_start + mem_read_length) - mem_read_pointer;

            memset(src_buffer, 0, read_length);
            src_length = read_length;

            bytes_skipped += read_length;
        }
        else if (is_readable_mem_address(mem_read_pointer)) {
            unsigned int read_length = (mem_read_pointer + SRC_BUFFER_SIZE) < mem_read_start + mem_read_length ? SRC_BUFFER_SIZE : (mem_read_start + mem_read_length) - mem_read_pointer;

            //for (int i = 0; i < read_length; i++) {
            //    if (!is_readable_mem_address(mem_read_pointer + i)) {
            //        read_length = i;
            //    }
            //}

            //printf("%p %i\n", (unsigned int*)mem_read_pointer, read_length);

            memcpy(src_buffer, (void*)mem_read_pointer, read_length);
            src_length = read_length;

            bytes_read += read_length;

            buffer_underrun = 0;
        }
        else {
            unsigned int read_length = (mem_read_pointer + SRC_NULL_LENGTH) < mem_read_start + mem_read_length ? SRC_NULL_LENGTH : (mem_read_start + mem_read_length) - mem_read_pointer;

            memset(src_buffer, 0, read_length);
            src_length = read_length;

            bytes_skipped += read_length;
        }

        sha256_update(&sha256_ctx, src_buffer, src_length);

        //printf("%p %i %p %i\n", src_buffer, src_length, dst_buffer + (DST_BUFFER_SIZE - dst_buffer_available), dst_buffer_available);

        //unsigned int compressed_length = 0;
        unsigned int compressed_length = lzf_compress(src_buffer, src_length, dst_buffer + (DST_BUFFER_SIZE - dst_buffer_available), dst_buffer_available, lzf_state_htable);

        mem_read_pointer += src_length;
        dst_buffer_available -= compressed_length;

        //printf("%i\n", compressed_length);

        if (compressed_length == 0) {
            printf("Memory compression failed!\n");

            printf("Read pointer %016" PRIx64 " Output pointer %016" PRIx64 " length %u/%u\n", 
                mem_read_pointer, 
                mem_output_pointer, 
                src_length,
                (DST_BUFFER_SIZE - dst_buffer_available)
            );
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }

        if (dst_buffer_available < DST_BUFFER_SIZE) {
            unsigned int bytes_waiting = DST_BUFFER_SIZE - dst_buffer_available;
            uint64_t mem_output_length_remaing = *mem_output_length - (mem_output_pointer - mem_output_start);

            //printf("Read pointer %016" PRIx64 " Output pointer %016" PRIx64 "\n", 
            //    mem_read_pointer, 
            //    mem_output_pointer
            //  );

            // Make sure we stay behind the read pointer
            if ((mem_output_pointer + bytes_waiting) < mem_read_pointer || mem_read_start + mem_read_length <= mem_output_start) { 
                if (write_buffer_to_buffer(dst_buffer, bytes_waiting, mem_output_pointer, &mem_output_length_remaing) < 0 || 
                    mem_output_length_remaing == 0)
                {
                    printf("Fatal error!\n");
                    
                    return_code = RETURN_CODE_FAILED;
                    goto out;
                }

                mem_output_pointer += mem_output_length_remaing;
                bytes_write += bytes_waiting;

                dst_buffer_available = DST_BUFFER_SIZE;
            }
            else {
                //printf("Partial buffer flush %i...\n", mem_read_pointer - mem_output_pointer);

                bytes_waiting = mem_read_pointer - mem_output_pointer;

                if (write_buffer_to_buffer(dst_buffer, bytes_waiting, mem_output_pointer, &mem_output_length_remaing) < 0 || 
                    mem_output_length_remaing == 0)
                {
                    printf("Fatal error!\n");
                    
                    return_code = RETURN_CODE_FAILED;
                    goto out;
                }

                mem_output_pointer += mem_output_length_remaing;
                bytes_write += bytes_waiting;

                //printf("Remaing: %i\n", DST_BUFFER_SIZE - dst_buffer_available - bytes_waiting);

                memmove(dst_buffer, dst_buffer + bytes_waiting, DST_BUFFER_SIZE - dst_buffer_available - bytes_waiting);

                dst_buffer_available += bytes_waiting;
            }
        }

        if (dst_buffer_available == 0 || dst_buffer_available > DST_BUFFER_SIZE) {
            printf("Memory compression failed! No space left in buffer\n");
            printf("Read pointer %016" PRIx64 " Output pointer %016" PRIx64 " length %x\n", 
                mem_read_pointer, 
                mem_output_pointer, 
                (DST_BUFFER_SIZE - dst_buffer_available)
            );
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }
    }

    if (IS_32BIT() && mem_read_pointer > ~0u) {
        printf("WARNING: Only lower 32 bit of memory is compressed!\n");
    }

out:

    //Marker so it can be found back
    (*header_address)->marker[0] = 'M';
    (*header_address)->marker[1] = 'E';
    (*header_address)->marker[2] = 'M';
    (*header_address)->marker[3] = 0xf1;
    (*header_address)->marker[4] = 0x88;
    (*header_address)->marker[5] = 0x15;
    (*header_address)->marker[6] = 0x08;
    (*header_address)->marker[7] = 0x5c;

    (*header_address)->start_address = mem_read_start;
    (*header_address)->end_address = mem_read_pointer - 1;
    (*header_address)->compressed_length = bytes_write;
    (*header_address)->uncompressed_length = bytes_read + bytes_skipped;
    (*header_address)->skipped_length = bytes_skipped;

    //(*header_address)->start_address = mem_read_start;
    //(*header_address)->end_address = mem_read_start + mem_read_length - 1;
    //(*header_address)->compressed_length = 0;
    //(*header_address)->uncompressed_length = mem_read_length;
    //(*header_address)->skipped_length = 0;

    sha256_finish(&sha256_ctx, (*header_address)->checksum);

    *mem_output_length = mem_output_pointer - mem_output_start;
    //*mem_output_length = 80;

    //printf("%p\n", mem_read_pointer - 1);
    //printf("%i\n", *mem_output_length);

    g_chunks++;

    //printf("Data in:               0x%x\n", bytes_read);
    //printf("Data out:              0x%x\n", bytes_write);
    //printf("Address space skipped: 0x%x\n", bytes_skipped);

    sleep(1);

    return return_code;
}

int compress_mem_chunk_zlib(
    const uint64_t mem_read_start, 
    const uint64_t mem_read_length, 
    const uint64_t mem_output_start, 
    uint64_t* const mem_output_length, 
    struct compress_header** const header_address)
{
    int return_code = 0;

    printf("Chunk %016" PRIx64 " - %016" PRIx64 " @ 0x%" PRIx64 " (%s)...\n", 
        mem_read_start, 
        mem_read_start + mem_read_length, 
        mem_output_start,
        get_mem_size(*mem_output_length)
    );

    unsigned char src_buffer[128];
    unsigned char dst_buffer[DST_BUFFER_SIZE];
    
    uint64_t mem_read_pointer = mem_read_start;
    uint64_t mem_output_pointer = mem_output_start;

    context_sha256_t sha256_ctx;

    z_stream stream_context;

    stream_context.zalloc = (alloc_func)0;
    stream_context.zfree = (free_func)0;
    stream_context.opaque = (voidpf)0;

    stream_context.next_out = dst_buffer;
    stream_context.avail_out = DST_BUFFER_SIZE;

    if (deflateInit(&stream_context, Z_BEST_COMPRESSION) != Z_OK) {
        printf("Zlib init failed!\n");
        return RETURN_CODE_FAILED;
    }

    sha256_starts(&sha256_ctx);

    // Set header pointer to begin of the output buffer
    *header_address = (struct compress_header*) mem_output_pointer;
    mem_output_pointer += sizeof(struct compress_header);

    uint64_t bytes_read = 0;
    uint64_t bytes_write = 0;
    uint64_t bytes_skipped = 0;

    for (; 
        (mem_read_pointer < mem_read_start + mem_read_length && 
        mem_output_pointer + DST_BUFFER_SIZE < mem_output_start + *mem_output_length && 
        (IS_64BIT() || mem_read_pointer <= ~0u));
        mem_read_pointer += sizeof(int)) 
    {
        bool buffer_underrun = 0;
        int data;

        if (stream_context.avail_out < 100) {
            // We have less than 100 bytes in our scratchpad...
            // There are basicly 2 options left. Skip some data or crash.
            // Most likely is skipping some data the less evil of the two

            if (!buffer_underrun) {
                printf("WARNING: Buffer underrun!\n");

                buffer_underrun = 1;
            }

            data = 0;

            bytes_skipped += sizeof(int);            
        }
        else if (is_readable_mem_address(mem_read_pointer)) {
            data = *((int*)mem_read_pointer);

            bytes_read += sizeof(int);

            buffer_underrun = 0;
        }
        else {
            data = 0;

            bytes_skipped += sizeof(int);
        }

        sha256_update(&sha256_ctx, (uint8_t*)&data, sizeof(int));

        //The input is not always 100% used, so use a small buffer for the few cases a couple of bytes are left
        *(int*)(src_buffer + stream_context.avail_in) = data;

        stream_context.next_in = src_buffer;
        stream_context.avail_in += sizeof(int);

        if (deflate(&stream_context, Z_NO_FLUSH) != Z_OK) {
            printf("Memory compression failed!\n");
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }

        if (stream_context.avail_out < (DST_BUFFER_SIZE - 100)) {
            unsigned short bytes_waiting = DST_BUFFER_SIZE - stream_context.avail_out;

            if ((mem_output_pointer + bytes_waiting) < mem_read_pointer || mem_read_start + mem_read_length <= mem_output_start) { 
                // Make sure we stay behind the read pointer
                uint64_t mem_output_length_remaing = *mem_output_length - (mem_output_pointer - mem_output_start);

                if (write_buffer_to_buffer(dst_buffer, bytes_waiting, mem_output_pointer, &mem_output_length_remaing) < 0 || 
                    mem_output_length_remaing == 0)
                {
                    printf("Fatal error!\n");
                    
                    return_code = RETURN_CODE_FAILED;
                    goto out;
                }

                mem_output_pointer += mem_output_length_remaing;
                bytes_write += bytes_waiting;

                stream_context.next_out = dst_buffer;
                stream_context.avail_out = DST_BUFFER_SIZE;
            }
        }

        if (stream_context.avail_out == 0) {
            printf("Memory compression failed! No space left in buffer\n");
            printf("Read pointer %016" PRIx64 " Output pointer %016" PRIx64 " length %x\n", 
                mem_read_pointer, 
                mem_output_pointer, 
                (DST_BUFFER_SIZE - stream_context.avail_out)
            );
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }
    }

    if (IS_32BIT() && mem_read_pointer > ~0u) {
        printf("WARNING: Only lower 32 bit of memory is compressed!\n");
    }

    int deflate_status;
    do {
        deflate_status = deflate(&stream_context, Z_FINISH);

        if (deflate_status != Z_OK && deflate_status != Z_STREAM_END) {
            printf("Memory compression failed to finish!\n");
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }

        unsigned short bytes_waiting = DST_BUFFER_SIZE - stream_context.avail_out;

        if ((mem_output_pointer + bytes_waiting) >= mem_read_pointer && mem_output_start <= mem_read_start) {
            printf("WARNING: Output buffer will overwrite not read data!\n");
            printf("Read pointer %016" PRIx64 " Output pointer %016" PRIx64 " length %x\n", 
                mem_read_pointer, 
                mem_output_pointer, 
                (DST_BUFFER_SIZE - stream_context.avail_out)
            );
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }

        uint64_t mem_output_length_remaing = *mem_output_length - (mem_output_pointer - mem_output_start);

        if (write_buffer_to_buffer(dst_buffer, bytes_waiting, mem_output_pointer, &mem_output_length_remaing) < 0 || 
            mem_output_length_remaing == 0)
        {
            printf("Fatal error!\n");
            
            return_code = RETURN_CODE_FAILED;
            goto out;
        }

        mem_output_pointer += mem_output_length_remaing;
        bytes_write += bytes_waiting;

        stream_context.next_out = dst_buffer;
        stream_context.avail_out = DST_BUFFER_SIZE;
    }
    while(deflate_status == Z_OK);

    if (deflateEnd(&stream_context) != Z_OK) {
        printf("Compression state failure!\n");
        
        return_code = RETURN_CODE_FAILED;
        goto out;
    }

out:

    //Marker so it can be found back
    (*header_address)->marker[0] = 'M';
    (*header_address)->marker[1] = 'E';
    (*header_address)->marker[2] = 'M';
    (*header_address)->marker[3] = 0xf1;
    (*header_address)->marker[4] = 0x88;
    (*header_address)->marker[5] = 0x15;
    (*header_address)->marker[6] = 0x08;
    (*header_address)->marker[7] = 0x5c;

    (*header_address)->start_address = mem_read_start;
    (*header_address)->end_address = mem_read_pointer - 1;
    (*header_address)->compressed_length = bytes_write;
    (*header_address)->uncompressed_length = bytes_read + bytes_skipped;
    (*header_address)->skipped_length = bytes_skipped;

    sha256_finish(&sha256_ctx, (*header_address)->checksum);

    *mem_output_length = mem_output_pointer - mem_output_start;

    g_chunks++;

    //printf("Data in:               0x%x\n", bytes_read);
    //printf("Data out:              0x%x\n", bytes_write);
    //printf("Address space skipped: 0x%x\n", bytes_skipped);

    return return_code;
}

int compress_mem_range(
    const uint64_t region_start, 
    const uint64_t region_length, 
    const uint64_t output_buffer_start, 
    uint64_t* const output_buffer_length)
{
    if (*output_buffer_length < DST_BUFFER_SIZE) {
        printf("ERROR: Output buffer too small; must be at least %x bytes!\n", DST_BUFFER_SIZE);
        return RETURN_CODE_FAILED;
    }

    printf("Start compressing %016" PRIx64 " - %016" PRIx64 " @ 0x%" PRIx64 " (%s)\n", 
        region_start, 
        region_start + region_length, 
        output_buffer_start,
        get_mem_size(*output_buffer_length)
    );

    struct compress_header* mem_output_header;

    uint64_t bytes_compressed = 0;
    uint64_t bytes_uncompressed = 0;
    uint64_t bytes_written = 0;
    uint64_t bytes_skipped = 0;

    uint64_t output_buffer = output_buffer_start;

    uint64_t bytes_left = region_length;
    do {
        uint64_t output_length = *output_buffer_length - (output_buffer - output_buffer_start);

        if (output_length < DST_BUFFER_SIZE) {
            printf("WARNING: Output buffer full! Last read: %016" PRIx64 "\n", mem_output_header->end_address);
            return RETURN_CODE_OUTPUT_BUFFER_FULL;
        }

        int status = _compress_mem_chunk(
            region_start + region_length - bytes_left, 
            (bytes_left > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : bytes_left, 
            output_buffer,
            &output_length,
            &mem_output_header
        );

        if (status < 0) {
            printf("WARNING: Chunk failed!\n");
        }

        bytes_left -= mem_output_header->uncompressed_length;

        bytes_compressed += mem_output_header->compressed_length;
        bytes_uncompressed += mem_output_header->uncompressed_length;
        bytes_skipped += mem_output_header->skipped_length;
        bytes_written += mem_output_header->compressed_length + sizeof(struct compress_header);

        output_buffer += mem_output_header->compressed_length + sizeof(struct compress_header);
    }
    while(bytes_left > 0);

    *output_buffer_length = output_buffer - output_buffer_start;

    //printf("Bytes comp: %s", get_mem_size(bytes_compressed));
    //printf(" - uncomp: %s", get_mem_size(bytes_uncompressed));
    //printf(" - skip: %s\n", get_mem_size(bytes_skipped));

    return 0;
}

static int scan_memory_for_chunks(scan_memory_callback_t callback, void *data)
{
    int rv = 0;

    printf("scan memory\n");

    uint64_t mem_max_address = get_max_usable_mem_addres();
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
printf("callback\n");

            rv = callback(data, p, tmp_ch_p->compressed_length + sizeof(struct compress_header), SMT_RESERVED);

            if (rv)
                break;
        }
    }

    return rv;
}

// Relocate the chunks from the begin to the end of a mem region.
// The assumption is that chunks are always continuously in a mem region and
// 2 usable mem regions are never continuously in the address space
// The 'E' in the marker is replaced with '3'. If something goes wrong, a
// manual dump can be created and carved for this marker to rescue the remaining data
int relocate_chunks(void) {
    uint64_t mem_max_address = get_max_usable_mem_addres();

    bool start_found = false;
    uint64_t start_offset = 0;

    printf("Scan memory until %x...\n", mem_max_address);

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
            printf("Found marker\n");

            if (!start_found) {
                start_offset = p;
                start_found = true;
            }

            p += tmp_ch_p->compressed_length + sizeof(struct compress_header) - 1;
        }
        else {
            if (start_found) {
                if (get_mem_region(start_offset) != get_mem_region(p - 1)) {
                    printf("Chunk range crosses multiple mem regions!\n");
                    return -1;
                }

                const struct e820_data* mem_region = get_mem_region(start_offset);

                uint64_t dst_length = p - start_offset;
                uint64_t dst_start = (mem_region->base + mem_region->len) - dst_length;

                printf("Move chunks from %016" PRIx64 " to %016" PRIx64 " (%s)\n", start_offset, dst_start, get_mem_size(dst_length));

                memmove((void*)dst_start, (void *)start_offset, dst_length);

                g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), " memmap=%s", get_mem_ksize(dst_length + 1024)); //Add 1024 bc we subtract it from the offset.
                g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), "$%s", get_mem_ksize(((((unsigned long) dst_start) >> 10UL) - 1) << 10UL));

                g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), " m3mcomp=%llu@%llu", dst_length, dst_start);

                // Destroy markers in such way that they can be recognized if something really goes wrong
                for (uint64_t q = 0; q < dst_start && q < mem_max_address && (IS_64BIT() || q <= ~0u); q++) {
                    struct compress_header* tmp_ch_q = (struct compress_header*)q;
                    
                    if (is_readable_mem_address(q) &&
                        tmp_ch_q->marker[0] == 'M' && 
                        tmp_ch_q->marker[1] == 'E' && 
                        tmp_ch_q->marker[2] == 'M' && 
                        tmp_ch_q->marker[3] == 0xf1 && 
                        tmp_ch_q->marker[4] == 0x88 && 
                        tmp_ch_q->marker[5] == 0x15 && 
                        tmp_ch_q->marker[6] == 0x08 && 
                        tmp_ch_q->marker[7] == 0x5c) 
                    {
                        tmp_ch_q->marker[1] = '3';
                    }

                }

                p = mem_region->base + mem_region->len - 1;

                start_found = false;
            }
        }
    }

    return 0;
}

// Protect memory > 4 GB in 32bit mode
void protect_uncompressed_memory(void) {
    struct e820_data* current_mem_region = 0;

    while (get_next_mem_region(&current_mem_region, MEM_ORDER_SIZE)) {
        if (current_mem_region->type == E820_TYPE_USABLE) {
            if (IS_32BIT() && current_mem_region->base > 0xFFFFFFFFLL) {
                g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), " memmap=%s", get_mem_ksize(current_mem_region->len - 1));
                g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), "$%s", get_mem_ksize((uint64_t) current_mem_region->base - 1));

                g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), " m3mraw=%llu@%llu", current_mem_region->len, current_mem_region->base);
            }
        }
        
        //if (current_mem_region->type != E820_TYPE_UNUSABLE) {
        //        g_cmdline_buffer_p += snprintf(g_cmdline_buffer_p, KERNEL_CMD_LINE_SIZE - (g_cmdline_buffer_p - g_cmdline_buffer), " m3mraw=%llu@%llu", current_mem_region->len, current_mem_region->base);
        //}
    }
}

int main(void)
{
    load_memmap();

    if (strlen(com32_cmdline()) > 512) {
        printf("ERROR: Commandline too long!\n");
        return 0;
    }

    g_cmdline_buffer_p += snprintf(g_cmdline_buffer, KERNEL_CMD_LINE_SIZE, "linux.c32 %s", com32_cmdline());

    bool use_tmp = false;
    bool use_tmp_now = false;

    struct e820_data tmp_mem_region;
    struct e820_data* current_mem_region = 0;
    struct e820_data* old_mem_region = 0;

    uint64_t output_buffer_start = 0;
    uint64_t output_buffer = 0;
    uint64_t output_buffer_length = 0;
    uint64_t output_buffer_size = 0;
    bool force_other_buffer = false;

    while (use_tmp || get_next_mem_region(&current_mem_region, MEM_ORDER_SIZE_REVERSE)) {
        if (use_tmp) {
            old_mem_region = current_mem_region;
            current_mem_region = &tmp_mem_region;

            use_tmp_now = true;
        }

        if ((IS_64BIT() || (current_mem_region->base <= ~0u && (current_mem_region->base + (current_mem_region->len - 1)) <= ~0u))) {
            if (current_mem_region->type == E820_TYPE_UNUSABLE || current_mem_region->type == E820_TYPE_RESERVED) {
                continue;
            }

            force_other_buffer = (current_mem_region->type != E820_TYPE_USABLE);

            printf("Compress mem region: %016" PRIx64 " - %016" PRIx64 " (%s)\n", 
                current_mem_region->base, 
                current_mem_region->base + current_mem_region->len, 
                get_mem_size(current_mem_region->len)
            );

            uint64_t region_start = current_mem_region->base;
            uint64_t region_length = current_mem_region->len;

            // Syslinux code
            // 0x0000000000330000                free_high_memory = .
            
            //if (current_mem_region->base < 0x330000ULL && (current_mem_region->base + current_mem_region->len) > 0x330000ULL) {
            //    region_start = 0x330000ULL;
            //    region_length = current_mem_region->len - (0x330000ULL - current_mem_region->base);
            //}

            if (current_mem_region->base < 0x1000000ULL && (current_mem_region->base + current_mem_region->len) > 0x1000000ULL) {
                region_start = 0x1000000ULL;
                region_length = current_mem_region->len - (0x1000000ULL - current_mem_region->base);

                tmp_mem_region.base = current_mem_region->base;
                tmp_mem_region.len = 0x1000000ULL - current_mem_region->base;
                tmp_mem_region.type = current_mem_region->type;

                use_tmp = true;
                use_tmp_now = false;
            }

            // Check if the heap is located here
            void* current_heap_pointer = malloc(1);
            if (region_start < current_heap_pointer && (region_start + region_length) > current_heap_pointer) {
                force_other_buffer = true;
            }

            if (output_buffer_length < region_length) {
                if (force_other_buffer) {
                    printf("Not enough space in output buffer, however can't overwrite current region; SKIP!\n");
                    continue;
                }
                else {
                    if (output_buffer_size) { // Don't show the message for the first region
                        printf("Not enough space in output buffer, output on the same location as input.\n");
                    }

                    output_buffer_start = output_buffer = region_start;
                    output_buffer_size = output_buffer_length = region_length;
                }
            }

            int status = compress_mem_range(
                region_start, 
                region_length, 
                output_buffer, 
                &output_buffer_length);

            if (status < 0) {
                switch(status) {
                    case RETURN_CODE_OUTPUT_BUFFER_FULL:
                        printf("Buffer full!\n");
                        return 0;

                    default:
                        printf("Unknown internal error!\n");
                        return 0;
                }
            }

            output_buffer += output_buffer_length;
            output_buffer_length = output_buffer_size - (output_buffer - output_buffer_start);

            printf("Region compressed, %s of space left\n", get_mem_size(output_buffer_length));
        }

        if (use_tmp && use_tmp_now) {
            current_mem_region = old_mem_region;

            use_tmp = false;
            use_tmp_now = false;
        }
    }

    if (relocate_chunks())  {
        printf("Failed to relocate chunks. Abort, and good luck!\n");
        return 0;
    }

    protect_uncompressed_memory();

    printf("Chunks created: %lli\n", g_chunks);
    printf("Kernel arguments: %s\n", g_cmdline_buffer);

    sleep(3);

    printf("Updating memory map...\n");

    // Modify memory map to include chunks
    // Add a function to the linked list to override memory map entries. New memscan functions are added to the begin of the list
    struct syslinux_memscan *override_entry;
    override_entry = malloc(sizeof *override_entry);
    override_entry->func = scan_memory_for_chunks;
    syslinux_memscan_add(override_entry);

    printf("memscan entry added\n");

    // Based on the added list entry, we can find the head of the memscan list
    //struct list_head* syslinux_memscan_head = &override_entry->next.prev;

    // New memmap entries overrule pevious added entries, so make our function the last in the list.
    struct syslinux_memscan *last_entry = list_entry(&override_entry->next.next->next->next, struct syslinux_memscan, next);
    void *org_func = last_entry->func;

    last_entry->func = scan_memory_for_chunks;
    override_entry->func = org_func;

    printf("memory map\n");

    sl_dump_memmap(syslinux_memory_map());

    printf("create args\n");

    int   linux_argc;
    char** linux_argv;

    create_args(g_cmdline_buffer, &linux_argc, &linux_argv);

    sleep(5);

    printf("Boot kernel...\n");

    linuxc32_main(linux_argc, linux_argv);

    return 0;
}
