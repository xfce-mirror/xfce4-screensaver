/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2018      Sean Davis <bluesabre@xfce.org>
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

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "gs-debug.h"
#include "gs-monitor.h"
#include "xfce4-screensaver.h"

void
xfce4_screensaver_quit (void) {
    gtk_main_quit ();
}

int
main (int argc,
      char **argv) {
    GSMonitor *monitor;
    GError *error = NULL;

    static gboolean show_version = FALSE;
    static gboolean debug = FALSE;

    static GOptionEntry entries[] = {
        { "version", 0, 0, G_OPTION_ARG_NONE, &show_version, N_ ("Version of this application"), NULL },
        { "debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_ ("Enable debugging code"), NULL },
        { NULL }
    };

    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

    if (!gtk_init_with_args (&argc, &argv, NULL, entries, NULL, &error)) {
        if (error) {
            g_warning ("%s", error->message);
            g_error_free (error);
        } else {
            g_warning ("Unable to initialize GTK+");
        }

        return EXIT_FAILURE;
    }

    if (show_version) {
        g_print ("%s %s\n", argv[0], VERSION_FULL);
        return EXIT_SUCCESS;
    }

#if defined(ENABLE_X11) && !defined(ENABLE_WAYLAND)
    if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        g_warning ("Unsupported windowing environment");
        return EXIT_FAILURE;
    }
#elif defined(ENABLE_WAYLAND) && !defined(ENABLE_X11)
    if (!GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        g_warning ("Unsupported windowing environment");
        return EXIT_FAILURE;
    }
#endif

    if (!xfconf_init (&error)) {
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* debug to a file if in deamon mode */
    gs_debug_init (debug, FALSE);
    gs_debug ("Initializing xfce4-screensaver %s", VERSION_FULL);

    monitor = gs_monitor_new ();

    if (monitor == NULL) {
        return EXIT_FAILURE;
    }

    error = NULL;

    if (!gs_monitor_start (monitor, &error)) {
        if (error) {
            g_warning ("%s", error->message);
            g_error_free (error);
        } else {
            g_warning ("Unable to start screensaver");
        }

        g_object_unref (monitor);
        return EXIT_FAILURE;
    }

    gtk_main ();

    g_object_unref (monitor);

    gs_debug ("xfce4-screensaver finished");

    gs_debug_shutdown ();
    xfconf_shutdown ();

    return 0;
}
