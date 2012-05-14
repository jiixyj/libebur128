#include "mp4_padding.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "input.h"

int input_read_mp4_padding(char *filename, unsigned *start, unsigned *end)
{
    unsigned char buffer[BUFSIZ];
    int fd = input_open_fd(filename);
    int bytes_read, bytes_read_new;

    if (fd < 0) {
        *start = *end = 0;
        return 1;
    }

    while ((bytes_read = input_read_fd(fd, buffer, BUFSIZ)) > 0) {
        unsigned char *match_maybe = memchr(buffer, 'i', (size_t) bytes_read);
        while (match_maybe) {
            size_t good_bytes = (size_t) (buffer + bytes_read - match_maybe);
            memmove(buffer, match_maybe, good_bytes);
            bytes_read_new = input_read_fd(fd, buffer + good_bytes,
                               (unsigned int) (BUFSIZ - good_bytes));
            if (bytes_read_new < 0) break;
            bytes_read = (int) good_bytes + bytes_read_new;

            if (bytes_read >= 256 && !memcmp("iTunSMPB", buffer, 8)) {
                unsigned char *space = memchr(buffer, ' ', 128);
                char gap_data[44];
                int i;
                long dummy;

                if (!space) break;

                memcpy(gap_data, space + 1, 44);
                for (i = 0; i < 43; ++i)
                    if (gap_data[i] == ' ') gap_data[i] = '\0';
                gap_data[43] = '\0';

                dummy = strtol(gap_data + 9,  NULL, 16);
                *start = dummy > 0 && dummy <= UINT_MAX ? (unsigned) dummy : 0;

                dummy = strtol(gap_data + 18, NULL, 16);
                *end   = dummy > 0 && dummy <= UINT_MAX ? (unsigned) dummy : 0;

                input_close_fd(fd);
                return 0;
            }

            match_maybe = memchr(buffer + 1, 'i', (size_t) (bytes_read - 1));
        }
    }

    input_close_fd(fd);
    return 1;
}
