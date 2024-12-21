/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"

#include <stdlib.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>

#include "gs-grab.h"
static GSGrab *grab = NULL;
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "gs-debug.h"
#include "gs-window.h"

static void
window_deactivated_cb (GSWindow *window,
                       gpointer data) {
    gs_window_destroy (window);
}

static void
window_dialog_up_cb (GSWindow *window,
                     gpointer data) {
}

static void
window_dialog_down_cb (GSWindow *window,
                       gpointer data) {
}

static void
window_show_cb (GSWindow *window,
                gpointer data) {
#ifdef ENABLE_X11
    if (grab != NULL) {
        /* move devices grab so that dialog can be used */
        gs_grab_move_to_window (grab,
                                gs_window_get_gdk_window (window),
                                gs_window_get_display (window),
                                TRUE, FALSE);
    }
#endif
}

static gboolean
window_activity_cb (GSWindow *window,
                    gpointer data) {
    gs_window_request_unlock (window);

    return TRUE;
}

static gboolean
auth_timeout (gpointer user_data) {
    gtk_main_quit ();
    return FALSE;
}
static void
disconnect_window_signals (GSWindow *window) {
    gpointer data;

    data = NULL;
    g_signal_handlers_disconnect_by_func (window, window_activity_cb, data);
    g_signal_handlers_disconnect_by_func (window, window_deactivated_cb, data);
    g_signal_handlers_disconnect_by_func (window, window_dialog_up_cb, data);
    g_signal_handlers_disconnect_by_func (window, window_dialog_down_cb, data);
    g_signal_handlers_disconnect_by_func (window, window_show_cb, data);
}

static void
window_destroyed_cb (GtkWindow *window,
                     gpointer data) {
    disconnect_window_signals (GS_WINDOW (window));
#ifdef ENABLE_X11
    if (grab != NULL) {
        gs_grab_release (grab, TRUE);
    }
#endif
    gtk_main_quit ();
}

static void
connect_window_signals (GSWindow *window) {
    gpointer data;

    data = NULL;

    g_signal_connect_object (window, "activity",
                             G_CALLBACK (window_activity_cb), data, 0);
    g_signal_connect_object (window, "destroy",
                             G_CALLBACK (window_destroyed_cb), data, 0);
    g_signal_connect_object (window, "deactivated",
                             G_CALLBACK (window_deactivated_cb), data, 0);
    g_signal_connect_object (window, "dialog-up",
                             G_CALLBACK (window_dialog_up_cb), data, 0);
    g_signal_connect_object (window, "dialog-down",
                             G_CALLBACK (window_dialog_down_cb), data, 0);
    g_signal_connect_object (window, "show",
                             G_CALLBACK (window_show_cb), data, 0);
}

static void
test_window (void) {
    GSWindow *window;
    GdkDisplay *display;
    GdkMonitor *monitor;

    display = gdk_display_get_default ();
    monitor = gdk_display_get_primary_monitor (display);

    window = gs_window_new (monitor);
    connect_window_signals (window);

    gs_window_show (window);
}

int
main (int argc,
      char **argv) {
    GError *error = NULL;

    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

    if (!gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
        fprintf (stderr, "%s", error->message);
        g_error_free (error);
        return EXIT_FAILURE;
    }

    if (!xfconf_init (&error)) {
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    gs_debug_init (TRUE, FALSE);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        grab = gs_grab_new ();
    }
#endif

    test_window ();

    /* safety valve in case we can't authenticate */
    g_timeout_add_seconds (30, auth_timeout, NULL);

    gtk_main ();

#ifdef ENABLE_X11
    if (grab != NULL) {
        g_object_unref (grab);
    }
#endif

    gs_debug_shutdown ();
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
