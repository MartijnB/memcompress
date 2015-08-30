#include "memcompress.h"

#define LZF_STATE_ARG 1
#define INIT_HTAB 1

#define main main__
#include "liblzf-3.6/lzf_c.c"

#include "liblzf-3.6/lzf_d.c"
#undef main

LZF_STATE lzf_state_htable;

int compress_mem_chunk(
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

    while (mem_read_pointer < mem_read_start + mem_read_length && 
         mem_output_pointer + DST_BUFFER_SIZE < mem_output_start + *mem_output_length &&
         IS_VALID_POINTER(mem_read_pointer)) 
    {
        bool buffer_underrun = 0;

        if (dst_buffer_available < 1.5 * SRC_BUFFER_SIZE) {
            // We have less than 1.5 times the source buffer size in our scratchpad...
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

        unsigned int compressed_length = lzf_compress(src_buffer, src_length, dst_buffer + (DST_BUFFER_SIZE - dst_buffer_available), dst_buffer_available, lzf_state_htable);

        mem_read_pointer += src_length;
        dst_buffer_available -= compressed_length;

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

    sha256_finish(&sha256_ctx, (*header_address)->checksum);

    *mem_output_length = mem_output_pointer - mem_output_start;

    g_chunks++;

    return return_code;
}