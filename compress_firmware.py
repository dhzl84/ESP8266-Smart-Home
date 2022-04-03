"""compress the binary with gzip"""
import gzip
import shutil
import os
Import("env")

def compress_firmware(source, target, env):
    """compress the binary with gzip"""
    # """ Compress ESP8266 firmware using gzip for 'compressed OTA upload' """
    # compressed binary_compressed shall replace the original binary_compressed
    binary_compressed = env.subst("$BUILD_DIR") + os.sep + env.subst("$PROGNAME") + ".bin"
    binary_uncompressed = binary_compressed + '.big'      # store original binary with other name
    if os.path.exists(binary_compressed):
        shutil.move(binary_compressed, binary_uncompressed)  # rename source file
        with open(binary_uncompressed, 'rb') as f_in:
            with gzip.open(binary_compressed, 'wb') as f_out:
                shutil.copyfileobj(f_in, f_out)
        # """ Set modification time on compressed file so incremental build works """
        shutil.copystat(binary_compressed, binary_uncompressed)

    if os.path.exists(binary_compressed):
        org_firmware_size = os.stat(binary_uncompressed).st_size
        gz_firmware_size = os.stat(binary_compressed).st_size
        compression = (gz_firmware_size / org_firmware_size) * 100
        print(f"Compression reduced firmware size to {compression:0.1f} % (was {org_firmware_size} bytes, now {gz_firmware_size} bytes)")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", compress_firmware)
