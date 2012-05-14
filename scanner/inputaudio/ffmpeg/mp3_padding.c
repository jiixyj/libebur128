#include "mp3_padding.h"

#include <stdio.h>

#include "input.h"

int input_read_mp3_padding(char *filename, unsigned *start, unsigned *end)
{
    unsigned char buffer[BUFSIZ];
    int fd = input_open_fd(filename);
    int bytes_read, bytes_read_new;

    if (fd < 0) {
        *start = *end = 0;
        return 1;
    }

    while ((bytes_read = input_read_fd(fd, buffer, BUFSIZ)) > 0) {
        unsigned char *match_maybe = memchr(buffer, '\xFF', (size_t) bytes_read);
        while (match_maybe) {
            size_t good_bytes = (size_t) (buffer + bytes_read - match_maybe);
            memmove(buffer, match_maybe, good_bytes);
            bytes_read_new = input_read_fd(fd, buffer + good_bytes,
                               (unsigned int) (BUFSIZ - good_bytes));
            if (bytes_read_new < 0) break;
            bytes_read = (int) good_bytes + bytes_read_new;

            if (bytes_read >= 180 && !memcmp("\xFF\xFB", buffer, 2)
                                  && !memcmp("\x00\x00\x00\x00\x00\x00\x00\x00"
                                             "\x00\x00\x00\x00\x00\x00\x00\x00",
                                             buffer + 4, 16)) {
                *start = (unsigned) (         (buffer[177] << 4) + (buffer[178] >> 4));
                *end   = (unsigned) (((buffer[178] & 0x0f) << 8) +  buffer[179]);

                input_close_fd(fd);
                return 0;
            }

            match_maybe = memchr(buffer + 1, '\xFF', (size_t) (bytes_read - 1));
        }
    }

    input_close_fd(fd);
    return 1;
}
