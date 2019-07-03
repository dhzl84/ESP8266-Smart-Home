#ifndef VERSION_H_
#define VERSION_H_

#define VERSION "0.9.3"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#endif  // VERSION_H_
