
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.17.0"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5ec66f51"

#define BUILD_TIME "2020-05-21 12:08:49"

#endif  /* VERSION_H_ */
