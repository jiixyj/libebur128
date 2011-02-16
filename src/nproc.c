/* See LICENSE file for copyright and license details. */
#include <glib.h>
#include <unistd.h>

#ifdef G_OS_WIN32
  #include <windows.h>
#endif

long nproc() {
  long ret = 1;
#if defined(G_OS_UNIX) && defined(_SC_NPROCESSORS_ONLN)
  ret = sysconf(_SC_NPROCESSORS_ONLN);
  if (ret < 0) ret = 1;
#elif defined(G_OS_WIN32)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  ret = (long) sysinfo.dwNumberOfProcessors;
#endif
  return ret;
}
