
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.17.0"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5f160fa8"

#define BUILD_TIME "2020-07-20 21:42:00"

#endif  /* VERSION_H_ */
