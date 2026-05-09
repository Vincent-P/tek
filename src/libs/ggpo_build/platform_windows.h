/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GGPO_WINDOWS_H_
#define _GGPO_WINDOWS_H_

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include "types.h"


typedef uint64 ProcessID;

inline ProcessID Platform_GetProcessID() { return (ProcessID)GetCurrentProcessId(); }
   inline void Platform_AssertFailed(char *msg) { MessageBoxA(NULL, msg, "GGPO Assertion Failed", MB_OK | MB_ICONEXCLAMATION); }
   inline uint32 Platform_GetCurrentTimeMS() { return timeGetTime(); }
   int Platform_GetConfigInt(const char* name);
   bool Platform_GetConfigBool(const char* name);

#endif
