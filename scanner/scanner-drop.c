#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "input.h"
#include "filetree.h"
#include "scanner-common.h"

gboolean verbose = TRUE;

static void exit_program(void)
{
    fprintf(stderr, "Exiting...\n");
    input_deinit();
    exit(0);
}

struct work_data
{
    gchar **filenames;
    guint length;
    int success;
};

static GStaticMutex thread_mutex = G_STATIC_MUTEX_INIT;
static GThread *worker_thread, *bar_thread;

static gpointer do_work(gpointer data)
{
    struct work_data *wd = (struct work_data *) data;
    int result;

    Filetree tree;
    GSList *errors = NULL, *files = NULL;
    tree = filetree_init(wd->filenames, wd->length,
                         TRUE, FALSE, FALSE, &errors);

    g_slist_foreach(errors, filetree_print_error, &verbose);
    g_slist_foreach(errors, filetree_free_error, NULL);
    g_slist_free(errors);

    filetree_file_list(tree, &files);

    result = loudness_tag(files);
    if (result) {
        gdk_threads_enter();
        GtkWidget *dialog;
        dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Some files could not be tagged!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        gdk_threads_leave();
    }

    g_slist_foreach(files, filetree_free_list_entry, NULL);
    g_slist_free(files);
    filetree_destroy(tree);

    g_free(wd->filenames);
    g_free(wd);

    g_static_mutex_lock(&thread_mutex);
    worker_thread = NULL;
    g_static_mutex_unlock(&thread_mutex);

    return NULL;
}
static gboolean update_bar_waiting;
static gpointer update_bar(gpointer data)
{
    GtkProgressBar *progress_bar = (GtkProgressBar *) data;
    for (;;) {
        g_mutex_lock(progress_mutex);
        update_bar_waiting = TRUE;
        g_cond_wait(progress_cond, progress_mutex);
        if (total_frames > 0) {
            gdk_threads_enter();
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), CLAMP((double) elapsed_frames / (double) total_frames, 0.0, 1.0));
            gdk_threads_leave();
        }
        if (total_frames == elapsed_frames) {
            g_mutex_unlock(progress_mutex);
            break;
        }
        g_mutex_unlock(progress_mutex);
    }

    g_static_mutex_lock(&thread_mutex);
    bar_thread = NULL;
    g_static_mutex_unlock(&thread_mutex);

    return NULL;
}

static void handle_data_received(GtkWidget *widget,
                                 GdkDragContext *drag_context, gint x,
                                 gint y, GtkSelectionData *data,
                                 guint info, guint time,
                                 gpointer progress_bar)
{
    guint i, no_uris;
    gchar **uris, **filenames;
    struct work_data *sl;

    g_static_mutex_lock(&thread_mutex);
    if (worker_thread || bar_thread) {
        g_static_mutex_unlock(&thread_mutex);
        gtk_drag_finish(drag_context, FALSE, FALSE, time);
        return;
    }
    g_static_mutex_unlock(&thread_mutex);
    uris = g_uri_list_extract_uris((const gchar *)
                                   gtk_selection_data_get_data(data));
    no_uris = g_strv_length(uris);
    filenames = g_new(gchar *, no_uris);

    for (i = 0; i < no_uris; ++i) {
        filenames[i] = g_filename_from_uri(uris[i], NULL, NULL);
    }
    g_strfreev(uris);

    update_bar_waiting = FALSE;
    bar_thread = g_thread_create(update_bar, progress_bar, FALSE, NULL);
    /* make sure the update_bar thread is waiting */
    while (!update_bar_waiting) g_thread_yield();
    g_mutex_lock(progress_mutex);
    g_mutex_unlock(progress_mutex);

    sl = g_new(struct work_data, 1);
    sl->filenames = filenames;
    sl->length = no_uris;
    worker_thread = g_thread_create(do_work, sl, FALSE, NULL);

    gtk_drag_finish(drag_context, TRUE, FALSE, time);
}

static gboolean handle_key_press(GtkWidget *widget, GdkEventKey *event,
                                 gpointer callback_data)
{
    if (event->type == GDK_KEY_PRESS
            && (event->keyval == GDK_q || event->keyval == GDK_Q
                || event->keyval == GDK_Escape)) {
        exit_program();
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char *argv[])
{
    GtkWidget *window, *progress_bar;

    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
    gtk_init(&argc, &argv);
    input_init(argv[0], NULL);
    scanner_init_common();
    setlocale(LC_COLLATE, "");
    setlocale(LC_CTYPE, "");

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Loudness Drop");
    gtk_window_set_default_size(GTK_WINDOW(window), 250, 250);
    g_signal_connect(window, "delete-event", exit_program, NULL);
    g_signal_connect(window, "destroy", exit_program, NULL);

    progress_bar = gtk_progress_bar_new();
    gtk_container_add(GTK_CONTAINER(window), progress_bar);

    gtk_drag_dest_set(window, GTK_DEST_DEFAULT_ALL, NULL, 0,
                      GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(window);

    g_signal_connect(window, "drag-data-received",
                     G_CALLBACK(handle_data_received), progress_bar);
    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(handle_key_press), NULL);

    gtk_widget_show_all(window);
    gtk_main();
    gdk_threads_leave();
    return 0;
}
