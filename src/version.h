
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.20.x"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5f6cf031"

#define BUILD_TIME "2020-09-24 19:14:57"

#endif  /* VERSION_H_ */
