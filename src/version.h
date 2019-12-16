#ifndef SRC_VERSION_H_
#define SRC_VERSION_H_

#define VERSION "0.11.1"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FW (FW_PREFIX VERSION)

#endif  // SRC_VERSION_H_
