/* See LICENSE file for copyright and license details. */
#include "nproc.h"

#include <glib.h>

#if defined(G_OS_UNIX)
  #include <unistd.h>
#elif defined(G_OS_WIN32)
  #include <windows.h>
#endif

int nproc(void) {
  int ret = 1;
#if defined(G_OS_UNIX) && defined(_SC_NPROCESSORS_ONLN)
  ret = (int) sysconf(_SC_NPROCESSORS_ONLN);
  if (ret < 0) ret = 1;
#elif defined(G_OS_WIN32)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  ret = sysinfo.dwNumberOfProcessors;
#endif
  return ret;
}
