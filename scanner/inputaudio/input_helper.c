#include "input.h"


#ifndef G_OS_WIN32
    #include <glib.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#include <fcntl.h>

int input_open_fd(const char* filename)
{
#ifdef G_OS_WIN32
    gunichar2 *utf16 = g_utf8_to_utf16(filename, -1, NULL, NULL, NULL);
    int ret = _wopen(utf16, _O_RDONLY | _O_BINARY);
    g_free(utf16);
#else
    int ret = open(filename, O_RDONLY);
#endif
    return ret;
}

void input_close_fd(int fd)
{
#ifdef G_OS_WIN32
    _close(fd);
#else
    close(fd);
#endif
}

int input_read_fd(int fd, void *buf, unsigned int count)
{
#ifdef G_OS_WIN32
    return _read(fd, buf, count);
#else
    return (int) read(fd, buf, count);
#endif
}
