
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.19.0"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5f6cff87"

#define BUILD_TIME "2020-09-24 20:20:23"

#endif  /* VERSION_H_ */
