#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <librsvg-2/librsvg/rsvg.h>
#include <librsvg-2/librsvg/rsvg-cairo.h>

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "input.h"
#include "filetree.h"

#include "scanner-tag.h"
#include "scanner-common.h"

#include "logo.h"

gboolean verbose = TRUE;

#if defined(__GNUC__)
static void exit_program(void) __attribute__ ((noreturn));
#endif

static void exit_program(void)
{
    fprintf(stderr, "Exiting...\n");
    input_deinit();
    exit(0);
}

struct work_data {
    gchar **files;
    guint length;
    int success;
    GtkWidget *progress_bar;
    GtkWidget *drawing_area;
};

static GStaticMutex thread_mutex = G_STATIC_MUTEX_INIT;
static GThread *worker_thread, *bar_thread;

static gboolean rotation_active;

static gpointer do_work(struct work_data *wd)
{
    int result;

    Filetree tree;
    GSList *errors = NULL, *files = NULL;
    tree = filetree_init(wd->files, wd->length, TRUE, FALSE, FALSE, &errors);

    g_slist_foreach(errors, filetree_print_error, &verbose);
    g_slist_foreach(errors, filetree_free_error, NULL);
    g_slist_free(errors);

    filetree_file_list(tree, &files);

    result = loudness_tag(files);
    if (result) {
        gdk_threads_enter();
        GtkWidget *dialog;
        dialog =
            gtk_message_dialog_new(NULL,
                                   GTK_DIALOG_MODAL |
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                   "Some files could not be tagged!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        gdk_threads_leave();
    }

    g_slist_foreach(files, filetree_free_list_entry, NULL);
    g_slist_free(files);
    filetree_destroy(tree);

    g_free(wd->files);
    g_free(wd);

    gdk_threads_enter();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wd->progress_bar), 0.0);
    rotation_active = FALSE;
    gdk_threads_leave();

    g_static_mutex_lock(&thread_mutex);
    worker_thread = NULL;
    g_static_mutex_unlock(&thread_mutex);

    return NULL;
}

static gboolean update_bar_waiting;
static gboolean rotate_logo(GtkWidget *widget);

struct received_data {
    GtkWidget *progress_bar, *drawing_area, *widget;
};

static gpointer update_bar(struct received_data *rd)
{
    for (;;) {
        g_mutex_lock(progress_mutex);
        update_bar_waiting = TRUE;
        g_cond_wait(progress_cond, progress_mutex);
        if (total_frames > 0) {
            gdk_threads_enter();
            if (!rotation_active && elapsed_frames) {
                g_timeout_add(40, (GSourceFunc) rotate_logo, rd->widget);
                rotation_active = TRUE;
            }
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rd->progress_bar),
                                          CLAMP((double) elapsed_frames /
                                                (double) total_frames,
                                                0.0, 1.0));
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
                                 GdkDragContext *drag_context, gint x, gint y,
                                 GtkSelectionData *data, guint info,
                                 guint time, struct received_data *rd)
{
    guint i, no_uris;
    gchar **uris, **files;
    struct work_data *sl;

    (void) widget; (void) x; (void) y; (void) info;

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
    files = g_new(gchar *, no_uris);

    for (i = 0; i < no_uris; ++i) {
        files[i] = g_filename_from_uri(uris[i], NULL, NULL);
    }
    g_strfreev(uris);

    update_bar_waiting = FALSE;
    bar_thread = g_thread_create((GThreadFunc) update_bar, rd, FALSE, NULL);
    /* make sure the update_bar thread is waiting */
    while (!update_bar_waiting) {
        g_thread_yield();
    }
    g_mutex_lock(progress_mutex);
    g_mutex_unlock(progress_mutex);

    sl = g_new(struct work_data, 1);
    sl->files = files;
    sl->length = no_uris;
    sl->progress_bar = rd->progress_bar;
    sl->drawing_area = rd->drawing_area;
    worker_thread = g_thread_create((GThreadFunc) do_work, sl, FALSE, NULL);

    gtk_drag_finish(drag_context, TRUE, FALSE, time);
}

struct popup_data {
    GtkAccelGroup *accel_group;
    GdkEventButton *event;
};

static void handle_popup(GtkWidget *widget, struct popup_data *pd)
{
    GtkWidget *menu, *menu_item;

    (void) widget;

    menu = gtk_menu_new();
    gtk_menu_set_accel_group(GTK_MENU(menu), pd->accel_group);

    /* ... add menu items ... */
    menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, pd->accel_group);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect(menu_item, "activate", gtk_main_quit, NULL);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   (pd->event != NULL) ? pd->event->button : 0,
                   gdk_event_get_time((GdkEvent *) pd->event));
}

static gboolean handle_button_press(GtkWidget *widget, GdkEventButton *event,
                                    GtkAccelGroup *accel_group)
{
    if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == 1) {
            gtk_window_begin_move_drag(GTK_WINDOW
                                       (gtk_widget_get_toplevel(widget)),
                                       (int) event->button, (int) event->x_root,
                                       (int) event->y_root, event->time);
        } else if (event->button == 3) {
            struct popup_data pd = { accel_group, event };
            handle_popup(widget, &pd);
        }
    }

    return FALSE;
}


static gboolean handle_key_press(GtkWidget *widget, GdkEventKey *event,
                                 gpointer callback_data)
{
    (void) widget;
    (void) callback_data;

    if (event->type == GDK_KEY_PRESS
            && (event->keyval == GDK_q || event->keyval == GDK_Q
                || event->keyval == GDK_Escape)) {
        exit_program();
        return TRUE;
    }

    return FALSE;
}

static double rotation_state;

static gboolean rotate_logo(GtkWidget *widget) {
    rotation_state += G_PI / 20;
    if (rotation_state >= 2.0 * G_PI) rotation_state = 0.0;
    gtk_widget_queue_draw(widget);
    if (!rotation_active) {
        rotation_state = 0.0;
        gtk_widget_queue_draw(widget);
    }
    return rotation_active;
}

struct expose_data {
    RsvgHandle *rh;
    RsvgDimensionData rdd;
    double scale_factor;
};

#define DRAW_WIDTH 130.0
#define DRAW_HEIGHT 115.0

static gboolean handle_expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    static double padding_factor = 0.8;
    double new_width, new_height;
    cairo_t *cr = gdk_cairo_create(widget->window);
    struct expose_data *ed = (struct expose_data *) data;

    (void) event;

    new_width = ed->scale_factor * padding_factor * ed->rdd.width;
    new_height = ed->scale_factor * padding_factor * ed->rdd.height;

    cairo_translate(cr,  DRAW_WIDTH / 2.0,  DRAW_HEIGHT / 2.0);
    cairo_rotate(cr, rotation_state);
    cairo_translate(cr, -DRAW_WIDTH / 2.0, -DRAW_HEIGHT / 2.0);

    cairo_translate(cr, (DRAW_WIDTH - new_width) / 2.0,
                        (DRAW_HEIGHT - new_height) / 2.0);
    cairo_scale(cr, ed->scale_factor * padding_factor,
                    ed->scale_factor * padding_factor);

    rsvg_handle_render_cairo(ed->rh, cr);

    cairo_destroy(cr);
    return TRUE;
}

int main(int argc, char *argv[])
{
    GtkWidget *window, *vbox, *drawing_area, *progress_bar;
    GtkAccelGroup *accel_group;
    struct popup_data pd;
    struct expose_data ed;
    struct received_data rd;

    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
    gtk_init(&argc, &argv);
    input_init(argv[0], NULL);
    scanner_init_common();
    setlocale(LC_COLLATE, "");
    setlocale(LC_CTYPE, "");

    accel_group = gtk_accel_group_new();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window), 130, 130);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    gtk_drag_dest_set(window, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(window);
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
    g_signal_connect(window, "delete-event", exit_program, NULL);
    g_signal_connect(window, "destroy", exit_program, NULL);
    g_signal_connect(window, "button-press-event",
                     G_CALLBACK(handle_button_press), accel_group);
    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(handle_key_press), NULL);
    pd.accel_group = accel_group;
    pd.event = NULL;
    g_signal_connect(window, "popup-menu", G_CALLBACK(handle_popup), &pd);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, (int) DRAW_WIDTH,
                                              (int) DRAW_HEIGHT);

    ed.rh = rsvg_handle_new_from_data(test_svg, test_svg_len, NULL);
    rsvg_handle_get_dimensions(ed.rh, &(ed.rdd));
    ed.scale_factor = MIN(DRAW_HEIGHT / ed.rdd.width,
                          DRAW_HEIGHT / ed.rdd.height);
    g_signal_connect(drawing_area, "expose-event",
                     G_CALLBACK(handle_expose), &ed);

    progress_bar = gtk_progress_bar_new();
    gtk_widget_set_size_request(progress_bar, 130, 15);
    rd.progress_bar = progress_bar;
    rd.drawing_area = drawing_area;
    rd.widget = window;
    g_signal_connect(window, "drag-data-received",
                     G_CALLBACK(handle_data_received), &rd);

    gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, TRUE, TRUE, 0);

    gtk_widget_show_all(window);

    gtk_main();
    gdk_threads_leave();
    return 0;
}
