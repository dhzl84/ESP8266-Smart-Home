#if CFG_DEVICE == cThermostat
#define VERSION "0.2.1"
#elif CFG_DEVICE == cS20
#define VERSION "0.1.2"
#else
#error "misconfigured"
#endif

#if defined CFG_DEBUG
#define FW_PREFIX "d"
#else
#define FW_PREFIX "r"
#endif

#define FIRMWARE_VERSION (FW_PREFIX VERSION)
