#include <gtk/gtk.h>
#include <cairo-pdf.h>
#include <math.h>

typedef struct {
    gboolean is_circle;
    gboolean is_polygon;
    double x, y;
    double size;
    double *points;
    int num_points;
    double color[3];
} Shape;

typedef struct {
    int current_page;
    int num_pages;
    GList **pages;
    gboolean draw_circle;
    gboolean draw_rectangle;
    gboolean draw_polygon;
    gboolean eraser_mode;
    double current_color[3];
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
static void on_save_pdf_button_clicked(GtkWidget *button, gpointer user_data);
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

    if (app->draw_polygon) {
        GList *page = app->pages[app->current_page];
        Shape *shape = NULL;

        if (page && ((Shape *)g_list_last(page)->data)->is_polygon) {
            shape = (Shape *)g_list_last(page)->data;
        } else {
            shape = g_malloc(sizeof(Shape));
            shape->is_circle = FALSE;
            shape->is_polygon = TRUE;
            shape->points = g_malloc(2 * sizeof(double));
            shape->num_points = 0;
            shape->color[0] = app->current_color[0];
            shape->color[1] = app->current_color[1];
            shape->color[2] = app->current_color[2];
            app->pages[app->current_page] = g_list_append(app->pages[app->current_page], shape);
        }

        shape->points = g_realloc(shape->points, (shape->num_points + 1) * 2 * sizeof(double));
        shape->points[shape->num_points * 2] = event->x;
        shape->points[shape->num_points * 2 + 1] = event->y;
        shape->num_points++;

        if (event->type == GDK_2BUTTON_PRESS) {
            shape->is_polygon = FALSE;
        }

        gtk_widget_queue_draw(drawing_area);
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
    app->eraser_mode = TRUE;
    app->draw_circle = FALSE;
    app->draw_rectangle = FALSE;
    app->draw_polygon = FALSE;
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
    app->num_pages++;
    app->pages = g_realloc(app->pages, app->num_pages * sizeof(GList *));
    app->pages[app->num_pages - 1] = NULL;
    app->current_page = app->num_pages - 1;
    gtk_widget_queue_draw(drawing_area);
}

static void on_save_pdf_button_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    char filename[100];
    snprintf(filename, sizeof(filename), "page_%d.pdf", app->current_page + 1);

    cairo_surface_t *surface = cairo_pdf_surface_create(filename, 800, 600);
    cairo_t *cr = cairo_create(surface);

    on_draw_event(drawing_area, cr, user_data);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    g_print("Saved current page as %s\n", filename);
}

static void free_object_list(GList *list) {
    for (GList *item = list; item != NULL; item = item->next) {
        Shape *shape = (Shape *)item->data;
        if (shape->is_polygon && shape->points != NULL) {
            g_free(shape->points);
        }
        g_free(shape);
    }
    g_list_free(list);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    AppData app;
    app.current_page = 0;
    app.num_pages = 1;
    app.pages = g_malloc(sizeof(GList *));
    app.pages[0] = NULL;
    app.draw_circle = FALSE;
    app.draw_rectangle = FALSE;
    app.draw_polygon = FALSE;
    app.eraser_mode = FALSE;
    app.current_color[0] = 0;
    app.current_color[1] = 0;
    app.current_color[2] = 0;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Drawing Application");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 800, 600);
    gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_event), &app);
    g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(button_press_event), &app);
    gtk_widget_add_events(drawing_area, GDK_BUTTON_PRESS_MASK);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *circle_button = gtk_button_new_with_label("Draw Circle");
    g_signal_connect(circle_button, "clicked", G_CALLBACK(on_circle_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), circle_button, FALSE, FALSE, 0);

    GtkWidget *rectangle_button = gtk_button_new_with_label("Draw Rectangle");
    g_signal_connect(rectangle_button, "clicked", G_CALLBACK(on_rectangle_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), rectangle_button, FALSE, FALSE, 0);

    GtkWidget *polygon_button = gtk_button_new_with_label("Draw Polygon");
    g_signal_connect(polygon_button, "clicked", G_CALLBACK(on_polygon_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), polygon_button, FALSE, FALSE, 0);

    GtkWidget *eraser_button = gtk_button_new_with_label("Eraser");
    g_signal_connect(eraser_button, "clicked", G_CALLBACK(on_eraser_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), eraser_button, FALSE, FALSE, 0);

    GtkWidget *color_button = gtk_color_button_new();
    g_signal_connect(color_button, "color-set", G_CALLBACK(on_color_set), &app);
    gtk_box_pack_start(GTK_BOX(hbox), color_button, FALSE, FALSE, 0);

    GtkWidget *next_page_button = gtk_button_new_with_label("Next Page");
    g_signal_connect(next_page_button, "clicked", G_CALLBACK(on_next_page_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), next_page_button, FALSE, FALSE, 0);

    GtkWidget *prev_page_button = gtk_button_new_with_label("Previous Page");
    g_signal_connect(prev_page_button, "clicked", G_CALLBACK(on_prev_page_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), prev_page_button, FALSE, FALSE, 0);

    GtkWidget *add_page_button = gtk_button_new_with_label("Add Page");
    g_signal_connect(add_page_button, "clicked", G_CALLBACK(on_add_page_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), add_page_button, FALSE, FALSE, 0);

    GtkWidget *save_pdf_button = gtk_button_new_with_label("Save as PDF");
    g_signal_connect(save_pdf_button, "clicked", G_CALLBACK(on_save_pdf_button_clicked), &app);
    gtk_box_pack_start(GTK_BOX(hbox), save_pdf_button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    for (int i = 0; i < app.num_pages; i++) {
        free_object_list(app.pages[i]);
    }
    g_free(app.pages);

    return 0;
}
