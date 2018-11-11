/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/* xfce-desktop-utils.c - Utilities for the Xfce Desktop

   Copyright (C) 1998 Tom Tromey
   Copyright (C) 2018 Sean Davis <bluesabre@xfce.org>
   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.  */
/*
  @NOTATION@
 */

#include <config.h>

#include <gio/gio.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce-desktop-utils.h>

/**
 * xfce_gdk_spawn_command_line_on_screen:
 * @screen: a GdkScreen
 * @command: a command line
 * @error: return location for errors
 *
 * This is a replacement for gdk_spawn_command_line_on_screen, deprecated
 * in GDK 2.24 and removed in GDK 3.0.
 *
 * gdk_spawn_command_line_on_screen is like g_spawn_command_line_async(),
 * except the child process is spawned in such an environment that on
 * calling gdk_display_open() it would be returned a GdkDisplay with
 * screen as the default screen.
 *
 * This is useful for applications which wish to launch an application
 * on a specific screen.
 *
 * Returns: TRUE on success, FALSE if error is set.
 *
 * Since: 1.7.1
 **/
gboolean
xfce_gdk_spawn_command_line_on_screen (GdkScreen    *screen,
                                       const gchar  *command,
                                       GError      **error) {
    GAppInfo            *appinfo = NULL;
    GdkAppLaunchContext *context = NULL;
    gboolean             res = FALSE;

    appinfo = g_app_info_create_from_commandline (command, NULL, G_APP_INFO_CREATE_NONE, error);

    if (appinfo) {
        context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
        res = g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (context), error);
        g_object_unref (context);
        g_object_unref (appinfo);
    }

    return res;
}
