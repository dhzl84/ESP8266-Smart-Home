
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.16.x"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5eabce6b"

#define BUILD_TIME "2020-05-01 07:23:23"

#endif  /* VERSION_H_ */
