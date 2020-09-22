
#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.18.0"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#define BUILD_NUMBER "5f69f231"

#define BUILD_TIME "2020-09-22 12:46:41"

#endif  /* VERSION_H_ */
