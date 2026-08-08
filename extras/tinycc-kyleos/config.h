#ifndef KYLEOS_TCC_CONFIG_H
#define KYLEOS_TCC_CONFIG_H

#define TCC_VERSION "0.9.28rc-kyleos"
#define CC_NAME CC_gcc
#define GCC_MAJOR 14
#define GCC_MINOR 0
#define TCC_TARGET_X86_64 1
#define CONFIG_TCC_STATIC 1
#define CONFIG_TCC_BACKTRACE 0
#define CONFIG_TCC_BCHECK 0
#define CONFIG_TCC_SEMLOCK 0
#define CONFIG_TCCDIR "/usr/lib/tcc"
#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include:/usr/include"
#define CONFIG_TCC_LIBPATHS "/usr/lib:{B}"
#define CONFIG_TCC_CRTPREFIX "/usr/lib"
#define CONFIG_TCC_ELFINTERP "-"
#define CONFIG_TCC_SWITCHES "-static"
#define CONFIG_TCC_PREDEFS 1
#define TCC_LIBTCC1 "libtcc1.a"

#endif
