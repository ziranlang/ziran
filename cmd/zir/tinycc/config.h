/* TinyCC configuration for the native ZX engine: an in-memory compiler for
 * the CPU the library runs on, with no files of its own. tccdefs.h is
 * written beside the bundle's headers when a bundle compiles. */
#define TCC_VERSION "0.9.28rc"
#define CONFIG_TCC_PREDEFS 0
#define CONFIG_TCCDIR "/nonexistent"
#define CONFIG_TCC_SYSINCLUDEPATHS ""
#define CONFIG_TCC_LIBPATHS ""
#define CONFIG_TCC_CRTPREFIX ""
#define CONFIG_TCC_BACKTRACE 0
#define CONFIG_TCC_BCHECK 0
#if defined(__arm__)
/* armeabi-v7a (Android) passes floating values in core registers; hard-float
 * Linux ARM defines __ARM_PCS_VFP. */
#define TCC_TARGET_ARM 1
#define TCC_ARM_EABI 1
#define TCC_ARM_VFP 1
#if defined(__ARM_PCS_VFP)
#define TCC_ARM_HARDFLOAT 1
#endif
#endif
