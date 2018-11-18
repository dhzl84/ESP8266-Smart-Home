#define VERSION "0.5.1"

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FIRMWARE_VERSION (FW_PREFIX VERSION)
