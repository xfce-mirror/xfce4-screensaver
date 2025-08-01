/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2018      Sean Davis <bluesabre@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef ENABLE_X11
#include <X11/extensions/shape.h>
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include <libwlembed-gtk3/libwlembed-gtk3.h>
#endif

#include <gdk/gdkkeysyms.h>

#include "gs-debug.h"
#include "gs-manager.h"
#include "gs-marshal.h"
#include "gs-prefs.h"
#include "gs-window.h"
#include "subprocs.h"
#include "xfce-desktop-utils.h"

static void
gs_window_finalize (GObject *object);

static gboolean
popup_dialog_idle (gpointer user_data);
static void
gs_window_dialog_finish (GSWindow *window);
static void
gs_window_set_obscured (GSWindow *window,
                        gboolean obscured);
static void
remove_command_watches (GSWindow *window);

enum {
    DIALOG_RESPONSE_CANCEL,
    DIALOG_RESPONSE_OK
};

#define MAX_QUEUED_EVENTS 16
#define INFO_BAR_SECONDS 30

struct GSWindowPrivate {
    GSManager *manager;
    GdkMonitor *monitor;

    GdkRectangle geometry;
    guint obscured : 1;
    guint dialog_up : 1;

    gboolean lock_active;
    char *status_message;
    GSPrefs *prefs;

    GtkWidget *vbox;
    GtkWidget *overlay;
    GtkWidget *drawing_area;
    GtkWidget *lock_box;
    GtkWidget *lock_socket;
    GtkWidget *keyboard_socket;
    GtkWidget *info_bar;
    GtkWidget *info_content;

    guint popup_dialog_idle_id;
    guint deactivated_idle_id;

    guint dialog_map_signal_id;
    guint dialog_unmap_signal_id;
    guint dialog_response_signal_id;

    guint watchdog_timer_id;
    guint info_bar_timer_id;

    gint lock_pid;
    gint lock_watch_id;
    gint dialog_response;

    gint keyboard_pid;
    gint keyboard_watch_id;

    GList *key_events;

    gdouble last_x;
    gdouble last_y;

    GTimer *timer;

    int shape_event_base;
};

enum {
    ACTIVITY,
    DEACTIVATED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_OBSCURED,
    PROP_DIALOG_UP,
    PROP_MONITOR,
};

static guint signals[LAST_SIGNAL] = {
    0,
};

G_DEFINE_TYPE_WITH_PRIVATE (GSWindow, gs_window, GTK_TYPE_WINDOW)

static void
set_invisible_cursor (GdkWindow *window,
                      gboolean invisible) {
    GdkCursor *cursor = NULL;

    if (invisible) {
        GdkDisplay *display;
        display = gdk_window_get_display (window);
        cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);
    }

    gdk_window_set_cursor (window, cursor);

    if (cursor) {
        g_object_unref (cursor);
    }
}

#ifdef ENABLE_X11
/* derived from tomboy */
static void
gs_window_override_user_time (GSWindow *window) {
    guint32 ev_time = gtk_get_current_event_time ();

    if (ev_time == 0) {
        gint ev_mask = gtk_widget_get_events (GTK_WIDGET (window));
        if (!(ev_mask & GDK_PROPERTY_CHANGE_MASK)) {
            gtk_widget_add_events (GTK_WIDGET (window),
                                   GDK_PROPERTY_CHANGE_MASK);
        }

        /*
         * NOTE: Last resort for D-BUS or other non-interactive
         *       openings.  Causes roundtrip to server.  Lame.
         */
        ev_time = gdk_x11_get_server_time (gtk_widget_get_window (GTK_WIDGET (window)));
    }

    gdk_x11_window_set_user_time (gtk_widget_get_window (GTK_WIDGET (window)), ev_time);
}

static void
clear_children (Window window) {
    Window root;
    Window parent;
    Window *children;
    unsigned int n_children;
    int status;

    children = NULL;
    status = XQueryTree (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                         window, &root, &parent, &children, &n_children);

    if (status == 0) {
        if (children) {
            XFree (children);
        }
        return;
    }

    if (children) {
        while (n_children) {
            Window child;

            child = children[--n_children];

            XClearWindow (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), child);

            clear_children (child);
        }

        XFree (children);
    }
}

static void
widget_clear_all_children (GtkWidget *widget) {
    GdkWindow *w;
    GdkDisplay *display;

    gs_debug ("Clearing all child windows");
    display = gtk_widget_get_display (widget);

    gdk_x11_display_error_trap_push (display);

    w = gtk_widget_get_window (widget);

    clear_children (GDK_WINDOW_XID (w));

    gdk_x11_display_error_trap_pop_ignored (display);
}
#endif

void
gs_window_clear (GSWindow *window) {
#ifdef ENABLE_X11
    GdkDisplay *display;
    g_return_if_fail (GS_IS_WINDOW (window));

    if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        return;
    }

    gs_debug ("Clearing widgets");

    if (gtk_widget_get_realized (GTK_WIDGET (window))) {
        gtk_widget_queue_draw (GTK_WIDGET (window));
        /* If a screensaver theme adds child windows
           we need to clear them too */
        widget_clear_all_children (GTK_WIDGET (window));
    }

    if (gtk_widget_get_realized (window->priv->drawing_area)) {
        gtk_widget_queue_draw (window->priv->drawing_area);
        widget_clear_all_children (window->priv->drawing_area);
    }

    display = gtk_widget_get_display (GTK_WIDGET (window));
    gdk_display_flush (display);
#endif
}

static cairo_region_t *
get_outside_region (GSWindow *window) {
    GdkDisplay *display;
    int i;
    int num_monitors;
    cairo_region_t *region;

    display = gtk_widget_get_display (GTK_WIDGET (window));

    region = cairo_region_create ();

    num_monitors = gdk_display_get_n_monitors (display);
    for (i = 0; i < num_monitors; i++) {
        GdkMonitor *mon = gdk_display_get_monitor (display, i);

        if (mon != window->priv->monitor) {
            GdkRectangle geometry;
            cairo_rectangle_int_t rectangle;

            gdk_monitor_get_geometry (mon, &geometry);
            rectangle.x = geometry.x;
            rectangle.y = geometry.y;
            rectangle.width = geometry.width;
            rectangle.height = geometry.height;
            cairo_region_union_rectangle (region, &rectangle);
        } else {
            break;
        }
    }

    return region;
}

static void
update_geometry (GSWindow *window) {
    GdkRectangle geometry;
    cairo_region_t *outside_region;
    cairo_region_t *monitor_region;

    outside_region = get_outside_region (window);

    gdk_monitor_get_geometry (window->priv->monitor, &geometry);
    gs_debug ("Got geometry for monitor: x=%d y=%d w=%d h=%d",
              geometry.x,
              geometry.y,
              geometry.width,
              geometry.height);
    monitor_region = cairo_region_create_rectangle ((const cairo_rectangle_int_t *) &geometry);
    cairo_region_subtract (monitor_region, outside_region);
    cairo_region_destroy (outside_region);

    cairo_region_get_extents (monitor_region, (cairo_rectangle_int_t *) &geometry);
    cairo_region_destroy (monitor_region);

    gs_debug ("Using geometry for monitor: x=%d y=%d w=%d h=%d",
              geometry.x,
              geometry.y,
              geometry.width,
              geometry.height);

    window->priv->geometry.x = geometry.x;
    window->priv->geometry.y = geometry.y;
    window->priv->geometry.width = geometry.width;
    window->priv->geometry.height = geometry.height;
}

static void
monitor_geometry_notify (GdkMonitor *monitor,
                         GParamSpec *pspec,
                         GSWindow *window) {
    gs_debug ("Got monitor geometry notify signal");
    gtk_widget_queue_resize (GTK_WIDGET (window));
}

/* copied from panel-toplevel.c */
static void
gs_window_move_resize_window (GSWindow *window,
                              gboolean move,
                              gboolean resize) {
    GtkWidget *widget;
    GdkWindow *gdkwindow;

    widget = GTK_WIDGET (window);
    gdkwindow = gtk_widget_get_window (widget);

    g_assert (gtk_widget_get_realized (widget));

    gs_debug ("Move and/or resize window: x=%d y=%d w=%d h=%d",
              window->priv->geometry.x,
              window->priv->geometry.y,
              window->priv->geometry.width,
              window->priv->geometry.height);

    if (move && resize) {
        gdk_window_move_resize (gdkwindow,
                                window->priv->geometry.x,
                                window->priv->geometry.y,
                                window->priv->geometry.width,
                                window->priv->geometry.height);
    } else if (move) {
        gdk_window_move (gdkwindow,
                         window->priv->geometry.x,
                         window->priv->geometry.y);
    } else if (resize) {
        gdk_window_resize (gdkwindow,
                           window->priv->geometry.width,
                           window->priv->geometry.height);
    }
}

static void
gs_window_real_unrealize (GtkWidget *widget) {
    GdkMonitor *monitor = GS_WINDOW (widget)->priv->monitor;

    if (monitor != NULL)
        g_signal_handlers_disconnect_by_func (monitor, monitor_geometry_notify, widget);

    if (GTK_WIDGET_CLASS (gs_window_parent_class)->unrealize) {
        GTK_WIDGET_CLASS (gs_window_parent_class)->unrealize (widget);
    }
}

static void
gs_window_real_realize (GtkWidget *widget) {
    GdkMonitor *monitor = GS_WINDOW (widget)->priv->monitor;

    if (GTK_WIDGET_CLASS (gs_window_parent_class)->realize) {
        GTK_WIDGET_CLASS (gs_window_parent_class)->realize (widget);
    }

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        gs_window_override_user_time (GS_WINDOW (widget));
    }
#endif

    gs_window_move_resize_window (GS_WINDOW (widget), TRUE, TRUE);

    g_signal_connect (monitor,
                      "notify::geometry",
                      G_CALLBACK (monitor_geometry_notify),
                      widget);
}

/* every so often we should raise the window in case
   another window has somehow gotten on top */
static gboolean
watchdog_timer (gpointer user_data) {
    GSWindow *window = user_data;
    GtkWidget *widget = GTK_WIDGET (window);

    gdk_window_focus (gtk_widget_get_window (widget), GDK_CURRENT_TIME);

    return TRUE;
}

static void
remove_watchdog_timer (GSWindow *window) {
    if (window->priv->watchdog_timer_id != 0) {
        g_source_remove (window->priv->watchdog_timer_id);
        window->priv->watchdog_timer_id = 0;
    }
}

static void
add_watchdog_timer (GSWindow *window,
                    glong seconds) {
    window->priv->watchdog_timer_id = g_timeout_add_seconds (seconds, watchdog_timer, window);
}

static void
remove_popup_dialog_idle (GSWindow *window) {
    if (window->priv->popup_dialog_idle_id != 0) {
        g_source_remove (window->priv->popup_dialog_idle_id);
        window->priv->popup_dialog_idle_id = 0;
    }
}

static void
add_popup_dialog_idle (GSWindow *window) {
    window->priv->popup_dialog_idle_id = g_idle_add (popup_dialog_idle, window);
}

static gboolean
emit_deactivated_idle (gpointer user_data) {
    GSWindow *window = user_data;

    g_signal_emit (window, signals[DEACTIVATED], 0);
    window->priv->deactivated_idle_id = 0;

    return FALSE;
}

static void
add_emit_deactivated_idle (GSWindow *window) {
    if (window->priv->deactivated_idle_id > 0) {
        g_source_remove (window->priv->deactivated_idle_id);
    }
    window->priv->deactivated_idle_id = g_idle_add (emit_deactivated_idle, window);
}

#ifdef ENABLE_X11
static void
gs_window_raise (GSWindow *window) {
    GdkWindow *win;

    g_return_if_fail (GS_IS_WINDOW (window));

    gs_debug ("Raising screensaver window");

    win = gtk_widget_get_window (GTK_WIDGET (window));

    gdk_window_raise (win);
}

static gboolean
x11_window_is_ours (Window window) {
    GdkWindow *gwindow;
    gboolean ret;

    ret = FALSE;

    gwindow = gdk_x11_window_lookup_for_display (gdk_display_get_default (), window);
    if (gwindow && (window != GDK_ROOT_WINDOW ())) {
        ret = TRUE;
    }

    return ret;
}

static void
unshape_window (GSWindow *window) {
    gdk_window_shape_combine_region (gtk_widget_get_window (GTK_WIDGET (window)),
                                     NULL,
                                     0,
                                     0);
}

static void
gs_window_xevent (GSWindow *window,
                  GdkXEvent *xevent) {
    XEvent *ev;

    ev = xevent;

    /* MapNotify is used to tell us when new windows are mapped.
       ConfigureNofify is used to tell us when windows are raised. */
    switch (ev->xany.type) {
        case MapNotify: {
            XMapEvent *xme = &ev->xmap;

            if (!x11_window_is_ours (xme->window)) {
                gs_window_raise (window);
            } else {
                gs_debug ("Not raising our windows");
            }

            break;
        }
        case ConfigureNotify: {
            XConfigureEvent *xce = &ev->xconfigure;

            if (!x11_window_is_ours (xce->window)) {
                gs_window_raise (window);
            } else {
                gs_debug ("Not raising our windows");
            }

            break;
        }
        default:
            /* extension events */
            if (ev->xany.type == (window->priv->shape_event_base + ShapeNotify)) {
                /*XShapeEvent *xse = (XShapeEvent *) ev;*/
                unshape_window (window);
                gs_debug ("Window was reshaped!");
            }

            break;
    }
}

static GdkFilterReturn
xevent_filter (GdkXEvent *xevent,
               GdkEvent *event,
               GSWindow *window) {
    gs_window_xevent (window, xevent);

    return GDK_FILTER_CONTINUE;
}

static void
select_popup_events (void) {
    XWindowAttributes attr;
    unsigned long events;
    GdkDisplay *display;

    display = gdk_display_get_default ();

    gdk_x11_display_error_trap_push (display);

    memset (&attr, 0, sizeof (attr));
    XGetWindowAttributes (GDK_DISPLAY_XDISPLAY (display), GDK_ROOT_WINDOW (), &attr);

    events = SubstructureNotifyMask | attr.your_event_mask;
    XSelectInput (GDK_DISPLAY_XDISPLAY (display), GDK_ROOT_WINDOW (), events);

    gdk_x11_display_error_trap_pop_ignored (display);
}

static void
window_select_shape_events (GSWindow *window) {
    int shape_error_base;
    GdkDisplay *display;

    display = gtk_widget_get_display (GTK_WIDGET (window));

    gdk_x11_display_error_trap_push (display);

    if (XShapeQueryExtension (GDK_DISPLAY_XDISPLAY (display), &window->priv->shape_event_base, &shape_error_base)) {
        unsigned long events = ShapeNotifyMask;
        XShapeSelectInput (GDK_DISPLAY_XDISPLAY (display),
                           GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (window))),
                           events);
    }

    gdk_x11_display_error_trap_pop_ignored (display);
}
#endif

static gboolean
gs_window_real_draw (GtkWidget *widget,
                     cairo_t *cr) {
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_paint (cr);
    return FALSE;
}

static void
gs_window_real_show (GtkWidget *widget) {
    GSWindow *window;

    if (GTK_WIDGET_CLASS (gs_window_parent_class)->show) {
        GTK_WIDGET_CLASS (gs_window_parent_class)->show (widget);
    }

    gs_window_clear (GS_WINDOW (widget));

    set_invisible_cursor (gtk_widget_get_window (widget), TRUE);

    window = GS_WINDOW (widget);
    if (window->priv->timer) {
        g_timer_destroy (window->priv->timer);
    }
    window->priv->timer = g_timer_new ();

    remove_watchdog_timer (window);
    add_watchdog_timer (window, 30);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        select_popup_events ();
        window_select_shape_events (window);
        gdk_window_add_filter (NULL, (GdkFilterFunc) xevent_filter, window);
    }
#endif
}

static void
set_info_text_and_icon (GSWindow *window,
                        const char *icon_name,
                        const char *primary_text,
                        const char *secondary_text) {
    GtkWidget *content_area;
    GtkWidget *hbox_content;
    GtkWidget *image;
    GtkWidget *vbox;
    gchar *primary_markup;
    GtkWidget *primary_label;

    hbox_content = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_show (hbox_content);

    image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DIALOG);
    gtk_widget_show (image);
    gtk_box_pack_start (GTK_BOX (hbox_content), image, FALSE, FALSE, 0);
    gtk_widget_set_valign (image, GTK_ALIGN_START);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_show (vbox);
    gtk_box_pack_start (GTK_BOX (hbox_content), vbox, FALSE, FALSE, 0);

    primary_markup = g_strdup_printf ("<b>%s</b>", primary_text);
    primary_label = gtk_label_new (primary_markup);
    g_free (primary_markup);
    gtk_widget_show (primary_label);
    gtk_box_pack_start (GTK_BOX (vbox), primary_label, TRUE, TRUE, 0);
    gtk_label_set_use_markup (GTK_LABEL (primary_label), TRUE);
    gtk_label_set_line_wrap (GTK_LABEL (primary_label), TRUE);
    gtk_widget_set_halign (primary_label, GTK_ALIGN_START);

    if (secondary_text != NULL) {
        gchar *secondary_markup;
        GtkWidget *secondary_label;
        secondary_markup = g_strdup_printf ("<small>%s</small>",
                                            secondary_text);
        secondary_label = gtk_label_new (secondary_markup);
        g_free (secondary_markup);
        gtk_widget_show (secondary_label);
        gtk_box_pack_start (GTK_BOX (vbox), secondary_label, TRUE, TRUE, 0);
        gtk_label_set_use_markup (GTK_LABEL (secondary_label), TRUE);
        gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
        gtk_widget_set_halign (secondary_label, GTK_ALIGN_START);
    }

    /* remove old content */
    content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (window->priv->info_bar));
    if (window->priv->info_content != NULL) {
        gtk_container_remove (GTK_CONTAINER (content_area), window->priv->info_content);
    }
    gtk_box_pack_start (GTK_BOX (content_area),
                        hbox_content,
                        TRUE, FALSE, 0);
    window->priv->info_content = hbox_content;
}

static gboolean
info_bar_timeout (gpointer user_data) {
    GSWindow *window = user_data;

    window->priv->info_bar_timer_id = 0;
    gtk_widget_hide (window->priv->info_bar);
    return FALSE;
}

void
gs_window_show_message (GSWindow *window,
                        const char *summary,
                        const char *body,
                        const char *icon) {
    g_return_if_fail (GS_IS_WINDOW (window));

    set_info_text_and_icon (window,
                            icon,
                            summary,
                            body);
    gtk_widget_show (window->priv->info_bar);

    if (window->priv->info_bar_timer_id > 0) {
        g_source_remove (window->priv->info_bar_timer_id);
    }

    window->priv->info_bar_timer_id = g_timeout_add_seconds (INFO_BAR_SECONDS, info_bar_timeout, window);
}

void
gs_window_show (GSWindow *window) {
    g_return_if_fail (GS_IS_WINDOW (window));

    gtk_widget_show (GTK_WIDGET (window));
}

static void
gs_window_real_hide (GtkWidget *widget) {
    GSWindow *window;

    window = GS_WINDOW (widget);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        gdk_window_remove_filter (NULL, (GdkFilterFunc) xevent_filter, window);
    }
#endif

    remove_watchdog_timer (window);

    if (GTK_WIDGET_CLASS (gs_window_parent_class)->hide) {
        GTK_WIDGET_CLASS (gs_window_parent_class)->hide (widget);
    }
}

void
gs_window_destroy (GSWindow *window) {
    g_return_if_fail (GS_IS_WINDOW (window));

    gs_window_cancel_unlock_request (window);

    gtk_widget_destroy (GTK_WIDGET (window));
}

GdkWindow *
gs_window_get_gdk_window (GSWindow *window) {
    g_return_val_if_fail (GS_IS_WINDOW (window), NULL);

    return gtk_widget_get_window (GTK_WIDGET (window));
}

GtkWidget *
gs_window_get_drawing_area (GSWindow *window) {
    g_return_val_if_fail (GS_IS_WINDOW (window), NULL);

    return window->priv->drawing_area;
}

/* just for debugging */
static gboolean
error_watch (GIOChannel *source,
             GIOCondition condition,
             gpointer data) {
    gboolean finished = FALSE;

    if (condition & G_IO_IN) {
        GIOStatus status;
        GError *error = NULL;
        char *line;

        line = NULL;
        status = g_io_channel_read_line (source, &line, NULL, NULL, &error);

        switch (status) {
            case G_IO_STATUS_NORMAL:
                gs_debug ("Command output: %s", line);
                break;
            case G_IO_STATUS_EOF:
                finished = TRUE;
                break;
            case G_IO_STATUS_ERROR:
                gs_debug ("Error reading from child: %s\n", error->message);
                g_error_free (error);
                return FALSE;
            case G_IO_STATUS_AGAIN:
            default:
                break;
        }
        g_free (line);
    } else if (condition & G_IO_HUP) {
        finished = TRUE;
    }

    if (finished) {
        return FALSE;
    }

    return TRUE;
}

static gboolean
spawn_on_window (GtkWidget *socket,
                 char *command,
                 int *pid,
                 GIOFunc watch_func,
                 gboolean redirect_stderr,
                 gpointer user_data,
                 gint *watch_id) {
    int argc;
    char **argv;
    char **envp;
    GError *error;
    gboolean result;
    GIOChannel *channel;
    int standard_output;
    int standard_error;
    int child_pid;
    int id;

    error = NULL;
    if (!g_shell_parse_argv (command, &argc, &argv, &error)) {
        gs_debug ("Could not parse command: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    error = NULL;
    envp = spawn_make_environment_for_display (socket);
    result = spawn_async_with_pipes (socket,
                                     NULL,
                                     argv,
                                     envp,
                                     G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                                     NULL,
                                     NULL,
                                     &child_pid,
                                     NULL,
                                     &standard_output,
                                     redirect_stderr ? &standard_error : NULL,
                                     &error);

    if (!result) {
        gs_debug ("Could not start command '%s': %s", command, error->message);
        g_error_free (error);
        g_strfreev (argv);
        return FALSE;
    }

    if (pid != NULL) {
        *pid = child_pid;
    } else {
        g_spawn_close_pid (child_pid);
    }

    /* output channel */
    channel = g_io_channel_unix_new (standard_output);
    g_io_channel_set_close_on_unref (channel, TRUE);
    g_io_channel_set_flags (channel,
                            g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK,
                            NULL);
    id = g_io_add_watch (channel,
                         G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                         watch_func,
                         user_data);
    if (watch_id != NULL) {
        *watch_id = id;
    }
    g_io_channel_unref (channel);

    /* error channel */
    if (redirect_stderr) {
        channel = g_io_channel_unix_new (standard_error);
        g_io_channel_set_close_on_unref (channel, TRUE);
        g_io_channel_set_flags (channel,
                                g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK,
                                NULL);
        g_io_add_watch (channel,
                        G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                        error_watch,
                        NULL);
        g_io_channel_unref (channel);
    }

    g_strfreev (argv);
    g_strfreev (envp);

    return result;
}

static void
lock_plug_added (GSWindow *window) {
    gtk_widget_show (window->priv->lock_socket);
}

static gboolean
lock_plug_removed (GSWindow *window) {
    gtk_widget_hide (window->priv->lock_socket);
    gtk_container_remove (GTK_CONTAINER (window->priv->overlay), GTK_WIDGET (window->priv->lock_box));
    window->priv->lock_box = NULL;

    gtk_container_remove (GTK_CONTAINER (window->priv->vbox), GTK_WIDGET (window->priv->overlay));
    window->priv->overlay = NULL;

    return TRUE;
}

static void
keyboard_plug_added (GSWindow *window) {
    gtk_widget_show (window->priv->keyboard_socket);
}

static gboolean
keyboard_plug_removed (GSWindow *window) {
    gtk_widget_hide (window->priv->keyboard_socket);
    gtk_container_remove (GTK_CONTAINER (window->priv->overlay), GTK_WIDGET (window->priv->keyboard_socket));

    return TRUE;
}

static void
keyboard_socket_destroyed (GtkWidget *widget,
                           GSWindow *window) {
    g_signal_handlers_disconnect_by_func (widget, keyboard_socket_destroyed, window);
    g_signal_handlers_disconnect_by_func (widget, keyboard_plug_added, window);
    g_signal_handlers_disconnect_by_func (widget, keyboard_plug_removed, window);

    window->priv->keyboard_socket = NULL;
}

static void
forward_key_events (GSWindow *window) {
    window->priv->key_events = g_list_reverse (window->priv->key_events);

    while (window->priv->key_events != NULL) {
        GdkEventKey *event = window->priv->key_events->data;

        gtk_window_propagate_key_event (GTK_WINDOW (window), event);

        gdk_event_free ((GdkEvent *) event);
        window->priv->key_events = g_list_delete_link (window->priv->key_events, window->priv->key_events);
    }
}

static void
remove_key_events (GSWindow *window) {
    window->priv->key_events = g_list_reverse (window->priv->key_events);

    while (window->priv->key_events) {
        GdkEventKey *event = window->priv->key_events->data;

        gdk_event_free ((GdkEvent *) event);
        window->priv->key_events = g_list_delete_link (window->priv->key_events, window->priv->key_events);
    }
}

static void
prefs_changed (GSPrefs *prefs,
               GSWindow *window) {
    if (window->priv->prefs->keyboard_enabled) {
        if (window->priv->keyboard_socket) {
            gtk_widget_set_visible (window->priv->keyboard_socket, window->priv->prefs->keyboard_displayed);
        }
    }
}

static void
lock_socket_show (GtkWidget *widget,
                  GSWindow *window) {
    gtk_widget_child_focus (window->priv->lock_socket, GTK_DIR_TAB_FORWARD);

    /* send queued events to the dialog */
    forward_key_events (window);
}

static void
lock_socket_destroyed (GtkWidget *widget,
                       GSWindow *window) {
    g_signal_handlers_disconnect_by_func (widget, lock_socket_show, window);
    g_signal_handlers_disconnect_by_func (widget, lock_socket_destroyed, window);
    g_signal_handlers_disconnect_by_func (widget, lock_plug_added, window);
    g_signal_handlers_disconnect_by_func (widget, lock_plug_removed, window);
    g_signal_handlers_disconnect_by_func (window->priv->prefs, prefs_changed, window);

    window->priv->lock_socket = NULL;
}

static GtkWidget *
socket_new (GSWindow *window) {
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        return gtk_socket_new ();
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        return wle_gtk_socket_new (gs_manager_get_compositor (window->priv->manager));
    }
#endif
    return NULL;
}

static void
create_keyboard_socket (GSWindow *window,
                        gulong id) {
    GdkDisplay *display;
    GdkMonitor *monitor;
    GdkWindow *g_window;
    GdkRectangle geometry;
    gint screen_height, screen_width;
    gint height, width;

    g_window = gtk_widget_get_window (GTK_WIDGET (window));
    display = gdk_window_get_display (g_window);
    monitor = gdk_display_get_monitor_at_window (display, g_window);
    gdk_monitor_get_geometry (monitor, &geometry);

    screen_height = geometry.height;
    if (screen_height < 610) {
        height = 205 - ((610 - screen_height) / 2); // Approximate, based on Greybird
    } else {
        height = 205; // Native height for onboard
    }

    screen_width = geometry.width;
    if (screen_width < 1400) {
        width = screen_width;
    } else {
        width = 1400; // Native width for onboard
    }

    window->priv->keyboard_socket = socket_new (window);
    gtk_widget_set_size_request (window->priv->keyboard_socket, width, height);
    gtk_widget_set_halign (window->priv->keyboard_socket, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (window->priv->keyboard_socket, GTK_ALIGN_END);

    g_signal_connect (window->priv->keyboard_socket, "destroy",
                      G_CALLBACK (keyboard_socket_destroyed), window);
    g_signal_connect_swapped (window->priv->keyboard_socket, "plug_added",
                              G_CALLBACK (keyboard_plug_added), window);
    g_signal_connect_swapped (window->priv->keyboard_socket, "plug_removed",
                              G_CALLBACK (keyboard_plug_removed), window);

    gtk_overlay_add_overlay (GTK_OVERLAY (window->priv->overlay), window->priv->keyboard_socket);
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        gtk_socket_add_id (GTK_SOCKET (window->priv->keyboard_socket), id);
    }
#endif

    if (!window->priv->prefs->keyboard_displayed) {
        gtk_widget_hide (window->priv->keyboard_socket);
    }
}

/* adapted from gspawn.c */
static int
wait_on_child (int pid) {
    int status;

wait_again:
    if (waitpid (pid, &status, 0) < 0) {
        if (errno == EINTR) {
            goto wait_again;
        } else if (errno == ECHILD) {
            /* do nothing, child already reaped */
        } else {
            gs_debug ("waitpid () should not fail in 'GSWindow'");
        }
    }

    return status;
}

static void
kill_keyboard_process (GSWindow *window) {
    if (window->priv->keyboard_pid > 0) {
        signal_pid (window->priv->keyboard_pid, SIGTERM);
    }
}

static void
kill_dialog_process (GSWindow *window) {
    /* If a dialog is up we need to signal it
       and wait on it */
    if (window->priv->lock_pid > 0) {
        signal_pid (window->priv->lock_pid, SIGTERM);
    }
}

static void
keyboard_process_finish (GSWindow *window) {
    g_return_if_fail (GS_IS_WINDOW (window));

    /* send a signal just in case */
    kill_keyboard_process (window);

    gs_debug ("Keyboard finished");

    if (window->priv->keyboard_pid > 0) {
        wait_on_child (window->priv->keyboard_pid);

        g_spawn_close_pid (window->priv->keyboard_pid);
        window->priv->keyboard_pid = 0;
    }

#ifdef ENABLE_WAYLAND
    if (window->priv->keyboard_socket != NULL) {
        keyboard_plug_removed (window);
    }
#endif
}

static gboolean
keyboard_process_watch (GIOChannel *source,
                        GIOCondition condition,
                        GSWindow *window) {
    gboolean finished = FALSE;

    g_return_val_if_fail (GS_IS_WINDOW (window), FALSE);

    if (condition & G_IO_IN) {
        GIOStatus status;
        GError *error = NULL;
        char *line;

        line = NULL;
        status = g_io_channel_read_line (source, &line, NULL, NULL, &error);

        switch (status) {
            case G_IO_STATUS_NORMAL: {
#ifdef ENABLE_X11
                if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
                    gulong id;
                    char c;
                    gs_debug ("Keyboard command output: %s", line);
                    if (1 == sscanf (line, " %lu %c", &id, &c)) {
                        create_keyboard_socket (window, id);
                    }
                }
#endif
            } break;
            case G_IO_STATUS_EOF:
                finished = TRUE;
                break;
            case G_IO_STATUS_ERROR:
                gs_debug ("Error reading from child: %s\n", error->message);
                g_error_free (error);
                finished = TRUE;
            case G_IO_STATUS_AGAIN:
            default:
                break;
        }

        g_free (line);
    } else if (condition & G_IO_HUP) {
        finished = TRUE;
    }

    if (finished) {
        window->priv->keyboard_watch_id = 0;
        keyboard_process_finish (window);
        return FALSE;
    }

    return TRUE;
}

static void
embed_keyboard (GSWindow *window) {
    gboolean res;

    if (!window->priv->prefs->keyboard_enabled
        || window->priv->prefs->keyboard_command == NULL)
        return;

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        create_keyboard_socket (window, 0);
    }
#endif

    gs_debug ("Adding embedded keyboard widget");

    /* FIXME: verify command is safe */

    gs_debug ("Running command: %s", window->priv->prefs->keyboard_command);

    res = spawn_on_window (window->priv->keyboard_socket,
                           window->priv->prefs->keyboard_command,
                           &window->priv->keyboard_pid,
                           (GIOFunc) keyboard_process_watch,
                           TRUE,
                           window,
                           &window->priv->keyboard_watch_id);
    if (!res) {
        gs_debug ("Could not start command: %s", window->priv->prefs->keyboard_command);
    }
}

static void
create_lock_socket (GSWindow *window,
                    gulong id) {
    window->priv->lock_socket = socket_new (window);
    window->priv->lock_box = gtk_grid_new ();
    gtk_widget_set_halign (GTK_WIDGET (window->priv->lock_box),
                           GTK_ALIGN_CENTER);
    gtk_widget_set_valign (GTK_WIDGET (window->priv->lock_box),
                           GTK_ALIGN_CENTER);
    gtk_widget_show (window->priv->lock_box);

    window->priv->overlay = gtk_overlay_new ();
    gtk_widget_show (window->priv->overlay);
    gtk_box_pack_start (GTK_BOX (window->priv->vbox), window->priv->overlay, TRUE, TRUE, 0);
    gtk_overlay_add_overlay (GTK_OVERLAY (window->priv->overlay), window->priv->lock_box);

    gtk_container_add (GTK_CONTAINER (window->priv->lock_box), window->priv->lock_socket);

    g_signal_connect (window->priv->lock_socket, "show",
                      G_CALLBACK (lock_socket_show), window);
    g_signal_connect (window->priv->lock_socket, "destroy",
                      G_CALLBACK (lock_socket_destroyed), window);
    g_signal_connect_swapped (window->priv->lock_socket, "plug-added",
                              G_CALLBACK (lock_plug_added), window);
    g_signal_connect_swapped (window->priv->lock_socket, "plug-removed",
                              G_CALLBACK (lock_plug_removed), window);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        gtk_socket_add_id (GTK_SOCKET (window->priv->lock_socket), id);
    }
#endif

    if (window->priv->prefs->keyboard_enabled) {
        embed_keyboard (window);
    }

    g_signal_connect (window->priv->prefs, "changed",
                      G_CALLBACK (prefs_changed), window);
}

static void
gs_window_dialog_finish (GSWindow *window) {
    g_return_if_fail (GS_IS_WINDOW (window));

    gs_debug ("Dialog finished");

    /* make sure we finish the keyboard thing too */
    keyboard_process_finish (window);

    /* send a signal just in case */
    kill_dialog_process (window);

    if (window->priv->lock_pid > 0) {
        wait_on_child (window->priv->lock_pid);

        g_spawn_close_pid (window->priv->lock_pid);
        window->priv->lock_pid = 0;
    }

    /* remove events for the case were we failed to show socket */
    remove_key_events (window);
}

static void
window_set_dialog_up (GSWindow *window,
                      gboolean dialog_up) {
    if (window->priv->dialog_up == dialog_up) {
        return;
    }

    window->priv->dialog_up = dialog_up;
    g_object_notify (G_OBJECT (window), "dialog-up");
}

static void
popdown_dialog (GSWindow *window) {
    gs_window_dialog_finish (window);

    gtk_widget_show (window->priv->drawing_area);
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        // TODO: Avoids a critical warning due to GTK bug
        // remove this if https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/6841 get merged
        gtk_widget_set_can_focus (window->priv->drawing_area, TRUE);
        gtk_widget_grab_focus (window->priv->drawing_area);
    }
#endif

    gs_window_clear (window);
    set_invisible_cursor (gtk_widget_get_window (GTK_WIDGET (window)), TRUE);

    window_set_dialog_up (window, FALSE);

    /* reset the pointer positions */
    window->priv->last_x = -1;
    window->priv->last_y = -1;

    if (window->priv->lock_socket != NULL) {
        lock_plug_removed (window);
    }

    remove_popup_dialog_idle (window);
    remove_command_watches (window);
}

static gboolean
dialog_process_watch (GIOChannel *source,
                      GIOCondition condition,
                      GSWindow *window) {
    gboolean finished = FALSE;

    g_return_val_if_fail (GS_IS_WINDOW (window), FALSE);

    if (condition & G_IO_IN) {
        GIOStatus status;
        GError *error = NULL;
        char *line;

        line = NULL;
        status = g_io_channel_read_line (source, &line, NULL, NULL, &error);

        switch (status) {
            case G_IO_STATUS_NORMAL:
                gs_debug ("Command output: %s", line);

                if (strstr (line, "WINDOW ID=") != NULL) {
#ifdef ENABLE_X11
                    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
                        gulong id;
                        char c;
                        if (1 == sscanf (line, " WINDOW ID= %lu %c", &id, &c)) {
                            create_lock_socket (window, id);
                        }
                    }
#endif
                } else if (strstr (line, "NOTICE=") != NULL) {
                    if (strstr (line, "NOTICE=AUTH FAILED") != NULL) {
                        gs_debug ("Authorization failed");
                    }
                } else if (strstr (line, "RESPONSE=") != NULL) {
                    if (strstr (line, "RESPONSE=OK") != NULL) {
                        gs_debug ("Got OK response");
                        window->priv->dialog_response = DIALOG_RESPONSE_OK;
                    } else {
                        gs_debug ("Got CANCEL response");
                        window->priv->dialog_response = DIALOG_RESPONSE_CANCEL;
                    }
                    finished = TRUE;
                } else if (strstr (line, "REQUEST QUIT") != NULL) {
                    gs_debug ("Got request for quit");
                    kill_dialog_process (window);
                }
                break;
            case G_IO_STATUS_EOF:
                finished = TRUE;
                break;
            case G_IO_STATUS_ERROR:
                gs_debug ("Error reading from child: %s\n", error->message);
                g_error_free (error);
                finished = TRUE;
            case G_IO_STATUS_AGAIN:
            default:
                break;
        }

        g_free (line);
    } else if (condition & G_IO_HUP) {
        finished = TRUE;
    }

    if (finished) {
        if (window->priv->dialog_response == DIALOG_RESPONSE_OK) {
            add_emit_deactivated_idle (window);
        } else {
            /* do not notify dialog-up when the window is about to be destroyed,
             * as this is unnecessary and may cause X11 errors related to grab
             * and GtkSocket/GtkPlug (see also gs_manager.c:add_overlays) */
            popdown_dialog (window);
        }

        window->priv->lock_watch_id = 0;

        return FALSE;
    }

    return TRUE;
}

static gboolean
is_logout_enabled (GSWindow *window) {
    double elapsed;

    if (!window->priv->prefs->logout_enabled) {
        return FALSE;
    }

    if (!window->priv->prefs->logout_command) {
        return FALSE;
    }

    elapsed = g_timer_elapsed (window->priv->timer, NULL);

    if (window->priv->prefs->logout_timeout < (guint) elapsed) {
        return TRUE;
    }

    return FALSE;
}

static gboolean
is_user_switch_enabled (GSWindow *window) {
    return window->priv->prefs->user_switch_enabled;
}

static gboolean
is_status_message_enabled (GSWindow *window) {
    return window->priv->prefs->status_message_enabled;
}

static gint
get_monitor_index (GSWindow *window) {
    GdkDisplay *display = gs_window_get_display (window);
    gint idx;

    for (idx = 0; idx < gdk_display_get_n_monitors (display); idx++) {
        GdkMonitor *monitor;
        monitor = gdk_display_get_monitor (display, idx);
        if (monitor == window->priv->monitor) {
            return idx;
        }
    }

    return 0;
}

static void
popup_dialog (GSWindow *window) {
    gboolean result;
    char *tmp;
    GString *command;

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        if (!gtk_window_is_active (GTK_WINDOW (window))) {
            /* active window changed the time we got here, we won't have focus */
            gs_debug ("Cancelling dialog popup because window is not active");
            popdown_dialog (window);
            return;
        }
    }
#endif

    gs_debug ("Popping up dialog");

    tmp = g_build_filename (LIBEXECDIR, "xfce4-screensaver-dialog", NULL);
    command = g_string_new (tmp);
    g_free (tmp);

    g_string_append_printf (command, " --monitor='%i'", get_monitor_index (window));
    g_string_append_printf (command, " --height='%i'", window->priv->geometry.height);
    g_string_append_printf (command, " --width='%i'", window->priv->geometry.width);

    if (is_logout_enabled (window)) {
        command = g_string_append (command, " --enable-logout");
        g_string_append_printf (command, " --logout-command='%s'", window->priv->prefs->logout_command);
    }

    if (is_status_message_enabled (window) && window->priv->status_message) {
        char *quoted;

        quoted = g_shell_quote (window->priv->status_message);
        g_string_append_printf (command, " --status-message=%s", quoted);
        g_free (quoted);
    }

    if (is_user_switch_enabled (window)) {
        command = g_string_append (command, " --enable-switch");
    }

    if (gs_debug_enabled ()) {
        command = g_string_append (command, " --verbose");
    }

    gtk_widget_hide (window->priv->drawing_area);

    gtk_widget_queue_draw (GTK_WIDGET (window));
    set_invisible_cursor (gtk_widget_get_window (GTK_WIDGET (window)), FALSE);

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        create_lock_socket (window, 0);
    }
#endif

    gs_debug ("Executing %s", command->str);
    result = spawn_on_window (window->priv->lock_socket,
                              command->str,
                              &window->priv->lock_pid,
                              (GIOFunc) dialog_process_watch,
                              FALSE,
                              window,
                              &window->priv->lock_watch_id);
    if (!result) {
        gs_debug ("Could not start command: %s", command->str);
    }

    g_string_free (command, TRUE);
}

static gboolean
popup_dialog_idle (gpointer user_data) {
    GSWindow *window = user_data;

    popup_dialog (window);

    window->priv->popup_dialog_idle_id = 0;

    return FALSE;
}

void
gs_window_request_unlock (GSWindow *window) {
    g_return_if_fail (GS_IS_WINDOW (window));


    if (!gtk_widget_get_visible (GTK_WIDGET (window))) {
        gs_debug ("Request unlock but window is not visible!");
        return;
    }

    if (window->priv->lock_watch_id > 0) {
        return;
    }

    if (!window->priv->lock_active) {
        add_emit_deactivated_idle (window);
        return;
    }

    if (window->priv->popup_dialog_idle_id == 0) {
        add_popup_dialog_idle (window);
    }
    window_set_dialog_up (window, TRUE);
}

void
gs_window_set_lock_active (GSWindow *window,
                           gboolean active) {
    g_return_if_fail (GS_IS_WINDOW (window));

    if (window->priv->lock_active == active) {
        return;
    }

    gs_debug ("Setting lock active: %i", active);

    window->priv->lock_active = active;
}

void
gs_window_cancel_unlock_request (GSWindow *window) {
    /* FIXME: This is a bit of a hammer approach...
     * Maybe we should send a delete-event to the plug?
     */
    g_return_if_fail (GS_IS_WINDOW (window));

    popdown_dialog (window);
}

GdkDisplay *
gs_window_get_display (GSWindow *window) {
    g_return_val_if_fail (GS_IS_WINDOW (window), NULL);

    return gtk_widget_get_display (GTK_WIDGET (window));
}

void
gs_window_set_status_message (GSWindow *window,
                              const char *status_message) {
    g_return_if_fail (GS_IS_WINDOW (window));

    g_free (window->priv->status_message);
    window->priv->status_message = g_strdup (status_message);
}

void
gs_window_set_monitor (GSWindow *window,
                       GdkMonitor *monitor) {
    gint tmp;

    g_return_if_fail (GS_IS_WINDOW (window));

    if (window->priv->monitor == monitor) {
        return;
    }

    window->priv->monitor = monitor;
    g_object_add_weak_pointer (G_OBJECT (monitor), (gpointer *) &window->priv->monitor);

    gtk_widget_queue_resize (GTK_WIDGET (window));
    gtk_widget_get_preferred_width (GTK_WIDGET (window), &tmp, &tmp);

    g_object_notify (G_OBJECT (window), "monitor");
}

GdkMonitor *
gs_window_get_monitor (GSWindow *window) {
    g_return_val_if_fail (GS_IS_WINDOW (window), NULL);

    return window->priv->monitor;
}

static void
gs_window_set_property (GObject *object,
                        guint prop_id,
                        const GValue *value,
                        GParamSpec *pspec) {
    GSWindow *self;

    self = GS_WINDOW (object);

    switch (prop_id) {
        case PROP_MONITOR:
            gs_window_set_monitor (self, g_value_get_pointer (value));
            break;
        case PROP_OBSCURED:
            gs_window_set_obscured (self, g_value_get_boolean (value));
            break;
        case PROP_DIALOG_UP:
            window_set_dialog_up (self, g_value_get_boolean (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_window_get_property (GObject *object,
                        guint prop_id,
                        GValue *value,
                        GParamSpec *pspec) {
    GSWindow *self;

    self = GS_WINDOW (object);

    switch (prop_id) {
        case PROP_MONITOR:
            g_value_set_pointer (value, (gpointer) self->priv->monitor);
            break;
        case PROP_OBSCURED:
            g_value_set_boolean (value, self->priv->obscured);
            break;
        case PROP_DIALOG_UP:
            g_value_set_boolean (value, self->priv->dialog_up);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
queue_key_event (GSWindow *window,
                 GdkEventKey *event) {
    /* Eat the first return, enter, escape, or space */
    if (window->priv->key_events == NULL
        && (event->keyval == GDK_KEY_Return
            || event->keyval == GDK_KEY_KP_Enter
            || event->keyval == GDK_KEY_Escape
            || event->keyval == GDK_KEY_space)) {
        return;
    }

    /* Only cache MAX_QUEUED_EVENTS key events.  If there are any more than this then
       something is wrong */
    /* Don't queue keys that may cause focus navigation in the dialog */
    if (g_list_length (window->priv->key_events) < MAX_QUEUED_EVENTS
        && event->keyval != GDK_KEY_Tab
        && event->keyval != GDK_KEY_Up
        && event->keyval != GDK_KEY_Down) {
        window->priv->key_events = g_list_prepend (window->priv->key_events,
                                                   gdk_event_copy ((GdkEvent *) event));
    }
}

static gboolean
maybe_handle_activity (GSWindow *window) {
    gboolean handled;

    handled = FALSE;

    /* if we already have a socket then don't bother */
    if (!window->priv->lock_socket
        && gtk_widget_get_sensitive (GTK_WIDGET (window))) {
        g_signal_emit (window, signals[ACTIVITY], 0, &handled);
    }

    return handled;
}

static gboolean
gs_window_real_key_press_event (GtkWidget *widget,
                                GdkEventKey *event) {
    /*g_message ("KEY PRESS state: %u keyval %u", event->state, event->keyval);*/

    /* Ignore brightness keys */
    if (event->hardware_keycode == 101 || event->hardware_keycode == 212) {
        gs_debug ("Ignoring brightness keys");
        return TRUE;
    }

    maybe_handle_activity (GS_WINDOW (widget));

    queue_key_event (GS_WINDOW (widget), event);

    if (GTK_WIDGET_CLASS (gs_window_parent_class)->key_press_event) {
        GTK_WIDGET_CLASS (gs_window_parent_class)->key_press_event (widget, event);
    }

    return TRUE;
}

static gboolean
gs_window_real_motion_notify_event (GtkWidget *widget,
                                    GdkEventMotion *event) {
    GSWindow *window;
    gdouble distance;
    gdouble min_distance;
    gdouble min_percentage = 0.1;
    GdkDisplay *display;
    GdkScreen *screen;

    window = GS_WINDOW (widget);

    display = gs_window_get_display (window);
    screen = gdk_display_get_default_screen (display);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    min_distance = gdk_screen_get_width (screen) * min_percentage;
    G_GNUC_END_IGNORE_DEPRECATIONS

    /* if the last position was not set then don't detect motion */
    if (window->priv->last_x < 0 || window->priv->last_y < 0) {
        window->priv->last_x = event->x;
        window->priv->last_y = event->y;

        return FALSE;
    }

    /* just an approximate distance */
    distance = MAX (ABS (window->priv->last_x - event->x),
                    ABS (window->priv->last_y - event->y));

    if (distance > min_distance) {
        maybe_handle_activity (window);

        window->priv->last_x = -1;
        window->priv->last_y = -1;
    }

    return FALSE;
}

static gboolean
gs_window_real_button_press_event (GtkWidget *widget,
                                   GdkEventButton *event) {
    GSWindow *window;

    window = GS_WINDOW (widget);
    maybe_handle_activity (window);

    return FALSE;
}

static gboolean
gs_window_real_scroll_event (GtkWidget *widget,
                             GdkEventScroll *event) {
    GSWindow *window;

    window = GS_WINDOW (widget);
    maybe_handle_activity (window);

    return FALSE;
}

static void
gs_window_real_size_request (GtkWidget *widget,
                             GtkRequisition *requisition) {
    GSWindow *window;
    GtkBin *bin;
    GtkWidget *child;
    GdkRectangle old_geometry;
    int position_changed = FALSE;
    int size_changed = FALSE;

    window = GS_WINDOW (widget);
    bin = GTK_BIN (widget);
    child = gtk_bin_get_child (bin);

    // monitor been removed, but we will return it later on
    if (window->priv->monitor == NULL)
        return;

    if (child && gtk_widget_get_visible (child)) {
        gtk_widget_get_preferred_size (child, requisition, NULL);
    }

    old_geometry = window->priv->geometry;

    update_geometry (window);

    requisition->width = window->priv->geometry.width;
    requisition->height = window->priv->geometry.height;

    if (!gtk_widget_get_realized (widget)) {
        return;
    }

    if (old_geometry.width != window->priv->geometry.width
        || old_geometry.height != window->priv->geometry.height) {
        size_changed = TRUE;
    }

    if (old_geometry.x != window->priv->geometry.x
        || old_geometry.y != window->priv->geometry.y) {
        position_changed = TRUE;
    }

    gs_window_move_resize_window (window, position_changed, size_changed);
}

static void
gs_window_real_get_preferred_width (GtkWidget *widget,
                                    gint *minimal_width,
                                    gint *natural_width) {
    GtkRequisition requisition = { 0 };
    gs_window_real_size_request (widget, &requisition);
    *minimal_width = *natural_width = requisition.width;
}

static void
gs_window_real_get_preferred_height (GtkWidget *widget,
                                     gint *minimal_height,
                                     gint *natural_height) {
    GtkRequisition requisition = { 0 };
    gs_window_real_size_request (widget, &requisition);
    *minimal_height = *natural_height = requisition.height;
}

void
gs_window_reposition (GSWindow *window) {
    GtkRequisition requisition;
    gs_window_real_size_request (GTK_WIDGET (window), &requisition);
}

#ifdef ENABLE_X11
static gboolean
gs_window_real_grab_broken (GtkWidget *widget,
                            GdkEventGrabBroken *event) {
    if (event->grab_window != NULL) {
        gs_debug ("Grab broken on window %lX %s, new grab on window %lX",
                  GDK_WINDOW_XID (event->window),
                  event->keyboard ? "keyboard" : "pointer",
                  GDK_WINDOW_XID (event->grab_window));
    } else {
        gs_debug ("Grab broken on window %lX %s, new grab is outside application",
                  GDK_WINDOW_XID (event->window),
                  event->keyboard ? "keyboard" : "pointer");
    }

    return FALSE;
}
#endif

gboolean
gs_window_is_obscured (GSWindow *window) {
    g_return_val_if_fail (GS_IS_WINDOW (window), FALSE);

    return window->priv->obscured;
}

gboolean
gs_window_is_dialog_up (GSWindow *window) {
    g_return_val_if_fail (GS_IS_WINDOW (window), FALSE);

    return window->priv->dialog_up;
}

static void
gs_window_set_obscured (GSWindow *window,
                        gboolean obscured) {
    if (window->priv->obscured == obscured) {
        return;
    }

    window->priv->obscured = obscured;
    g_object_notify (G_OBJECT (window), "obscured");
}

static gboolean
gs_window_real_visibility_notify_event (GtkWidget *widget,
                                        GdkEventVisibility *event) {
    switch (event->state) {
        case GDK_VISIBILITY_FULLY_OBSCURED:
            gs_window_set_obscured (GS_WINDOW (widget), TRUE);
            break;
        case GDK_VISIBILITY_PARTIAL:
            break;
        case GDK_VISIBILITY_UNOBSCURED:
            gs_window_set_obscured (GS_WINDOW (widget), FALSE);
            break;
        default:
            break;
    }

    return FALSE;
}

static void
gs_window_class_init (GSWindowClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = gs_window_finalize;
    object_class->get_property = gs_window_get_property;
    object_class->set_property = gs_window_set_property;

    widget_class->show = gs_window_real_show;
    widget_class->hide = gs_window_real_hide;
    widget_class->draw = gs_window_real_draw;
    widget_class->realize = gs_window_real_realize;
    widget_class->unrealize = gs_window_real_unrealize;
    widget_class->key_press_event = gs_window_real_key_press_event;
    widget_class->motion_notify_event = gs_window_real_motion_notify_event;
    widget_class->button_press_event = gs_window_real_button_press_event;
    widget_class->scroll_event = gs_window_real_scroll_event;
    widget_class->get_preferred_width = gs_window_real_get_preferred_width;
    widget_class->get_preferred_height = gs_window_real_get_preferred_height;
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        widget_class->grab_broken_event = gs_window_real_grab_broken;
    }
#endif
    widget_class->visibility_notify_event = gs_window_real_visibility_notify_event;

    signals[ACTIVITY] =
        g_signal_new ("activity",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSWindowClass, activity),
                      NULL,
                      NULL,
                      gs_marshal_BOOLEAN__VOID,
                      G_TYPE_BOOLEAN,
                      0);
    signals[DEACTIVATED] =
        g_signal_new ("deactivated",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSWindowClass, deactivated),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    g_object_class_install_property (object_class,
                                     PROP_OBSCURED,
                                     g_param_spec_boolean ("obscured",
                                                           NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));
    g_object_class_install_property (object_class,
                                     PROP_DIALOG_UP,
                                     g_param_spec_boolean ("dialog-up",
                                                           NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));
    g_object_class_install_property (object_class,
                                     PROP_MONITOR,
                                     g_param_spec_pointer ("monitor",
                                                           "Gdk monitor",
                                                           "The monitor (in terms of Gdk) which the window is on",
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
create_info_bar (GSWindow *window) {
    window->priv->info_bar = gtk_info_bar_new ();
    gtk_widget_set_no_show_all (window->priv->info_bar, TRUE);
    gtk_box_pack_end (GTK_BOX (window->priv->vbox), window->priv->info_bar, FALSE, FALSE, 0);
}

static gboolean
on_drawing_area_draw (GtkWidget *widget,
                      cairo_t *cr) {
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_paint (cr);

    return FALSE;
}

static void
gs_window_init (GSWindow *window) {
    window->priv = gs_window_get_instance_private (window);

    window->priv->geometry.x = -1;
    window->priv->geometry.y = -1;
    window->priv->geometry.width = -1;
    window->priv->geometry.height = -1;

    window->priv->last_x = -1;
    window->priv->last_y = -1;

    window->priv->manager = gs_manager_new ();
    window->priv->prefs = gs_prefs_new ();

    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
    gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);

    gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);

    gtk_window_fullscreen (GTK_WINDOW (window));

    gtk_widget_set_events (GTK_WIDGET (window),
                           gtk_widget_get_events (GTK_WIDGET (window))
                               | GDK_POINTER_MOTION_MASK
                               | GDK_BUTTON_PRESS_MASK
                               | GDK_BUTTON_RELEASE_MASK
                               | GDK_KEY_PRESS_MASK
                               | GDK_KEY_RELEASE_MASK
                               | GDK_EXPOSURE_MASK
                               | GDK_VISIBILITY_NOTIFY_MASK
                               | GDK_ENTER_NOTIFY_MASK
                               | GDK_LEAVE_NOTIFY_MASK);

    window->priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_show (window->priv->vbox);
    gtk_container_add (GTK_CONTAINER (window), window->priv->vbox);

    window->priv->drawing_area = socket_new (window);
    gtk_widget_show (window->priv->drawing_area);
    gtk_widget_set_app_paintable (window->priv->drawing_area, TRUE);
    gtk_box_pack_start (GTK_BOX (window->priv->vbox),
                        window->priv->drawing_area, TRUE, TRUE, 0);
    g_signal_connect (window->priv->drawing_area,
                      "draw",
                      G_CALLBACK (on_drawing_area_draw),
                      NULL);
    g_signal_connect (window->priv->drawing_area,
                      "plug-removed",
                      G_CALLBACK (gtk_true),
                      NULL);
    create_info_bar (window);
}

static void
remove_command_watches (GSWindow *window) {
    if (window->priv->lock_watch_id != 0) {
        g_source_remove (window->priv->lock_watch_id);
        window->priv->lock_watch_id = 0;
    }
    if (window->priv->keyboard_watch_id != 0) {
        g_source_remove (window->priv->keyboard_watch_id);
        window->priv->keyboard_watch_id = 0;
    }
}

static void
gs_window_finalize (GObject *object) {
    GSWindow *window;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_WINDOW (object));

    window = GS_WINDOW (object);

    g_return_if_fail (window->priv != NULL);

    if (window->priv->info_bar_timer_id > 0) {
        g_source_remove (window->priv->info_bar_timer_id);
        window->priv->info_bar_timer_id = 0;
    }

    remove_watchdog_timer (window);
    remove_popup_dialog_idle (window);
    if (window->priv->deactivated_idle_id > 0) {
        g_source_remove (window->priv->deactivated_idle_id);
        window->priv->deactivated_idle_id = 0;
    }
    if (window->priv->monitor != NULL) {
        g_object_remove_weak_pointer (G_OBJECT (window->priv->monitor), (gpointer *) &window->priv->monitor);
    }

    if (window->priv->timer) {
        g_timer_destroy (window->priv->timer);
    }

    remove_key_events (window);

    remove_command_watches (window);

    gs_window_dialog_finish (window);
    g_object_unref (window->priv->prefs);
    g_object_unref (window->priv->manager);

    G_OBJECT_CLASS (gs_window_parent_class)->finalize (object);
}

GSWindow *
gs_window_new (GdkMonitor *monitor) {
    GObject *result;
    GdkDisplay *display = gdk_monitor_get_display (monitor);
    GdkScreen *screen = gdk_display_get_default_screen (display);

    result = g_object_new (GS_TYPE_WINDOW,
                           "type", GTK_WINDOW_POPUP,
                           "screen", screen,
                           "monitor", monitor,
                           "app-paintable", TRUE,
                           NULL);

    return GS_WINDOW (result);
}
