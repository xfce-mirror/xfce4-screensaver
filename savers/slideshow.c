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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include <libwlembed-gtk3/libwlembed-gtk3.h>
#endif

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "gste-slideshow.h"
#include "xdg-user-dir-lookup.h"

int
main (int argc, char **argv) {
    GSThemeEngine *engine;
    GtkWidget *window = NULL;
    GError *error;
    gboolean ret;
    char *location = NULL;
    char *background_color = NULL;
    gboolean sort_images = FALSE;
    gboolean no_stretch = FALSE;
    gboolean no_crop = FALSE;
    GOptionEntry entries[] = {
        { "location", 0, 0, G_OPTION_ARG_STRING, &location,
          N_ ("Location to get images from"), N_ ("PATH") },
        { "background-color", 0, 0, G_OPTION_ARG_STRING, &background_color,
          N_ ("Color to use for images background"), N_ ("\"#rrggbb\"") },
        { "sort-images", 0, 0, G_OPTION_ARG_NONE, &sort_images,
          N_ ("Do not randomize pictures from location"), NULL },
        { "no-stretch", 0, 0, G_OPTION_ARG_NONE, &no_stretch,
          N_ ("Do not try to stretch images on screen"), NULL },
        { "no-crop", 0, 0, G_OPTION_ARG_NONE, &no_crop,
          N_ ("Do not crop images to the screen size"), NULL },
        { NULL }
    };

    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

    error = NULL;

    ret = gtk_init_with_args (&argc, &argv,
                              NULL,
                              entries,
                              NULL,
                              &error);
    if (!ret) {
        g_message ("%s", error->message);
        g_error_free (error);
        return EXIT_FAILURE;
    }

    g_chdir (g_get_home_dir ());

    g_set_prgname ("slideshow");

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        window = gtk_plug_new (strtoul (g_getenv ("XSCREENSAVER_WINDOW"), NULL, 0));
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        window = wle_gtk_plug_new (g_getenv ("XSCREENSAVER_WINDOW"));
    }
#endif
    gtk_widget_set_app_paintable (window, TRUE);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (gtk_main_quit), NULL);

    engine = g_object_new (GSTE_TYPE_SLIDESHOW, NULL);

    if (location == NULL) {
        location = xdg_user_dir_lookup ("PICTURES");
        if (location == NULL || strcmp (location, "/tmp") == 0 || strcmp (location, g_get_home_dir ()) == 0) {
            g_free (location);
            location = g_build_filename (g_get_home_dir (), "Pictures", NULL);
        }
    }

    if (location != NULL) {
        g_object_set (engine, "images-location", location, NULL);
        g_free (location);
    }

    if (sort_images) {
        g_object_set (engine, "sort-images", sort_images, NULL);
    }

    if (background_color != NULL) {
        g_object_set (engine, "background-color", background_color, NULL);
    }

    if (no_stretch) {
        g_object_set (engine, "no-stretch", no_stretch, NULL);
    }

    if (no_crop) {
        g_object_set (engine, "no-crop", no_crop, NULL);
    }

    gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (engine));

    gtk_widget_show (GTK_WIDGET (engine));

    gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
    gtk_widget_show (window);

    gtk_main ();

    return EXIT_SUCCESS;
}
