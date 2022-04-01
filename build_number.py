import datetime
import calendar

VERSION = '2022.04.0'

FILENAME_VERSION_H = 'src/version.h'

dt = datetime.datetime.utcnow()
hex_timestamp = hex(calendar.timegm(dt.timetuple())).lstrip("0x")
dt = dt.strftime("%Y-%m-%d %H:%M:%S")

HEADER_FILE = """
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "{}"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "{}"

#define BUILD_TIME "{}"

#endif  /* VERSION_H_ */
""".format(VERSION, str(hex_timestamp), dt)
with open(FILENAME_VERSION_H, 'w+') as f:
    f.write(HEADER_FILE)
