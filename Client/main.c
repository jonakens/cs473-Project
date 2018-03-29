#include <gtk/gtk.h>

// Function to open a dialog box with a message
void
quick_message (GtkWindow *parent, gchar *message)
{
 GtkWidget *dialog, *label, *content_area;
 GtkDialogFlags flags;

 // Create the widgets
 flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 dialog = gtk_dialog_new_with_buttons ("Message", parent, flags, ("_OK"), GTK_RESPONSE_NONE, NULL);
 content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
 label = gtk_label_new (message);

 // Ensure that the dialog box is destroyed when the user responds

 g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

 // Add the label, and show everything we’ve added

 gtk_container_add (GTK_CONTAINER (content_area), label);
 gtk_widget_show_all (dialog);
}

static void
activate (GtkApplication* app, gpointer user_data)
{
  GtkWidget *window;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Window");
  gtk_window_set_default_size (GTK_WINDOW (window), 1920, 1080);
  quick_message(window, "Hello...");
  gtk_widget_show_all (window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}