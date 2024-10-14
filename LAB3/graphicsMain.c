#include <gtk/gtk.h>
#include <math.h>  // Include for sqrt()

// Define the Circle struct
typedef struct {
    gdouble x, y;
    gdouble radius;
} Circle;

typedef struct {
    GList *circles;       // List to store circle positions and radius
    Circle *active_circle; // The circle currently being resized or erased
    gboolean resizing;     // Track if we are resizing
    gboolean draw_circle;  // Flag to control circle drawing
    gboolean eraser_mode;  // Flag to control eraser mode
} AppData;

// Draw circles in the drawing area
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppData *app = (AppData *)user_data;

    // Set the background color (white)
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);  // Fill the background with white

    // Set the drawing color (black)
    cairo_set_source_rgb(cr, 0, 0, 0);

    // Iterate over the list of circles and draw each one with an outline
    for (GList *l = app->circles; l != NULL; l = l->next) {
        Circle *circle = (Circle *)l->data;
        cairo_arc(cr, circle->x, circle->y, circle->radius, 0, 2 * G_PI);
        cairo_stroke(cr); // Draw the outline of the circle
    }

    return FALSE;
}

// Handle button press for selecting/resizing or erasing a circle
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
                    app->resizing = TRUE;
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
        }
    }

    return TRUE;
}

// Handle mouse motion to resize the circle
static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    AppData *app = (AppData *)data;

    if (app->resizing && app->active_circle) {
        // Update the radius of the active circle based on the drag distance
        gdouble dx = event->x - app->active_circle->x;
        gdouble dy = event->y - app->active_circle->y;
        app->active_circle->radius = sqrt(dx * dx + dy * dy);

        gtk_widget_queue_draw(widget);  // Redraw with the updated radius
    }

    return TRUE;
}

// Handle button release to stop resizing
static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppData *app = (AppData *)data;

    if (event->button == GDK_BUTTON_PRIMARY && app->resizing) {
        // Stop resizing when the mouse button is released
        app->resizing = FALSE;
    }

    return TRUE;
}

// Free circle data on cleanup
static void free_circle_list(GList *circles) {
    for (GList *l = circles; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_list_free(circles);
}

// Handle the Circle button click
static void on_circle_button_clicked(GtkWidget *button, gpointer data) {
    AppData *app = (AppData *)data;
    app->draw_circle = TRUE; // Enable drawing circles
    app->eraser_mode = FALSE; // Disable eraser mode
    g_print("Circle button clicked! Click in the drawing area to create circles.\n");
}

// Handle the Eraser button click
static void on_eraser_button_clicked(GtkWidget *button, gpointer data) {
    AppData *app = (AppData *)data;
    app->eraser_mode = TRUE; // Enable eraser mode
    app->draw_circle = FALSE; // Disable circle drawing
    g_print("Eraser button clicked! Click on a circle to erase it.\n");
}

int main(int argc, char *argv[]) {
    GtkWidget *window, *darea, *vbox, *button_box, *button_circle, *button_eraser;
    AppData app_data = {NULL, NULL, FALSE, FALSE, FALSE};  // Initialize with no circles and no active resizing

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Circle Drawing App with Eraser");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create the Circle button
    button_circle = gtk_button_new_with_label("Circle");
    gtk_widget_set_size_request(button_circle, 70, 30);  // Set the size of the button

    // Create the Eraser button
    button_eraser = gtk_button_new_with_label("Eraser");
    gtk_widget_set_size_request(button_eraser, 70, 30);  // Set the size of the button

    // Create a vertical box container for the buttons
    button_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(button_box), FALSE); // Make buttons not stretch
    gtk_box_pack_start(GTK_BOX(button_box), button_circle, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(button_box), button_eraser, FALSE, FALSE, 5);

    // Create a vertical box container for the main layout
    vbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Add button box to the main layout
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 5);

    // Create a drawing area
    darea = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(vbox), darea, TRUE, TRUE, 5);

    // Connect button click signals
    g_signal_connect(G_OBJECT(button_circle), "clicked", G_CALLBACK(on_circle_button_clicked), &app_data);
    g_signal_connect(G_OBJECT(button_eraser), "clicked", G_CALLBACK(on_eraser_button_clicked), &app_data);

    // Connect signals for drawing and mouse events
    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), &app_data);
    g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(button_press_event), &app_data);
    g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(motion_notify_event), &app_data);
    g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(button_release_event), &app_data);

    // Enable mouse event handling for the drawing area
    gtk_widget_set_events(darea, gtk_widget_get_events(darea) | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK);

    gtk_widget_show_all(window);

    gtk_main();

    // Free any allocated circle data after GTK exits
    free_circle_list(app_data.circles);

    return 0;
}