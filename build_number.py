"""Generate the version and verion.h file during build."""

import datetime
import calendar

YEAR = '2022'
MONTH = '06'
PATCH = '1'

VERSION = ".".join([YEAR,MONTH,PATCH])

FILENAME_VERSION_H = 'src/version.h'

dt = datetime.datetime.utcnow()
hex_timestamp = hex(calendar.timegm(dt.timetuple())).lstrip("0x")
dt = dt.strftime("%Y-%m-%d %H:%M:%S")

HEADER_FILE = f"""
#ifndef VERSION_H_
#define VERSION_H_

#define FW_YEAR  = int16_t({YEAR})
#define FW_MONTH = int16_t({MONTH})
#define FW_PATCH = int8_t({PATCH})

#define VERSION "{VERSION}"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "{str(hex_timestamp)}"

#define BUILD_TIME "{dt}"

#endif  /* VERSION_H_ */
"""

with open(FILENAME_VERSION_H, 'w+',encoding='UTF-8') as f:
    f.write(HEADER_FILE)
    f.close()

with open("version", 'w+',encoding='UTF-8') as f:
    f.write(VERSION)
    f.close()

print(f"Build version: {VERSION}")
