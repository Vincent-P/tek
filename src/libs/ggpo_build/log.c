/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"

static FILE *logfile = NULL;

void LogFlush()
{
   if (logfile) {
      fflush(logfile);
   }
}

static char logbuf[4 * 1024 * 1024];

void Log(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   Logv(fmt, args);
   va_end(args);
}

void Logv(const char *fmt, va_list args)
{
    char logbuf2[256] = { 0 };
    vsnprintf(logbuf2, 256, fmt, args);
    OutputDebugStringA(logbuf2);

   if (!Platform_GetConfigBool("ggpo.log") || Platform_GetConfigBool("ggpo.log.ignore")) {
      return;
   }
   if (!logfile) {
      snprintf(logbuf, ARRAY_SIZE(logbuf), "log-%llu.log", Platform_GetProcessID());
      logfile = fopen(logbuf, "w");
   }
   LogvFile(logfile, fmt, args);
}

void LogvFile(FILE *fp, const char *fmt, va_list args)
{
   if (Platform_GetConfigBool("ggpo.log.timestamps")) {
      static int start = 0;
      int t = 0;
      if (!start) {
         start = Platform_GetCurrentTimeMS();
      } else {
         t = Platform_GetCurrentTimeMS() - start;
      }
      fprintf(fp, "%d.%03d : ", t / 1000, t % 1000);
   }

   vfprintf(fp, fmt, args);
   fflush(fp);

   vsnprintf(logbuf, ARRAY_SIZE(logbuf), fmt, args);
}
