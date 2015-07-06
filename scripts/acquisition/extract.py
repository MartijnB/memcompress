#!/usr/bin/python

import os
import pickle
import uuid
import struct
import zlib
import hashlib
import operator

DUMP_STORAGE_PATH = os.getcwd() + "/dumps"


def strtohex(s, sep=""):
    return sep.join(x.encode('hex') for x in s)


class DevMem:
    def __init__(self):
        self.fd = os.open("/dev/mem", os.O_RDONLY)

        self.buffer = ""
        self.buffer_p = 0

        self.current_pos = 0
        self.max_offset = 0

    def read(self, n=1, buffer_size=8192, return_tuple=False):
        if n > 1 or return_tuple:
            return bytearray(os.read(self.fd, n)), n
        else:
            return bytearray(os.read(self.fd, n))

        # if (len(self.buffer) - self.buffer_p) <= 0:
        #     buffer_size = self.max_offset - self.current_pos if ((self.current_pos + buffer_size) > self.max_offset) else buffer_size
        #
        #     self.buffer = bytearray(os.read(self.fd, buffer_size))
        #     self.buffer_p = 0
        #
        #     self.current_pos += buffer_size
        #
        # p_old = self.buffer_p
        # p_new = (p_old + n) % (buffer_size + 1)
        #
        # tmp_buffer = self.buffer[p_old:p_new]
        # self.buffer_p = p_new
        #
        # if n > 1 or return_tuple:
        #     return tmp_buffer, p_new - p_old
        # else:
        #     return tmp_buffer

    def read_guaranteed(self, n):
        b = ""
        p = 0

        while p < n:
            b_tmp, b_tmp_len = self.read(n - len(b), return_tuple=True)

            b += b_tmp
            p += b_tmp_len

        return b

    def seek(self, pos, how=os.SEEK_SET):
        os.lseek(self.fd, pos, how)

        # Invalidate the buffer
        self.buffer = ""
        self.buffer_p = 0

        self.current_pos = pos

    def set_max_seek(self, max_offset):
        self.max_offset = max_offset


devmem = DevMem()

kernel_cmdline = ""
with open("/proc/cmdline", "r") as f:
    kernel_cmdline = f.read()

session_guid = str(uuid.uuid4())

os.mkdir(DUMP_STORAGE_PATH + session_guid)

compressed_blobs = []
uncompressed_blobs = []

for p in kernel_cmdline.split(" "):
    if len(p.strip()) > 0:
        cmd = p.strip()

        if len(cmd) > 7 and cmd[:7] == "m3mcomp":
            try:
                region_length, region_start = cmd.split("=")[1].split("@")

                region_start = int(region_start)
                region_length = int(region_length)

                print "Read region {0} - {1}".format(hex(region_start), hex(region_start + region_length))

                devmem.seek(region_start)
                devmem.set_max_seek(region_start + region_length)

                p = 0
                while p < region_length:
                    if ord(devmem.read()) == 0x4D:
                        if ord(devmem.read()) == 0x45 and \
                                        ord(devmem.read()) == 0x4D and \
                                        ord(devmem.read()) == 0xF1 and \
                                        ord(devmem.read()) == 0x88 and \
                                        ord(devmem.read()) == 0x15 and \
                                        ord(devmem.read()) == 0x08 and \
                                        ord(devmem.read()) == 0x5C:
                            print "Chunk found @ {0}".format(hex(region_start + p))

                            chunk_start_address = struct.unpack("<Q", bytes(devmem.read_guaranteed(8)))[0]
                            chunk_end_address = struct.unpack("<Q", bytes(devmem.read_guaranteed(8)))[0]
                            chunk_compressed_length = struct.unpack("<Q", bytes(devmem.read_guaranteed(8)))[0]
                            chunk_uncompressed_length = struct.unpack("<Q", bytes(devmem.read_guaranteed(8)))[0]
                            chunk_skipped_length = struct.unpack("<Q", bytes(devmem.read_guaranteed(8)))[0]
                            chunk_checksum = bytes(devmem.read_guaranteed(32))

                            p += 8 + 8 + 8 + 8 + 8 + 8 + 32  # 80 bytes

                            print "{0} - {1} Compressed: {2} Uncompressed: {3}".format(hex(chunk_start_address).rstrip("L"), hex(chunk_end_address).rstrip("L"), chunk_compressed_length, chunk_uncompressed_length)

                            compressed_filename = "memregion_{0}-{1}_{2}b_{3}.compressed".format(hex(chunk_start_address).rstrip("L"), hex(chunk_end_address).rstrip("L"), chunk_uncompressed_length, strtohex(chunk_checksum))
                            decompressed_filename = "memregion_{0}-{1}_{2}b_{3}".format(hex(chunk_start_address).rstrip("L"), hex(chunk_end_address).rstrip("L"), chunk_uncompressed_length, strtohex(chunk_checksum))

                            print "Storing into {0}...".format(DUMP_STORAGE_PATH + session_guid + "/" + compressed_filename)

                            with open(DUMP_STORAGE_PATH + session_guid + "/" + compressed_filename, "wb") as cfd:
                                q = 0

                                while q < chunk_compressed_length:
                                    b, b_len = devmem.read(8192 if q + 8192 < chunk_compressed_length else chunk_compressed_length - q)
                                    q += b_len

                                    cfd.write(b)

                            if os.path.getsize(DUMP_STORAGE_PATH + session_guid + "/" + compressed_filename) != chunk_compressed_length:
                                print "Incorrect length! Expected: {0} Found: {1}".format(chunk_compressed_length, os.path.getsize(DUMP_STORAGE_PATH + session_guid + "/" + compressed_filename))

                            p += chunk_compressed_length

                            compressed_blobs.append({
                                "start_address": chunk_start_address,
                                "end_address": chunk_end_address,
                                "compressed_length": chunk_compressed_length,
                                "uncompressed_length": chunk_uncompressed_length,
                                "skipped_length": chunk_skipped_length,
                                "checksum": chunk_checksum,
                                "compressed_filename": compressed_filename,
                                "decompressed_filename": decompressed_filename,
                            })

                            continue

                        devmem.seek(region_start + p + 1)
                    p += 1
            except BaseException as e:
                print "Error with: " + cmd + " @ " + hex(region_start + p)
                print e

        elif len(cmd) > 6 and cmd[:6] == "m3mraw":
            try:
                region_length, region_start = cmd.split("=")[1].split("@")

                region_start = int(region_start)
                region_length = int(region_length)

                p = 0

                print "Read raw data from region {0} - {1}".format(hex(region_start).rstrip("L"), hex(region_start + region_length).rstrip("L"))

                devmem.seek(region_start)
                devmem.set_max_seek(region_start + region_length)

                decompressed_filename = "memregion_{0}-{1}_{2}b".format(hex(region_start).rstrip("L"), hex(region_start + region_length).rstrip("L"), region_length)

                print "Storing into {0}...".format(DUMP_STORAGE_PATH + session_guid + "/" + decompressed_filename)

                with open(DUMP_STORAGE_PATH + session_guid + "/" + decompressed_filename, "wb") as ucfd:
                    while p < region_length:
                        b, b_len = devmem.read(8192 if p + 8192 <= region_length else region_length - p)
                        p += b_len

                        ucfd.write(b)

                uncompressed_blobs.append({
                    "start_address": region_start,
                    "end_address": region_start + region_length - 1,
                    "compressed_length": None,
                    "uncompressed_length": region_length,
                    "skipped_length": 0,
                    "checksum": None,
                    "compressed_filename": None,
                    "decompressed_filename": decompressed_filename,
                    "valid_checksum": True
                })

            except BaseException as e:
                print "Error with: " + cmd + " @ " + hex(region_start + p).rstrip("L") + " - " + hex(region_start + p + 8192 if p + 8192 <= region_length else region_length - p).rstrip("L")
                print e

with open(DUMP_STORAGE_PATH + session_guid + "/" + ".meta", "w") as f:
    pickle.dump({
        "compressed": compressed_blobs,
        "uncompressed": uncompressed_blobs
    }, f)

print("Compressed: {0}".format(len(compressed_blobs)))
print("Uncompressed: {0}".format(len(uncompressed_blobs)))

# for compressed_blob in compressed_blobs:
#     try:
#         print "Decompress {0}...".format(compressed_blob["compressed_filename"])
#
#         decompressor = zlib.decompressobj()
#
#         cs = hashlib.sha256()
#
#         with open(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["compressed_filename"], 'rb') as fd:
#             with open(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["decompressed_filename"], 'wb') as n_fd:
#                 for data in iter(lambda: fd.read(8192), ''):
#                     dc_data = decompressor.decompress(data)
#
#                     n_fd.write(dc_data)
#                     cs.update(dc_data)
#
#                 dc_data = decompressor.flush()
#
#                 n_fd.write(dc_data)
#                 cs.update(dc_data)
#
#         if compressed_blob["checksum"] != cs.digest():
#             print "Incorrect checksum!"
#
#         if os.path.getsize(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["decompressed_filename"]) != compressed_blob["uncompressed_length"]:
#             print "Incorrect length! Expected: {0} Found: {1}".format(compressed_blob["uncompressed_length"], os.path.getsize(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["decompressed_filename"]))
#
#         uncompressed_blobs.append({
#             "start_address": compressed_blob["start_address"],
#             "end_address": compressed_blob["end_address"],
#             "compressed_length": compressed_blob["compressed_length"],
#             "uncompressed_length": compressed_blob["uncompressed_length"],
#             "skipped_length": compressed_blob["skipped_length"],
#             "checksum": compressed_blob["checksum"],
#             "compressed_filename": compressed_blob["compressed_filename"],
#             "decompressed_filename": compressed_blob["decompressed_filename"],
#             "valid_checksum": compressed_blob["checksum"] == cs.digest()
#         })
#     except BaseException as e:
#         print "Decompression failed!"
#         print e
#
# print "\n\nMemory map:\n"
#
# with open(DUMP_STORAGE_PATH + session_guid + "/" + "/memory.txt", "w") as fd:
#     last_address = -1
#
#     for blob in sorted(uncompressed_blobs, key=operator.itemgetter('start_address')):
#         if (blob["start_address"] - last_address) > 1:
#             fd.write("[{0:16X}] - [{1:16X}] {2}".format(last_address + 1, blob["start_address"] - 1, "MISSING") + "\r\n")
#             print "[{0:16X}] - [{1:16X}] {2}".format(last_address + 1, blob["start_address"] - 1, "MISSING")
#
#
#         fd.write("[{0:16X}] - [{1:16X}] {2}".format(blob["start_address"], blob["end_address"], "OK" if blob["valid_checksum"] else "Checksum INVALID!") + "\r\n")
#         print "[{0:16X}] - [{1:16X}] {2}".format(blob["start_address"], blob["end_address"], "OK" if blob["valid_checksum"] else "Checksum INVALID!")
#
#         last_address = blob["end_address"]
