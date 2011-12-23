#include "input.h"

#include <fcntl.h>

int input_open_fd(const char* filename)
{
    gunichar2 *utf16 = g_utf8_to_utf16(filename, -1, NULL, NULL, NULL);
    int ret = _wopen(utf16, _O_RDONLY);
    g_free(utf16);
    return ret;
}
