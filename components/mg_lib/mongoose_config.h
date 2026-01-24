#if !defined(MG_ARCH) || (MG_ARCH == MG_ARCH_CUSTOM)
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#ifdef USE_ESP8266
#define USE_ESPHOME_SOCKETS 
#endif

#define MG_IO_SIZE  512
#define MG_ENABLE_CUSTOM_MILLIS 0
#define MG_PATH_MAX 64
#define MG_ENABLE_SOCKET 0
#define MG_ENABLE_DIRLIST 0
#define MG_ENABLE_PROFILE 0
#define MG_ENABLE_POSIX_FS 0
#define MG_ENABLE_TCPIP 0 
#define MG_DATA_SIZE 8

#ifdef USE_RP2040
//#define MG_TLS MG_TLS_NONE
#define USE_ESPHOME_SOCKETS 
#endif

// #define MG_ENABLE_CUSTOM_CALLOC 1

// void *mg_calloc(size_t count, size_t size) {
//   return calloc(count, size);
// }

// void mg_free(void *ptr) {
//   free(ptr);
// }

#endif
