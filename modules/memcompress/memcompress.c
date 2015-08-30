#include "memcompress.h"

#define DEBUG_PORT
#define DYNAMIC_DEBUG
#define DEBUG_CORE
#define CORE_DEBUG 1
#define DEBUG_STDIO

uint64_t g_chunks = 0;

// Write src_buffer of length src_len to dst_buffer. Update dst_len with the amount of data written.
int write_buffer_to_buffer(const void* const src_buffer, const unsigned int src_len, const uint64_t dst_buffer, uint64_t* const dst_len) {
    uint64_t dst_buffer_len = *dst_len;

    uint64_t p_in = 0;
    uint64_t p_out = 0;
    for (; p_in < src_len && p_out < dst_buffer_len && IS_VALID_POINTER(dst_buffer + p_out);) {
        if (is_usable_mem_address(dst_buffer + p_out)) {
            *((unsigned char*)(dst_buffer + (p_out))) = *((unsigned char*)(src_buffer + (p_in)));

            p_in++;
            p_out++;
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

        int status = compress_mem_chunk(
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

    return 0;
}

static int scan_memory_for_chunks(scan_memory_callback_t callback, void *data)
{
    int rv = 0;

    ptr_t mem_max_address = get_max_usable_mem_addres();
    for (ptr_t p = 0; p < mem_max_address && IS_VALID_POINTER(p); p++) {
        if (is_readable_mem_address(p) && HAS_CHUNK_MARKER(p)) {
            rv = callback(data, p, TO_CMP_HDR_PTR(p)->compressed_length + sizeof(struct compress_header), SMT_RESERVED);

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
    ptr_t mem_max_address = get_max_usable_mem_addres();

    bool start_found = false;
    ptr_t start_offset = 0;

    printf("Scan memory until %x...\n", mem_max_address);

    for (ptr_t p = 0; p < mem_max_address && IS_VALID_POINTER(p); p++) {
        if (is_readable_mem_address(p) && HAS_CHUNK_MARKER(p)) {
            printf("Found marker\n");

            if (!start_found) {
                start_offset = p;
                start_found = true;
            }

            p += TO_CMP_HDR_PTR(p)->compressed_length + sizeof(struct compress_header) - 1;
        }
        else if (start_found) {
            if (get_mem_region(start_offset) != get_mem_region(p - 1)) {
                printf("Chunk range crosses multiple mem regions!\n");
                return RETURN_CODE_FAILED;
            }

            const struct e820_data* mem_region = get_mem_region(start_offset);

            ptr_t dst_length = p - start_offset;
            ptr_t dst_start = (mem_region->base + mem_region->len) - dst_length;

            printf("Move chunks from %016" PRIx64 " to %016" PRIx64 " (%s)\n", start_offset, dst_start, get_mem_size(dst_length));

            memmove(TO_N_PTR(dst_start), TO_N_PTR(start_offset), dst_length);

            // Mark the location of the chunk as unavailable by adding a "memmap" parameter to the kernel commandline
            linux_append_cmdline(" memmap=%s", get_mem_ksize(dst_length + 1024)); //Add 1024 bc we subtract it from the offset.
            linux_append_cmdline("$%s", get_mem_ksize(((((unsigned long) dst_start) >> 10UL) - 1) << 10UL));

            // Add a "m3mcomp" parameter to the kernel commandline to allow easy extraction with the extraction script
            linux_append_cmdline(" m3mcomp=%llu@%llu", dst_length, dst_start);

            // Destroy markers in such a way that they can be recognized if something really goes wrong
            for (ptr_t q = 0; q < dst_start && q < mem_max_address && IS_VALID_POINTER(q); q++) {
                if (is_readable_mem_address(q) && HAS_CHUNK_MARKER(q)) {
                    DESTROY_CHUNK_MARKER(q);
                }
            }

            p = mem_region->base + mem_region->len - 1;

            start_found = false;
        }
    }

    return RETURN_CODE_OK;
}

// Protect memory > 4 GB in 32bit mode as this can not be compressed
// Hide it for the kernel by marking it as unavailable in the memory kernel using kernel parameters
// And add a special parameter to know it's location during the extraction
void protect_uncompressed_memory(void) {
    struct e820_data* current_mem_region = 0;

    while (get_next_mem_region(&current_mem_region, MEM_ORDER_SIZE)) {
        if (current_mem_region->type == E820_TYPE_USABLE) {
            if (IS_32BIT() && current_mem_region->base > 0xFFFFFFFFLL) {
                // Mark the location of uncompressed data as unavailable by adding a "memmap" parameter to the kernel commandline
                linux_append_cmdline(" memmap=%s", get_mem_ksize(current_mem_region->len - 1));
                linux_append_cmdline("$%s", get_mem_ksize((uint64_t) current_mem_region->base - 1));

                // Add a "m3mraw" parameter to the kernel commandline to indicate a part of the memory must we extracted without decompressing it
                linux_append_cmdline(" m3mraw=%llu@%llu", current_mem_region->len, current_mem_region->base);
            }
        }
    }
}

int main(void)
{
    load_memmap();

    if (strlen(com32_cmdline()) > 512) {
        printf("ERROR: Commandline too long!\n");
        return 0;
    }

    linux_append_cmdline("%s %s", "linux.c32", com32_cmdline());

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
    printf("Kernel arguments: %s\n", linux_get_cmdline());

    sleep(3);

    printf("Updating memory map...\n");

    // Modify memory map to include chunks
    // Add a function to the linked list to override memory map entries. New memscan functions are added to the begin of the list
    struct syslinux_memscan *override_entry;
    override_entry = malloc(sizeof *override_entry);
    override_entry->func = scan_memory_for_chunks;
    syslinux_memscan_add(override_entry);

    // Based on the added list entry, we can find the head of the memscan list
    //struct list_head* syslinux_memscan_head = &override_entry->next.prev;

    // New memmap entries overrule pevious added entries, so make our function the last in the list.
    struct syslinux_memscan *last_entry = list_entry(&override_entry->next.next->next->next, struct syslinux_memscan, next);
    void *org_func = last_entry->func;

    last_entry->func = scan_memory_for_chunks;
    override_entry->func = org_func;

    sl_dump_memmap(syslinux_memory_map());

    boot_linux();

    return 0;
}
