#include "memcompress.h"

#include "zlib.h"

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
        mem_read_pointer < mem_read_start + mem_read_length && 
        mem_output_pointer + DST_BUFFER_SIZE < mem_output_start + *mem_output_length && 
        IS_VALID_POINTER(mem_read_pointer);
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

    return return_code;
}