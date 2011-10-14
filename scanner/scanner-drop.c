#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <librsvg-2/librsvg/rsvg.h>
#include <librsvg-2/librsvg/rsvg-cairo.h>

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <assert.h>

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
};

static GStaticMutex thread_mutex = G_STATIC_MUTEX_INIT;
static GThread *worker_thread;




/* result list */

typedef struct {
    const gchar *filename;
    const float  album_gain;
    const float  track_gain;
    const float  album_peak;
    const float  track_peak;
} ResultEntry;

enum {
    COLUMN_IS_TAGGED,
    COLUMN_FILENAME,
    COLUMN_ALBUM_GAIN,
    COLUMN_TRACK_GAIN,
    COLUMN_ALBUM_PEAK,
    COLUMN_TRACK_PEAK,
    NUM_COLUMNS
};

static GtkTreeModel *create_result_list_model(GSList *files)
{
    GtkListStore *store;
    GtkTreeIter iter;

    store = gtk_list_store_new(NUM_COLUMNS,
                               GDK_TYPE_PIXBUF,
                               G_TYPE_STRING,
                               G_TYPE_FLOAT,
                               G_TYPE_FLOAT,
                               G_TYPE_FLOAT,
                               G_TYPE_FLOAT);

    /* add data to the list store */
    while (files) {
        struct filename_list_node *fln;
        struct file_data *fd;
        fln = (struct filename_list_node *) files->data;
        fd = (struct file_data *) fln->d;

        if (fd->scanned) {
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               COLUMN_FILENAME, fln->fr->display,
                               COLUMN_ALBUM_GAIN, fd->gain_album,
                               COLUMN_TRACK_GAIN, clamp_rg(RG_REFERENCE_LEVEL - fd->loudness),
                               COLUMN_ALBUM_PEAK, fd->peak_album,
                               COLUMN_TRACK_PEAK, fd->peak,
                               -1);
        }
        files = g_slist_next(files);
    }

    return GTK_TREE_MODEL(store);
}

static void float_to_rg_display(GtkTreeViewColumn *tree_column,
                                GtkCellRenderer   *cell,
                                GtkTreeModel      *tree_model,
                                GtkTreeIter       *iter,
                                gpointer           data)
{
  float d;
  gchar *text;

  /* Get the double value from the model. */
  gtk_tree_model_get(tree_model, iter, (gint) data, &d, -1);
  /* Now we can format the value ourselves. */
  text = g_strdup_printf("%+.2f dB", d);
  g_object_set(cell, "text", text, NULL);
  g_free(text);
}

static void result_view_add_colums(GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_pixbuf_new();
  // g_object_set(renderer, "icon-name", GTK_STOCK_APPLY, NULL);
  column = gtk_tree_view_column_new_with_attributes(
                  "Tagged", renderer, "pixbuf", COLUMN_IS_TAGGED, NULL);
  gtk_tree_view_column_set_sort_column_id(column, COLUMN_IS_TAGGED);
  gtk_tree_view_append_column(treeview, column);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  column = gtk_tree_view_column_new_with_attributes(
                        "File", renderer, "text", COLUMN_FILENAME, NULL);
  gtk_tree_view_column_set_sort_column_id(column, COLUMN_FILENAME);
  gtk_tree_view_append_column(treeview, column);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_column_set_min_width(column, 0);
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(
                  "Album Gain", renderer, "text", COLUMN_ALBUM_GAIN, NULL);
  gtk_tree_view_column_set_sort_column_id(column, COLUMN_ALBUM_GAIN);
  gtk_tree_view_append_column(treeview, column);
  gtk_tree_view_column_set_cell_data_func(column, renderer,
                                          float_to_rg_display,
                                          (gpointer) COLUMN_ALBUM_GAIN, NULL);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(
                  "Track Gain", renderer, "text", COLUMN_TRACK_GAIN, NULL);
  gtk_tree_view_column_set_sort_column_id(column, COLUMN_TRACK_GAIN);
  gtk_tree_view_append_column(treeview, column);
  gtk_tree_view_column_set_cell_data_func(column, renderer,
                                          float_to_rg_display,
                                          (gpointer) COLUMN_TRACK_GAIN, NULL);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(
                  "Album Peak", renderer, "text", COLUMN_ALBUM_PEAK, NULL);
  gtk_tree_view_column_set_sort_column_id(column, COLUMN_ALBUM_PEAK);
  gtk_tree_view_append_column(treeview, column);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(
                  "Track Peak", renderer, "text", COLUMN_TRACK_PEAK, NULL);
  gtk_tree_view_column_set_sort_column_id(column, COLUMN_TRACK_PEAK);
  gtk_tree_view_append_column(treeview, column);
}

static GtkTreeModel *tag_model; /* Must be updated before function call */
static void tag_files_and_update_model(GSList *files) {
    printf("test %p\n", (void *) files);
}

static GtkWidget *show_result_list(GSList *files) {
    GtkWidget *window;
    GtkTreeModel *model;
    GtkWidget *vbox;
    GtkWidget *sw;
    GtkWidget *treeview;

    GtkWidget *lower_box, *lower_box_fill;
    GtkWidget *button_box, *tag_button, *ok_button;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Scanning Result");

    g_signal_connect(window, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &window);
    gtk_container_set_border_width(GTK_CONTAINER(window), 8);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (sw),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (sw),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    /* create tree model */
    model = create_result_list_model(files);

    /* create tree view */
    treeview = gtk_tree_view_new_with_model(model);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
                                    COLUMN_FILENAME);

    g_object_unref(model);

    /* add columns to the tree view */
    result_view_add_colums(GTK_TREE_VIEW(treeview));
    gtk_container_add(GTK_CONTAINER(sw), treeview);

    /* create tag button */
    lower_box = gtk_hbox_new(FALSE, 8);
    ok_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect_swapped(ok_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), window);
    tag_button = gtk_button_new_with_mnemonic("_Tag files");
    tag_model = model;
    g_signal_connect(tag_button, "clicked",
                     G_CALLBACK(tag_files_and_update_model), files);
    lower_box_fill = gtk_alignment_new(0, 0.5f, 1, 1);
    button_box = gtk_hbutton_box_new();
    gtk_container_add(GTK_CONTAINER(button_box), tag_button);
    gtk_container_add(GTK_CONTAINER(button_box), ok_button);
    gtk_box_set_spacing(GTK_BOX(button_box), 6);

    gtk_box_pack_start(GTK_BOX(lower_box), lower_box_fill, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(lower_box), button_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), lower_box, FALSE, FALSE, 0);

    /* finish & show */
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 400);
    gtk_widget_show_all(window);

    return window;
}




/* drop handling and work */

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
    filetree_remove_common_prefix(files);

    result = scan_files(files);
    if (result) {
        gdk_threads_enter();
        show_result_list(files);
        gdk_threads_leave();
    }

    g_slist_foreach(files, filetree_free_list_entry, NULL);
    g_slist_free(files);
    filetree_destroy(tree);

    g_free(wd->files);
    g_free(wd);

    g_static_mutex_lock(&thread_mutex);
    worker_thread = NULL;
    g_static_mutex_unlock(&thread_mutex);

    return NULL;
}

static gboolean rotate_logo(GtkWidget *widget);

static void handle_data_received(GtkWidget *widget,
                                 GdkDragContext *drag_context, gint x, gint y,
                                 GtkSelectionData *data, guint info,
                                 guint time, gpointer unused)
{
    guint i, no_uris;
    gchar **uris, **files;
    struct work_data *sl;

    (void) widget; (void) x; (void) y; (void) info; (void) unused;

    g_static_mutex_lock(&thread_mutex);
    if (worker_thread) {
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

    sl = g_new(struct work_data, 1);
    sl->files = files;
    sl->length = no_uris;
    worker_thread = g_thread_create((GThreadFunc) do_work, sl, FALSE, NULL);

    gtk_drag_finish(drag_context, TRUE, FALSE, time);
}





/* input handling */

static GtkWidget *popup_menu;

static void handle_popup(GtkWidget *widget, GdkEventButton *event)
{
    (void) widget;
    gtk_widget_show_all(popup_menu);
    gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
                   (event != NULL) ? event->button : 0,
                   gdk_event_get_time((GdkEvent *) event));
}

static gboolean handle_button_press(GtkWidget *widget, GdkEventButton *event,
                                    gpointer *unused)
{
    (void) unused;

    if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == 1) {
            gtk_window_begin_move_drag(GTK_WINDOW
                                       (gtk_widget_get_toplevel(widget)),
                                       (int) event->button, (int) event->x_root,
                                       (int) event->y_root, event->time);
        } else if (event->button == 3) {
            handle_popup(widget, event);
        }
    }

    return FALSE;
}



/* logo drawing */

static double rotation_state;
static gboolean rotation_active;

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

#define DRAW_WIDTH 130.0
#define DRAW_HEIGHT 115.0

static gboolean handle_expose(GtkWidget *widget, GdkEventExpose *event,
                              RsvgHandle *rh)
{
    static double scale_factor;
    static RsvgDimensionData rdd;

    double new_width, new_height;
    cairo_t *cr = gdk_cairo_create(widget->window);
    (void) event;

    if (scale_factor <= 0.0) {
        rsvg_handle_get_dimensions(rh, &rdd);
        scale_factor = 0.8 * MIN(DRAW_HEIGHT / rdd.width,
                                 DRAW_HEIGHT / rdd.height);
    }

    new_width = scale_factor * rdd.width;
    new_height = scale_factor * rdd.height;

    cairo_translate(cr,  DRAW_WIDTH / 2.0,  DRAW_HEIGHT / 2.0);
    cairo_rotate(cr, rotation_state);
    cairo_translate(cr, -DRAW_WIDTH / 2.0, -DRAW_HEIGHT / 2.0);

    cairo_translate(cr, (DRAW_WIDTH - new_width) / 2.0,
                        (DRAW_HEIGHT - new_height) / 2.0);
    cairo_scale(cr, scale_factor, scale_factor);

    rsvg_handle_render_cairo(rh, cr);

    cairo_destroy(cr);
    return TRUE;
}

static gpointer update_bar(GtkWidget *widget)
{
    double frac, new_frac;
    GtkWidget *vbox = gtk_bin_get_child(GTK_BIN(widget));
    GList *children = gtk_container_get_children(GTK_CONTAINER(vbox));
    GtkWidget *progress_bar = GTK_IS_PROGRESS_BAR(children->data) ?
                              children->data : g_list_next(children)->data;
    g_list_free(children);

    for (;;) {
        g_mutex_lock(progress_mutex);
        g_cond_wait(progress_cond, progress_mutex);
        if (total_frames > 0) {
            gdk_threads_enter();
            if (!rotation_active && elapsed_frames) {
                g_timeout_add(40, (GSourceFunc) rotate_logo, widget);
                rotation_active = TRUE;
            }
            frac = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progress_bar));
            new_frac = CLAMP((double) elapsed_frames / (double) total_frames,
                             0.0, 1.0);
            if (ABS(frac - new_frac) > 1.0 / DRAW_WIDTH) {
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),
                                              new_frac);
            }
            if (total_frames == elapsed_frames) {
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
                rotation_active = FALSE;
            }
            gdk_threads_leave();
        }
        g_mutex_unlock(progress_mutex);
    }

    return NULL;
}



static GtkActionEntry action_entries[] =
{
  { "FileMenuAction", NULL, "_File",
    NULL, NULL, NULL },

  { "QuitAction", GTK_STOCK_QUIT,
    "_Quit", "<control>Q",
    "Quit",
    G_CALLBACK(exit_program) }
};

static guint n_action_entries = G_N_ELEMENTS(action_entries);

static const char *ui =
"<ui>"
"  <menubar name=\"MainMenu\">"
"    <menu name=\"FileMenu\" action=\"FileMenuAction\">"
"      <menuitem name=\"Quit\" action=\"QuitAction\" />"
"      <placeholder name=\"FileMenuAdditions\" />"
"    </menu>"
"  </menubar>"
"</ui>"
;



int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *drawing_area;
    GtkWidget *progress_bar;
    GThread *bar_thread;
    GtkActionGroup *action_group;
    GtkUIManager *menu_manager;
    GError *error;
    RsvgHandle *rh;

    /* initialization */
    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
    gtk_init(&argc, &argv);
    input_init(argv[0], NULL);
    scanner_init_common();
    setlocale(LC_COLLATE, "");
    setlocale(LC_CTYPE, "");

    /* set up widgets */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    vbox = gtk_vbox_new(FALSE, 0);
    drawing_area = gtk_drawing_area_new();
    progress_bar = gtk_progress_bar_new();
    action_group = gtk_action_group_new("Actions");
    menu_manager = gtk_ui_manager_new();

    /* set up window */
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window), 130, 130);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    gtk_drag_dest_set(window, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(window);
    g_signal_connect(window, "delete-event", exit_program, NULL);
    g_signal_connect(window, "destroy", exit_program, NULL);
    g_signal_connect(window, "button-press-event",
                     G_CALLBACK(handle_button_press), NULL);
    g_signal_connect(window, "popup-menu", G_CALLBACK(handle_popup), NULL);
    g_signal_connect(window, "drag-data-received",
                     G_CALLBACK(handle_data_received), NULL);

    /* set up vbox */
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, TRUE, TRUE, 0);

    /* set up drawing area */
    gtk_widget_set_size_request(drawing_area, (int) DRAW_WIDTH,
                                              (int) DRAW_HEIGHT);
    rh = rsvg_handle_new_from_data(test_svg, test_svg_len, NULL);
    g_signal_connect(drawing_area, "expose-event",
                     G_CALLBACK(handle_expose), rh);

    /* set up progress bar */
    gtk_widget_set_size_request(progress_bar, 130, 15);

    /* set up bar thread */
    bar_thread = g_thread_create((GThreadFunc) update_bar, window, FALSE, NULL);

    /* set up action group */
    gtk_action_group_add_actions(action_group, action_entries,
                                 n_action_entries, NULL);

    /* set up menu manager */
    gtk_ui_manager_insert_action_group(menu_manager, action_group, 0);
    error = NULL;
    gtk_ui_manager_add_ui_from_string(menu_manager, ui, -1, &error);
    if (error) {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
    }
    gtk_window_add_accel_group(GTK_WINDOW(window),
                               gtk_ui_manager_get_accel_group(menu_manager));
    popup_menu = gtk_menu_item_get_submenu(
                     GTK_MENU_ITEM(gtk_ui_manager_get_widget(
                                       menu_manager, "/MainMenu/FileMenu")));

    gtk_widget_show_all(window);

    gtk_main();
    gdk_threads_leave();
    return 0;
}
