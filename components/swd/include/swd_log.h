#ifndef SWD_LOG_H
#define SWD_LOG_H

#include <stdio.h>

/*
 * Set SWD_VERBOSE to 1 when low-level SWD/Flash transaction logs
 * are required. Keep it 0 for the polished debugger UI.
 */
#ifndef SWD_VERBOSE
#define SWD_VERBOSE 0
#endif

#if SWD_VERBOSE
#define SWD_LOG(...) printf(__VA_ARGS__)
#else
#define SWD_LOG(...) do { } while (0)
#endif

#endif
