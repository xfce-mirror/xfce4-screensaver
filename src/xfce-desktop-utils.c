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
#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include <libwlembed-gtk3/libwlembed-gtk3.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfce-desktop-utils.h"
#include "gs-debug.h"

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

gchar **
spawn_make_environment_for_display (GtkWidget *socket) {
    gchar **retval = NULL;
    const gchar *display_name = gdk_display_get_name (gdk_display_get_default ());
    gchar **envp = g_get_environ ();
    gint env_len = g_strv_length (envp);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        retval = g_new0 (char *, env_len + 1);
        for (gint i = 0; i < env_len; i++) {
            if (strcmp (envp[i], "DISPLAY") == 0) {
                retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
            } else {
                retval[i] = g_strdup (envp[i]);
            }
        }
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        retval = g_new0 (char *, env_len + 3);
        for (gint i = 0; i < env_len; i++) {
            retval[i] = g_strdup (envp[i]);
        }
        retval[env_len] = g_strdup_printf ("WAYLAND_DISPLAY=%s", display_name);
        retval[env_len + 1] = g_strdup_printf ("WLE_EMBEDDING_TOKEN=%s", wle_gtk_socket_get_embedding_token (WLE_GTK_SOCKET (socket)));
    }
#endif

    g_strfreev (envp);

    return retval;
}

gboolean
spawn_async_with_pipes (GtkWidget *socket,
                        const gchar *working_directory,
                        gchar **argv,
                        gchar **envp,
                        GSpawnFlags flags,
                        GSpawnChildSetupFunc child_setup,
                        gpointer user_data,
                        GPid *child_pid,
                        gint *standard_input,
                        gint *standard_output,
                        gint *standard_error,
                        GError **error) {
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        return g_spawn_async_with_pipes (working_directory, argv, envp, flags, child_setup, user_data,
                                         child_pid, standard_input, standard_output, standard_error, error);
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        return wle_embedded_compositor_spawn_with_pipes (wle_gtk_socket_get_embedded_compositor (WLE_GTK_SOCKET (socket)),
                                                         working_directory, argv, envp, flags, child_setup, user_data,
                                                         child_pid, standard_input, standard_output, standard_error, error);
    }
#endif
    return FALSE;
}
