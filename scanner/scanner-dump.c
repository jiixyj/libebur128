#include "scanner-dump.h"

#include <stdlib.h>
#include <stdio.h>

#include "parse_args.h"
#include "scanner-common.h"
#include "nproc.h"

extern gboolean verbose;
static double momentary;
static double shortterm;
static double integrated;
extern gchar *decode_to_file;

static GOptionEntry entries[] =
{
    { "momentary", 'm', 0, G_OPTION_ARG_DOUBLE, &momentary, NULL, NULL },
    { "shortterm", 's', 0, G_OPTION_ARG_DOUBLE, &shortterm, NULL, NULL },
    { "integrated", 'i', 0, G_OPTION_ARG_DOUBLE, &integrated, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

static double interval;
static int r128_mode;
static ebur128_state *st;

static void dump_loudness_info(struct filename_list_node *fln, int *ret)
{
    struct input_ops* ops = NULL;
    struct input_handle* ih = NULL;
    float *buffer = NULL;

    int result;
    static size_t nr_frames_read;
    static size_t frames_counter, frames_needed;

    result = open_plugin(fln->fr->raw, fln->fr->display, &ops, &ih);
    if (result) {
        *ret = EXIT_FAILURE;
        goto free;
    }

    if (!st) {
        st = ebur128_init(ops->get_channels(ih),
                          ops->get_samplerate(ih),
                          r128_mode);
        if (!st) abort();
    } else {
        if (!ebur128_change_parameters(st, ops->get_channels(ih),
                                           ops->get_samplerate(ih))) {
            frames_counter = 0;
        }
    }

    result = ops->allocate_buffer(ih);
    if (result) abort();
    buffer = ops->get_buffer(ih);

    frames_needed = (size_t) (interval * (double) st->samplerate + 0.5);

    while ((nr_frames_read = ops->read_frames(ih))) {
        float* tmp_buffer = buffer;
        double loudness;
        while (nr_frames_read > 0) {
            if (frames_counter + nr_frames_read >= frames_needed) {
                result = ebur128_add_frames_float(st, tmp_buffer,
                                                  frames_needed - frames_counter);
                if (result) abort();
                tmp_buffer += (frames_needed - frames_counter) * st->channels;
                nr_frames_read -= frames_needed - frames_counter;
                frames_counter = 0;
                switch (r128_mode) {
                  case EBUR128_MODE_M:
                    ebur128_loudness_momentary(st, &loudness);
                    printf("%.1f\n", loudness);
                    break;
                  case EBUR128_MODE_S:
                    ebur128_loudness_shortterm(st, &loudness);
                    printf("%.1f\n", loudness);
                    break;
                  case EBUR128_MODE_I:
                    ebur128_loudness_global(st, &loudness);
                    printf("%.1f\n", loudness);
                    break;
                  default:
                    fprintf(stderr, "Invalid mode!\n");
                    abort();
                }
            } else {
                result = ebur128_add_frames_float(st, tmp_buffer, nr_frames_read);
                if (result) abort();
                tmp_buffer += (nr_frames_read) * st->channels;
                frames_counter += nr_frames_read;
                nr_frames_read = 0;
            }
        }
    }

  free:
    if (ih) ops->free_buffer(ih);
    if (!result) ops->close_file(ih);
    if (ih) ops->handle_destroy(&ih);
}

int loudness_dump(GSList *files)
{
    int ret = 0;

    if (momentary > 0.0)
        r128_mode = EBUR128_MODE_M;
    else if (shortterm > 0.0)
        r128_mode = EBUR128_MODE_S;
    else if (integrated > 0.0)
        r128_mode = EBUR128_MODE_I;
    else
        return EXIT_FAILURE;

    g_slist_foreach(files, (GFunc) dump_loudness_info, &ret);
    if (st) ebur128_destroy(&st);

    return ret;
}

gboolean loudness_dump_parse(int *argc, char **argv[])
{
    if (decode_to_file) {
        fprintf(stderr, "Cannot decode to file in dump mode\n");
        return FALSE;
    }

    if (!parse_mode_args(argc, argv, entries)) {
        if (*argc == 1) fprintf(stderr, "Missing arguments\n");
        return FALSE;
    }

    if ((momentary != 0.0) + (shortterm != 0.0) + (integrated != 0.0) != 1 ||
        (interval = momentary + shortterm + integrated) <= 0.0) {
        fprintf(stderr, "Exactly one of -m, -s and -i must be positive!\n");
        return FALSE;
    }

    if (momentary > 0.4 || shortterm > 3.0) {
      fprintf(stderr, "Warning: you may lose samples when specifying "
                      "this interval!\n");
    }
    return TRUE;
}
