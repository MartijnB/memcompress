#!/usr/bin/python
import hashlib

import os
import pickle
import sys
import zlib
import operator
import lzf
import io

DUMP_STORAGE_PATH = os.getcwd() + "/dumps"

session_guid = sys.argv[1].strip().replace("dumps","").replace("/", "")

print "Memory dump: {0}".format(session_guid)

if not os.path.exists(DUMP_STORAGE_PATH + "/" + session_guid):
    print "Dump missing!"
    exit(0)

if not os.path.exists(DUMP_STORAGE_PATH + "/" + session_guid + "/.meta"):
    print "Metadata missing!"
    exit(0)

with open(DUMP_STORAGE_PATH + session_guid + "/" + ".meta", "r") as f:
    data = pickle.load(f)

compressed_blobs = data["compressed"]
uncompressed_blobs = data["uncompressed"]

print("Compressed: {0}".format(len(compressed_blobs)))
print("Uncompressed: {0}".format(len(uncompressed_blobs)))

for compressed_blob in compressed_blobs:
    try:
        print "Decompress {0}...".format(compressed_blob["compressed_filename"])

        if False:
            decompressor = zlib.decompressobj()

        cs = hashlib.sha256()

        with io.FileIO(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["compressed_filename"], 'rb') as fd:
            with io.FileIO(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["decompressed_filename"], 'wb') as n_fd:
                if True:
                    cd_data = fd.readall()
                    dc_data = lzf.decompress(cd_data, 1 * 1024 * 1024 * 1024)

                    n_fd.write(dc_data)
                    cs.update(dc_data)
                else:
                    for data in iter(lambda: fd.read(8192), ''):
                        dc_data = decompressor.decompress(data)

                        n_fd.write(dc_data)
                        cs.update(dc_data)

                    dc_data = decompressor.flush()

                    n_fd.write(dc_data)
                    cs.update(dc_data)

        if compressed_blob["checksum"] != cs.digest():
            print "Incorrect checksum!"

        if os.path.getsize(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["decompressed_filename"]) != compressed_blob["uncompressed_length"]:
            print "Incorrect length! Expected: {0} Found: {1}".format(compressed_blob["uncompressed_length"], os.path.getsize(DUMP_STORAGE_PATH + session_guid + "/" + compressed_blob["decompressed_filename"]))

        uncompressed_blobs.append({
            "start_address": compressed_blob["start_address"],
            "end_address": compressed_blob["end_address"],
            "compressed_length": compressed_blob["compressed_length"],
            "uncompressed_length": compressed_blob["uncompressed_length"],
            "skipped_length": compressed_blob["skipped_length"],
            "checksum": compressed_blob["checksum"],
            "compressed_filename": compressed_blob["compressed_filename"],
            "decompressed_filename": compressed_blob["decompressed_filename"],
            "valid_checksum": compressed_blob["checksum"] == cs.digest()
        })
    except BaseException as e:
        print "Decompression failed!"
        print e

print "\n\nMemory map:\n"

with open(DUMP_STORAGE_PATH + session_guid + "/" + "/memory.txt", "w") as fd:
    last_address = -1

    for blob in sorted(uncompressed_blobs, key=operator.itemgetter('start_address')):
        if (blob["start_address"] - last_address) > 1:
            fd.write("[{0:16X}] - [{1:16X}] {2}".format(last_address + 1, blob["start_address"] - 1, "MISSING") + "\r\n")
            print "[{0:16X}] - [{1:16X}] {2}".format(last_address + 1, blob["start_address"] - 1, "MISSING")


        fd.write("[{0:16X}] - [{1:16X}] {2}".format(blob["start_address"], blob["end_address"], "OK" if blob["valid_checksum"] else "Checksum INVALID!") + "\r\n")
        print "[{0:16X}] - [{1:16X}] {2}".format(blob["start_address"], blob["end_address"], "OK" if blob["valid_checksum"] else "Checksum INVALID!")

        last_address = blob["end_address"]