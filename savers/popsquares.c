/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-
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
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <gtk/gtkx.h>

#include <libxfce4util/libxfce4util.h>

#include "gs-theme-engine.h"
#include "gste-popsquares.h"

int
main (int    argc,
      char **argv) {
    GSThemeEngine *engine;
    GtkWidget     *window;
    GError        *error;

    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

    error = NULL;

    if (!gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
        g_printerr (_("%s. See --help for usage information.\n"),
                    error->message);
        g_error_free (error);
        return EXIT_FAILURE;
    }

    window = gtk_plug_new (strtoul (g_getenv ("XSCREENSAVER_WINDOW"), NULL, 0));
    gtk_widget_set_app_paintable (window, TRUE);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (gtk_main_quit), NULL);

    g_set_prgname ("popsquares");

    engine = g_object_new (GSTE_TYPE_POPSQUARES, NULL);
    gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (engine));

    gtk_widget_show (GTK_WIDGET (engine));

    gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
    gtk_widget_show (window);

    gtk_main ();

    return EXIT_SUCCESS;
}
