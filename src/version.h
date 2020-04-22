
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.16.x"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5ea042f4"

#define BUILD_TIME "2020-04-22 13:13:24"

#endif  /* VERSION_H_ */
