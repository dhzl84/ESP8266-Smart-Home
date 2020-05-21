
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.16.0"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5ec670c6"

#define BUILD_TIME "2020-05-21 12:15:02"

#endif  /* VERSION_H_ */
