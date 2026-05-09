/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GGPO_LINUX_H_
#define _GGPO_LINUX_H_

#define _POSIX_C_SOURCE 199309L // We need this POSIX standard for clock_gettime

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <stdint.h>

typedef uint64 ProcessID;

inline int strncat_s(
   char *strDestination,
   size_t numberOfElements,
   const char *strSource,
   size_t srcSize
)
{
	size_t min_size = numberOfElements > srcSize ? srcSize : numberOfElements;
	strncat(strDestination, strSource, min_size);
	return errno;
}

inline void DebugBreak()
{
	__builtin_trap();
}

#define MAX_PATH 255

inline ProcessID Platform_GetProcessID() { return (ProcessID)getpid(); }
inline void Platform_AssertFailed(char *msg) {}
uint32 Platform_GetCurrentTimeMS();
int Platform_GetConfigInt(const char* name);
bool Platform_GetConfigBool(const char* name);

#endif
