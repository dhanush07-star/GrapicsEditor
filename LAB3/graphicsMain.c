#include <gtk/gtk.h>
#include <math.h>  // Include for sqrt()

// Define the Circle struct
typedef struct {
    gdouble x, y;
    gdouble radius;
} Circle;

// Define the Rectangle struct
typedef struct {
    gdouble x, y;     // Top-left corner
    gdouble width, height; // Width and height of the rectangle
} Rectangle;

typedef struct {
    GList *circles;       // List to store circles
    Circle *active_circle; // The circle currently being resized or erased
    GList *rectangles;    // List to store rectangles
    Rectangle *active_rectangle; // The rectangle currently being resized
    gboolean resizing;     // Track if we are resizing
    gboolean draw_circle;  // Flag to control circle drawing
    gboolean draw_rectangle; // Flag to control rectangle drawing
    gboolean eraser_mode;  // Flag to control eraser mode
    gboolean is_dragging;  // Track if we are dragging for resizing
} AppData;

// Draw circles and rectangles in the drawing area
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppData *app = (AppData *)user_data;

    // Set the background color (white)
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);  // Fill the background with white

    // Set the drawing color (black)
    cairo_set_source_rgb(cr, 0, 0, 0);

    // Draw circles
    for (GList *l = app->circles; l != NULL; l = l->next) {
        Circle *circle = (Circle *)l->data;
        cairo_arc(cr, circle->x, circle->y, circle->radius, 0, 2 * G_PI);
        cairo_stroke(cr); // Draw the outline of the circle
    }

    // Draw rectangles
    for (GList *l = app->rectangles; l != NULL; l = l->next) {
        Rectangle *rectangle = (Rectangle *)l->data;
        cairo_rectangle(cr, rectangle->x, rectangle->y, rectangle->width, rectangle->height);
        cairo_stroke(cr); // Draw the outline of the rectangle
    }

    return FALSE;
}

// Handle button press for selecting/resizing or erasing a circle or rectangle
static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppData *app = (AppData *)data;

    if (event->button == GDK_BUTTON_PRIMARY) {
        // Check if we are in eraser mode
        if (app->eraser_mode) {
            // Find and remove the clicked circle
            for (GList *l = app->circles; l != NULL; l = l->next) {
                Circle *circle = (Circle *)l->data;

                // Check if the click is within the circle
                gdouble dx = event->x - circle->x;
                gdouble dy = event->y - circle->y;
                gdouble distance = sqrt(dx * dx + dy * dy);

                if (distance <= circle->radius) {
                    // Remove the circle from the list
                    app->circles = g_list_remove(app->circles, circle);
                    g_free(circle); // Free the memory for the removed circle
                    gtk_widget_queue_draw(widget); // Redraw to reflect changes
                    return TRUE;
                }
            }

            // Find and remove the clicked rectangle
            for (GList *l = app->rectangles; l != NULL; l = l->next) {
                Rectangle *rectangle = (Rectangle *)l->data;

                // Check if the click is within the rectangle
                if (event->x >= rectangle->x && event->x <= rectangle->x + rectangle->width &&
                    event->y >= rectangle->y && event->y <= rectangle->y + rectangle->height) {
                    // Remove the rectangle from the list
                    app->rectangles = g_list_remove(app->rectangles, rectangle);
                    g_free(rectangle); // Free the memory for the removed rectangle
                    gtk_widget_queue_draw(widget); // Redraw to reflect changes
                    return TRUE;
                }
            }
        } else if (app->draw_circle) {
            // Find the clicked circle, if any, for resizing
            for (GList *l = app->circles; l != NULL; l = l->next) {
                Circle *circle = (Circle *)l->data;

                // Check if the click is within the circle
                gdouble dx = event->x - circle->x;
                gdouble dy = event->y - circle->y;
                gdouble distance = sqrt(dx * dx + dy * dy);

                if (distance <= circle->radius) {
                    // Circle clicked, prepare for resizing
                    app->active_circle = circle;
                    app->is_dragging = TRUE;
                    return TRUE;
                }
            }

            // If no circle clicked, create a new one
            Circle *new_circle = g_malloc(sizeof(Circle));
            new_circle->x = event->x;
            new_circle->y = event->y;
            new_circle->radius = 20; // Default radius

            // Add the new circle to the list
            app->circles = g_list_append(app->circles, new_circle);
            gtk_widget_queue_draw(widget);
        } else if (app->draw_rectangle) {
            // Find the clicked rectangle, if any, for resizing
            for (GList *l = app->rectangles; l != NULL; l = l->next) {
                Rectangle *rectangle = (Rectangle *)l->data;

                // Check if the click is within the rectangle
                if (event->x >= rectangle->x && event->x <= rectangle->x + rectangle->width &&
                    event->y >= rectangle->y && event->y <= rectangle->y + rectangle->height) {
                    // Rectangle clicked, prepare for resizing
                    app->active_rectangle = rectangle;
                    app->is_dragging = TRUE;
                    return TRUE;
                }
            }

            // If no rectangle clicked, create a new one
            Rectangle *new_rectangle = g_malloc(sizeof(Rectangle));
            new_rectangle->x = event->x;
            new_rectangle->y = event->y;
            new_rectangle->width = 50; // Default width
            new_rectangle->height = 30; // Default height

            // Add the new rectangle to the list
            app->rectangles = g_list_append(app->rectangles, new_rectangle);
            gtk_widget_queue_draw(widget);
        }
    }

    return TRUE;
}

// Handle mouse motion to resize the circle or rectangle
static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    AppData *app = (AppData *)data;

    if (app->is_dragging) {
        if (app->active_circle) {
            // Update the radius of the active circle based on the drag distance
            gdouble dx = event->x - app->active_circle->x;
            gdouble dy = event->y - app->active_circle->y;
            app->active_circle->radius = sqrt(dx * dx + dy * dy);
        } else if (app->active_rectangle) {
            // Update the size of the active rectangle based on the drag distance
            app->active_rectangle->width = event->x - app->active_rectangle->x;
            app->active_rectangle->height = event->y - app->active_rectangle->y;

            // Ensure the dimensions are not negative
            if (app->active_rectangle->width < 0) {
                app->active_rectangle->width = 0;
                app->active_rectangle->x = event->x; // Move the top-left corner to the current mouse position
            }
            if (app->active_rectangle->height < 0) {
                app->active_rectangle->height = 0;
                app->active_rectangle->y = event->y; // Move the top-left corner to the current mouse position
            }
        }

        gtk_widget_queue_draw(widget);  // Redraw with the updated size
    }

    return TRUE;
}

// Handle button release to stop resizing
static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppData *app = (AppData *)data;

    if (event->button == GDK_BUTTON_PRIMARY && app->is_dragging) {
        // Stop resizing when the mouse button is released
        app->is_dragging = FALSE;
    }

    return TRUE;
}

// Free circle and rectangle data on cleanup
static void free_circle_list(GList *circles) {
    for (GList *l = circles; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_list_free(circles);
}

static void free_rectangle_list(GList *rectangles) {
    for (GList *l = rectangles; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_list_free(rectangles);
}

// Handle the Circle button click
static void on_circle_button_clicked(GtkWidget *button, gpointer data) {
    AppData *app = (AppData *)data;
    app->draw_circle = TRUE; // Enable drawing circles
    app->draw_rectangle = FALSE; // Disable rectangle drawing
    app->eraser_mode = FALSE; // Disable eraser mode
    g_print("Circle button clicked! Click to draw circles.\n");
}

// Handle the Rectangle button click
static void on_rectangle_button_clicked(GtkWidget *button, gpointer data) {
    AppData *app = (AppData *)data;
    app->draw_rectangle = TRUE; // Enable drawing rectangles
    app->draw_circle = FALSE; // Disable circle drawing
    app->eraser_mode = FALSE; // Disable eraser mode
    g_print("Rectangle button clicked! Click to draw rectangles.\n");
}

// Handle the Eraser button click
static void on_eraser_button_clicked(GtkWidget *button, gpointer data) {
    AppData *app = (AppData *)data;
    app->eraser_mode = TRUE; // Enable eraser mode
    app->draw_circle = FALSE; // Disable circle drawing
    app->draw_rectangle = FALSE; // Disable rectangle drawing
    g_print("Eraser button clicked! Click on circles or rectangles to erase them.\n");
}

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Drawing Application");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    AppData *app = g_malloc(sizeof(AppData));
    app->circles = NULL;
    app->rectangles = NULL;
    app->active_circle = NULL;
    app->active_rectangle = NULL;
    app->resizing = FALSE;
    app->draw_circle = FALSE;
    app->draw_rectangle = FALSE;
    app->eraser_mode = FALSE;
    app->is_dragging = FALSE;

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 800, 600);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_event), app);
    g_signal_connect(drawing_area, "button-press-event", G_CALLBACK(button_press_event), app);
    g_signal_connect(drawing_area, "motion-notify-event", G_CALLBACK(motion_notify_event), app);
    g_signal_connect(drawing_area, "button-release-event", G_CALLBACK(button_release_event), app);
    gtk_widget_set_events(drawing_area, gtk_widget_get_events(drawing_area) | 
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_RELEASE_MASK);

    // Create a vertical box for the buttons and the drawing area
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);

    // Create a vertical box for the buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    
    // Create buttons for Circle, Rectangle, and Eraser
    GtkWidget *circle_button = gtk_button_new_with_label("Circle");
    GtkWidget *rectangle_button = gtk_button_new_with_label("Rectangle");
    GtkWidget *eraser_button = gtk_button_new_with_label("Eraser");

    // Connect buttons to their respective handlers
    g_signal_connect(circle_button, "clicked", G_CALLBACK(on_circle_button_clicked), app);
    g_signal_connect(rectangle_button, "clicked", G_CALLBACK(on_rectangle_button_clicked), app);
    g_signal_connect(eraser_button, "clicked", G_CALLBACK(on_eraser_button_clicked), app);

    // Pack buttons into the button box
    gtk_box_pack_start(GTK_BOX(button_box), circle_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), rectangle_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), eraser_button, FALSE, FALSE, 0);

    // Pack the button box into the vertical box
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_widget_show_all(window);
    gtk_main();

    // Free memory
    free_circle_list(app->circles);
    free_rectangle_list(app->rectangles);
    g_free(app);

    return 0;
}
