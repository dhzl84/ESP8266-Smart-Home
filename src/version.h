
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.20.x"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5f6f016d"

#define BUILD_TIME "2020-09-26 08:53:01"

#endif  /* VERSION_H_ */
