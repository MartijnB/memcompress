import os

DUMP_PATH = os.getcwd() + "/dumps"

# http://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size
def sizeof_fmt(num, suffix=''):
    if abs(num) < 1024.0:
        return "%3.0f %s%s" % (num, '', suffix)

    for unit in ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z']:
        if abs(num) < 1024.0:
            return "%3.1f %s%s" % (num, unit, suffix)
        num /= 1024.0

    return "%.1f %s%s" % (num, 'Y', suffix)


for dirs in os.listdir(DUMP_PATH):
    print dirs

    log = open(DUMP_PATH + "/" + dirs + "/" + "analysis.txt", "w")

    valid_bytes = 0
    invalid_bytes = 0

    if not os.path.isdir(DUMP_PATH + "/" + dirs):
        continue

    for filename in os.listdir(DUMP_PATH + "/" + dirs):
        if not os.path.isfile(DUMP_PATH + "/" + dirs + "/" + filename):
            continue

        if filename != "memory.txt" and filename != ".meta" and filename[-11:] != ".compressed" and filename[-4:] != ".rtf" and filename[-4:] != ".txt":
            print filename
            log.write(filename + "\n")

            try:
                start_address_base = int(filename.split("_")[1].split("-")[0], 16)
            except:
                start_address_base = 0

            p = 0
            start_address = -1
            with open(DUMP_PATH + "/" + dirs + "/" + filename, "r") as f:
                while True:
                    b = f.read(1)

                    if b == "\x12\x34\x56\x78"[p % 4]:
                        if start_address >= 0:
                            log.write(hex(start_address_base + start_address) + "-" + hex(start_address_base + p - 1) + "\n")
                            start_address = -1

                        p += 1
                        valid_bytes += 1
                    elif b == "":
                        if start_address >= 0:
                            log.write(hex(start_address) + "-" + hex(start_address_base + p - 1) + "\n")
                        break
                    else:
                        if start_address == -1:
                            start_address = p

                        p += 1
                        invalid_bytes += 1

    try:
        log.write("Valid: " + str(valid_bytes) + "\n")
        log.write("Valid: " + str(sizeof_fmt(valid_bytes)) + "\n")
        log.write("Invalid: " + str(invalid_bytes) + "\n")
        log.write("Invalid: " + str(1073741824 - valid_bytes) + "\n")
        log.write("Invalid: " + str(sizeof_fmt(invalid_bytes)) + "\n")
        log.write("Invalid: " + str(sizeof_fmt(1073741824 - valid_bytes)) + "\n")

        log.write("Percentage: {0}".format(1. * valid_bytes / (valid_bytes + invalid_bytes) * 100) + "\n")
        log.write("Percentage: {0}".format(100 - 1. * valid_bytes / (valid_bytes + invalid_bytes) * 100) + "\n")

        log.write("Percentage: {0}".format(1. * valid_bytes / (valid_bytes + (1073741824 - valid_bytes)) * 100) + "\n")
        log.write("Percentage: {0}".format(100 - 1. * valid_bytes / (valid_bytes + (1073741824 - valid_bytes)) * 100) + "\n")
    except:
        pass

    log.close()
