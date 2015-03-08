#include <gtk/gtk.h>

#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfcegui4/xfce-gdk-extensions.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/libxfce4panel.h>

#include <libwnck/libwnck.h>

#include <glib.h>
#include <glib-object.h>

#include <syslog.h>

#ifdef DEBUG
	#define OPENLOG() openlog(g_get_application_name(), LOG_PID, LOG_USER)
	#define SYSLOG(...) syslog(LOG_INFO, __VA_ARGS__)
	#define CLOSELOG() closelog()
#else
	#define OPENLOG()
	#define SYSLOG(...)
	#define CLOSELOG()
#endif

#define SYSLOG_FUNCTION() SYSLOG("%s", __FUNCTION__)

typedef struct {
	XfcePanelPlugin *plugin;

	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *action_menu;

	WnckScreen *screen;
	WnckWindow *window;

	gint screen_id;
	gint wnck_screen_id;
	gint wnck_window_id;
} WindowHeaderData;

static void windowheader_construct(XfcePanelPlugin *plugin);

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(windowheader_construct);

static void windowheader_free(XfcePanelPlugin *plugin, WindowHeaderData *whd)
{
	SYSLOG_FUNCTION();

	if (whd->screen_id != 0)
		g_signal_handler_disconnect(plugin, whd->screen_id);

	if (whd->wnck_screen_id != 0)
		g_signal_handler_disconnect(whd->screen, whd->wnck_screen_id);

	if (whd->wnck_window_id != 0)
		g_signal_handler_disconnect(whd->window, whd->wnck_window_id);

	if (whd->action_menu != NULL)
		gtk_widget_destroy(whd->action_menu);

	whd->screen_id = whd->wnck_screen_id = whd->wnck_window_id = 0;

	gtk_widget_destroy(whd->button);
	panel_slice_free(WindowHeaderData, whd);

	SYSLOG("destroyed");
	CLOSELOG();
}

static gboolean windowheader_size_changed(XfcePanelPlugin *plugin,
					  gint size,
					  WindowHeaderData *whd)
{
	(void) whd;

	GtkOrientation orientation;

	SYSLOG_FUNCTION();
	SYSLOG("size changed to %d", size);

	orientation = xfce_panel_plugin_get_orientation(plugin);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		gtk_widget_set_size_request(GTK_WIDGET(plugin), -1, size);
	else
		gtk_widget_set_size_request(GTK_WIDGET(plugin), size, -1);

	return TRUE;
}

static void windowheader_update_label(WindowHeaderData *whd, const char *label)
{
	gtk_label_set_label(GTK_LABEL(whd->label), label);
	gtk_widget_set_tooltip_text(whd->button, label);
}

static void windowheader_window_name_changed(WnckWindow *window,
					     WindowHeaderData *whd)
{
	const gchar *name = NULL;

	SYSLOG_FUNCTION();

	name = wnck_window_get_name(window);
	SYSLOG("window name: %s", name);

	windowheader_update_label(whd, name);
}

static int windowheader_check_type(WnckWindow *window)
{
	WnckWindowType type = wnck_window_get_window_type(window);

	SYSLOG("window type: %u", type);

	switch (type) {
	case WNCK_WINDOW_NORMAL:
	case WNCK_WINDOW_DIALOG:
		return 0;

	default:
		break;
	}

	return 1;
}

static void windowheader_window_changed(WnckScreen *screen,
					WnckWindow *prev_window,
					WindowHeaderData *whd)
{
	WnckWindow *window;
	const gchar *name = NULL;

	(void) prev_window;

	SYSLOG_FUNCTION();

	if (whd->wnck_window_id != 0) {
		g_signal_handler_disconnect(whd->window, whd->wnck_window_id);
		whd->wnck_window_id = 0;
		whd->window = NULL;
	}

	window = wnck_screen_get_active_window(screen);

	SYSLOG("window: %p", window);

	if (window == NULL || windowheader_check_type(window))
		gtk_widget_hide(whd->button);
	else {
		whd->window = window;

		name = wnck_window_get_name(window);
		SYSLOG("window name: %s", name);

		whd->wnck_window_id = g_signal_connect(
			window, "name-changed",
			G_CALLBACK(windowheader_window_name_changed), whd
		);

		gtk_widget_show(whd->button);
	}

	windowheader_update_label(whd, name);
	SYSLOG("eof windowheader_update");
}

static void windowheader_screen_changed(XfcePanelPlugin *plugin,
					GdkScreen *screen,
					WindowHeaderData *whd)
{
	SYSLOG_FUNCTION();

	if (whd->wnck_screen_id != 0) {
		g_signal_handler_disconnect(whd->screen, whd->wnck_screen_id);
		whd->wnck_screen_id = 0;
	}

	if (whd->wnck_window_id != 0) {
		g_signal_handler_disconnect(whd->window, whd->wnck_window_id);
		whd->wnck_window_id = 0;
		whd->window = NULL;
	}

	screen = gtk_widget_get_screen(GTK_WIDGET(plugin));
	if (screen != NULL) {
		whd->screen = wnck_screen_get(gdk_screen_get_number(screen));

		whd->wnck_screen_id = g_signal_connect(
			whd->screen, "active-window-changed",
			G_CALLBACK(windowheader_window_changed), whd
		);

		windowheader_window_changed(whd->screen, NULL, whd);
	}
}

/*
static void windowheader_clicked(GtkButton *button, WindowHeaderData *whd)
{
	(void) button;

	if (whd->window != NULL)
		netk_window_minimize(whd->window);
}
*/

static void windowheader_menu_position(GtkMenu *menu, gint *x, gint *y,
				       gboolean *push_in, gpointer user_data)
{
	GdkWindow *root_window;
	gint tmp;

	root_window = gtk_widget_get_root_window(GTK_WIDGET(menu));
	gdk_window_get_pointer(root_window, x, NULL, NULL);

	*x -= 40;

	xfce_panel_plugin_position_menu(menu, &tmp, y, push_in, user_data);
}

static void windowheader_pressed(GtkButton *button, WindowHeaderData *whd)
{
	(void) button;

	if (whd->action_menu != NULL)
		gtk_widget_destroy(whd->action_menu);

	if (whd->window != NULL) {
		whd->action_menu = wnck_action_menu_new(whd->window);

		g_object_add_weak_pointer(G_OBJECT(whd->action_menu),
					  (gpointer *) &whd->action_menu);

		gtk_menu_set_screen(
			GTK_MENU(whd->action_menu),
			xfce_gdk_display_locate_monitor_with_pointer(NULL, NULL)
		);

		gtk_widget_show(whd->action_menu);
		gtk_menu_popup(
			GTK_MENU(whd->action_menu), NULL, NULL,
			windowheader_menu_position, whd->plugin, 1,
			gtk_get_current_event_time()
		);
	}
}

static void windowheader_dialog_close(GtkDialog *dlg, gint response G_GNUC_UNUSED,
				      WindowHeaderData *whd)
{
	gtk_widget_destroy(GTK_WIDGET(dlg));
	xfce_panel_plugin_unblock_menu(whd->plugin);
}

static void windowheader_configure(XfcePanelPlugin *plugin, WindowHeaderData *whd)
{
	GtkWidget *dlg;
	GtkWidget *label;

	xfce_panel_plugin_block_menu(plugin);

	dlg = xfce_titled_dialog_new_with_buttons("Window Header",
		NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);

	g_signal_connect(dlg, "response",
			 G_CALLBACK(windowheader_dialog_close), whd);

	label = gtk_label_new("configure will be soon ...");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(dlg);
}

static void windowheader_construct(XfcePanelPlugin *plugin)
{
	WindowHeaderData *whd;

	OPENLOG();
	SYSLOG("started");

	whd = panel_slice_new0(WindowHeaderData);
	whd->plugin = plugin;

	whd->button = xfce_create_panel_button();
	gtk_widget_show(whd->button);
	gtk_container_add(GTK_CONTAINER(plugin), whd->button);

	whd->label = gtk_label_new(NULL);
	gtk_label_set_ellipsize(GTK_LABEL(whd->label), PANGO_ELLIPSIZE_END);
	gtk_widget_show(whd->label);

	gtk_container_add(GTK_CONTAINER(whd->button), whd->label);

	xfce_panel_plugin_add_action_widget(plugin, whd->button);
	xfce_panel_plugin_set_expand(plugin, TRUE);

	g_signal_connect(plugin, "free-data",
			 G_CALLBACK(windowheader_free), whd);
	g_signal_connect(plugin, "size-changed",
			 G_CALLBACK(windowheader_size_changed), NULL);
	g_signal_connect(plugin, "configure-plugin",
			 G_CALLBACK(windowheader_configure), whd);
	xfce_panel_plugin_menu_show_configure(plugin);

	g_signal_connect(whd->button, "pressed",
			 G_CALLBACK(windowheader_pressed), whd);

/*
	g_signal_connect(whd->button, "clicked",
			 G_CALLBACK(windowheader_clicked), whd);
*/

	windowheader_screen_changed(plugin, gtk_widget_get_screen(whd->button),
				    whd);

	whd->screen_id = g_signal_connect(
		plugin, "screen-changed",
		G_CALLBACK(windowheader_screen_changed), whd
	);

	SYSLOG("initialized");
}

