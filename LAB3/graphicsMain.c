#include <gtk/gtk.h>
#include <math.h>

typedef struct {
    gboolean is_circle;
    gboolean is_polygon;
    double x, y;
    double size;
    double *points;
    int num_points;
    double color[3]; // RGB color for shape
} Shape;

typedef struct {
    int current_page;
    int num_pages;
    GList **pages;
    gboolean draw_circle;
    gboolean draw_rectangle;
    gboolean draw_polygon;
    gboolean eraser_mode;
    double current_color[3]; // Current color chosen from color picker
} AppData;

static GtkWidget *drawing_area;

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static void button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void on_circle_button_clicked(GtkWidget *button, gpointer user_data);
static void on_rectangle_button_clicked(GtkWidget *button, gpointer user_data);
static void on_polygon_button_clicked(GtkWidget *button, gpointer user_data);
static void on_eraser_button_clicked(GtkWidget *button, gpointer user_data);
static void on_color_set(GtkColorButton *button, gpointer user_data);
static void on_next_page_button_clicked(GtkWidget *button, gpointer user_data);
static void on_prev_page_button_clicked(GtkWidget *button, gpointer user_data);
static void on_add_page_button_clicked(GtkWidget *button, gpointer user_data);
static void free_object_list(GList *list);

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppData *app = (AppData *)user_data;

    for (GList *item = app->pages[app->current_page]; item != NULL; item = item->next) {
        Shape *shape = (Shape *)item->data;
        cairo_set_source_rgb(cr, shape->color[0], shape->color[1], shape->color[2]);

        if (shape->is_circle) {
            cairo_arc(cr, shape->x, shape->y, shape->size, 0, 2 * G_PI);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_stroke(cr);
        } else if (shape->is_polygon && shape->num_points > 1) {
            cairo_move_to(cr, shape->points[0], shape->points[1]);
            for (int i = 1; i < shape->num_points; i++) {
                cairo_line_to(cr, shape->points[i * 2], shape->points[i * 2 + 1]);
            }
            cairo_close_path(cr);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_stroke(cr);
        } else {
            cairo_rectangle(cr, shape->x, shape->y, shape->size, shape->size);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_stroke(cr);
        }
    }

    char page_number_text[50];
    snprintf(page_number_text, sizeof(page_number_text), "Page %d of %d", app->current_page + 1, app->num_pages);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);
    cairo_move_to(cr, 10, 30);
    cairo_show_text(cr, page_number_text);

    return FALSE;
}

static void button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AppData *app = (AppData *)user_data;

    if (app->eraser_mode) {
        for (GList *item = app->pages[app->current_page]; item != NULL; item = item->next) {
            Shape *existing_shape = (Shape *)item->data;
            gboolean shape_removed = FALSE;

            if (existing_shape->is_circle) {
                double dx = event->x - existing_shape->x;
                double dy = event->y - existing_shape->y;
                if (sqrt(dx * dx + dy * dy) <= existing_shape->size) {
                    app->pages[app->current_page] = g_list_remove(app->pages[app->current_page], existing_shape);
                    g_free(existing_shape);
                    shape_removed = TRUE;
                }
            } else if (existing_shape->is_polygon) {
                double min_x = existing_shape->points[0];
                double max_x = existing_shape->points[0];
                double min_y = existing_shape->points[1];
                double max_y = existing_shape->points[1];
                for (int i = 1; i < existing_shape->num_points; i++) {
                    double x = existing_shape->points[i * 2];
                    double y = existing_shape->points[i * 2 + 1];
                    if (x < min_x) min_x = x;
                    if (x > max_x) max_x = x;
                    if (y < min_y) min_y = y;
                    if (y > max_y) max_y = y;
                }
                if (event->x >= min_x && event->x <= max_x && event->y >= min_y && event->y <= max_y) {
                    app->pages[app->current_page] = g_list_remove(app->pages[app->current_page], existing_shape);
                    g_free(existing_shape->points);
                    g_free(existing_shape);
                    shape_removed = TRUE;
                }
            } else {
                if (event->x >= existing_shape->x && event->x <= existing_shape->x + existing_shape->size &&
                    event->y >= existing_shape->y && event->y <= existing_shape->y + existing_shape->size) {
                    app->pages[app->current_page] = g_list_remove(app->pages[app->current_page], existing_shape);
                    g_free(existing_shape);
                    shape_removed = TRUE;
                }
            }

            if (shape_removed) {
                gtk_widget_queue_draw(drawing_area);
                break;
            }
        }
        return;
    }

    Shape *shape = g_malloc(sizeof(Shape));
    shape->color[0] = app->current_color[0];
    shape->color[1] = app->current_color[1];
    shape->color[2] = app->current_color[2];

    if (app->draw_circle) {
        shape->is_circle = TRUE;
        shape->is_polygon = FALSE;
        shape->x = event->x;
        shape->y = event->y;
        shape->size = 25;
        shape->points = NULL;
        shape->num_points = 0;
    } else if (app->draw_rectangle) {
        shape->is_circle = FALSE;
        shape->is_polygon = FALSE;
        shape->x = event->x;
        shape->y = event->y;
        shape->size = 50;
        shape->points = NULL;
        shape->num_points = 0;
    } else if (app->draw_polygon) {
        shape->is_circle = FALSE;
        shape->is_polygon = TRUE;
        shape->x = event->x;
        shape->y = event->y;
        shape->points = g_malloc(2 * sizeof(double));
        shape->points[0] = event->x;
        shape->points[1] = event->y;
        shape->num_points = 1;
    }

    app->pages[app->current_page] = g_list_append(app->pages[app->current_page], shape);
    gtk_widget_queue_draw(drawing_area);
}

static void on_circle_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    app->draw_circle = TRUE;
    app->draw_rectangle = FALSE;
    app->draw_polygon = FALSE;
    app->eraser_mode = FALSE;
}

static void on_rectangle_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    app->draw_circle = FALSE;
    app->draw_rectangle = TRUE;
    app->draw_polygon = FALSE;
    app->eraser_mode = FALSE;
}

static void on_polygon_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    app->draw_circle = FALSE;
    app->draw_rectangle = FALSE;
    app->draw_polygon = TRUE;
    app->eraser_mode = FALSE;
}

static void on_eraser_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    app->draw_circle = FALSE;
    app->draw_rectangle = FALSE;
    app->draw_polygon = FALSE;
    app->eraser_mode = TRUE;
}

static void on_color_set(GtkColorButton *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    app->current_color[0] = color.red;
    app->current_color[1] = color.green;
    app->current_color[2] = color.blue;
}

static void on_next_page_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    if (app->current_page < app->num_pages - 1) {
        app->current_page++;
        gtk_widget_queue_draw(drawing_area);
    }
}

static void on_prev_page_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    if (app->current_page > 0) {
        app->current_page--;
        gtk_widget_queue_draw(drawing_area);
    }
}

static void on_add_page_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    app->pages = g_realloc(app->pages, (app->num_pages + 1) * sizeof(GList *));
    app->pages[app->num_pages] = NULL;
    app->num_pages++;
    app->current_page = app->num_pages - 1;
    gtk_widget_queue_draw(drawing_area);
}

static void free_object_list(GList *list) {
    for (GList *item = list; item != NULL; item = item->next) {
        Shape *shape = (Shape *)item->data;
        if (shape->points) g_free(shape->points);
        g_free(shape);
    }
    g_list_free(list);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    AppData *app = g_malloc(sizeof(AppData));
    app->current_page = 0;
    app->num_pages = 1;
    app->pages = g_malloc(sizeof(GList *));
    app->pages[0] = NULL;
    app->draw_circle = FALSE;
    app->draw_rectangle = FALSE;
    app->draw_polygon = FALSE;
    app->eraser_mode = FALSE;
    app->current_color[0] = 0;
    app->current_color[1] = 0;
    app->current_color[2] = 0;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 800, 600);
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw_event), app);
    g_signal_connect(G_OBJECT(drawing_area), "button-press-event", G_CALLBACK(button_press_event), app);
    gtk_widget_set_events(drawing_area, gtk_widget_get_events(drawing_area) | GDK_BUTTON_PRESS_MASK);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    GtkWidget *circle_button = gtk_button_new_with_label("Circle");
    g_signal_connect(circle_button, "clicked", G_CALLBACK(on_circle_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), circle_button, TRUE, TRUE, 0);

    GtkWidget *rectangle_button = gtk_button_new_with_label("Rectangle");
    g_signal_connect(rectangle_button, "clicked", G_CALLBACK(on_rectangle_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), rectangle_button, TRUE, TRUE, 0);

    GtkWidget *polygon_button = gtk_button_new_with_label("Polygon");
    g_signal_connect(polygon_button, "clicked", G_CALLBACK(on_polygon_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), polygon_button, TRUE, TRUE, 0);

    GtkWidget *eraser_button = gtk_button_new_with_label("Eraser");
    g_signal_connect(eraser_button, "clicked", G_CALLBACK(on_eraser_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), eraser_button, TRUE, TRUE, 0);

    GtkWidget *next_page_button = gtk_button_new_with_label("Next Page");
    g_signal_connect(next_page_button, "clicked", G_CALLBACK(on_next_page_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), next_page_button, TRUE, TRUE, 0);

    GtkWidget *prev_page_button = gtk_button_new_with_label("Previous Page");
    g_signal_connect(prev_page_button, "clicked", G_CALLBACK(on_prev_page_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), prev_page_button, TRUE, TRUE, 0);

    GtkWidget *add_page_button = gtk_button_new_with_label("Add Page");
    g_signal_connect(add_page_button, "clicked", G_CALLBACK(on_add_page_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), add_page_button, TRUE, TRUE, 0);

    GtkWidget *color_picker = gtk_color_button_new();
    g_signal_connect(color_picker, "color-set", G_CALLBACK(on_color_set), app);
    gtk_box_pack_start(GTK_BOX(hbox), color_picker, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_widget_show_all(window);
    gtk_main();

    for (int i = 0; i < app->num_pages; i++) {
        free_object_list(app->pages[i]);
    }
    g_free(app->pages);
    g_free(app);

    return 0;
}
