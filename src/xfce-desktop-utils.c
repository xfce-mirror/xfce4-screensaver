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

#include <string.h>
#include <gio/gio.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce-desktop-utils.h>

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
spawn_make_environment_for_display (GdkDisplay  *display,
                                    gchar      **envp) {
    gchar       **retval = NULL;
    const gchar  *display_name;
    gint          display_index = -1;
    gint          i, env_len;
    gboolean      own_envp = FALSE;

    g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

    if (envp == NULL) {
        envp = g_get_environ ();
        own_envp = TRUE;
    }

    for (env_len = 0; envp[env_len]; env_len++)
        if (strncmp (envp[env_len], "DISPLAY", strlen ("DISPLAY")) == 0)
            display_index = env_len;

    retval = g_new (char *, env_len + 1);
    retval[env_len] = NULL;

    display_name = gdk_display_get_name (display);

    for (i = 0; i < env_len; i++)
        if (i == display_index)
            retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
        else
            retval[i] = g_strdup (envp[i]);

    if (own_envp)
        g_strfreev (envp);

    g_assert (i == env_len);

    return retval;
}

static gboolean
spawn_command_line_on_display_sync (GdkDisplay  *display,
                                    const gchar  *command_line,
                                    char        **standard_output,
                                    char        **standard_error,
                                    int          *exit_status,
                                    GError      **error) {
    char     **argv = NULL;
    char     **envp = NULL;
    gboolean   retval;

    g_return_val_if_fail (command_line != NULL, FALSE);

    if (!g_shell_parse_argv (command_line, NULL, &argv, error)) {
        return FALSE;
    }

    envp = spawn_make_environment_for_display (display, NULL);

    retval = g_spawn_sync (NULL,
                           argv,
                           envp,
                           G_SPAWN_SEARCH_PATH,
                           NULL,
                           NULL,
                           standard_output,
                           standard_error,
                           exit_status,
                           error);

    g_strfreev (argv);
    g_strfreev (envp);

    return retval;
}

static GdkVisual *
get_best_visual_for_display (GdkDisplay *display) {
    GdkScreen     *screen;
    char          *command;
    char          *std_output;
    int            exit_status;
    GError        *error;
    unsigned long  v;
    char           c;
    GdkVisual     *visual;
    gboolean       res;

    visual = NULL;
    screen = gdk_display_get_default_screen (display);

    command = g_build_filename (LIBEXECDIR, "xfce4-screensaver-gl-helper", NULL);

    error = NULL;
    std_output = NULL;
    res = spawn_command_line_on_display_sync (display,
                                              command,
                                              &std_output,
                                              NULL,
                                              &exit_status,
                                              &error);
    if (!res) {
        gs_debug ("Could not run command '%s': %s", command, error->message);
        g_error_free (error);
        goto out;
    }

    if (1 == sscanf (std_output, "0x%lx %c", &v, &c)) {
        if (v != 0) {
            VisualID      visual_id;

            visual_id = (VisualID) v;
            visual = gdk_x11_screen_lookup_visual (screen, visual_id);

            gs_debug ("Found best GL visual for display %s: 0x%x",
                      gdk_display_get_name (display),
                      (unsigned int) visual_id);
        }
    }
out:
    g_free (std_output);
    g_free (command);

    return g_object_ref (visual);
}

void
widget_set_best_visual (GtkWidget *widget) {
    GdkVisual *visual;

    g_return_if_fail (widget != NULL);

    visual = get_best_visual_for_display (gtk_widget_get_display (widget));
    if (visual != NULL) {
        gtk_widget_set_visual (widget, visual);
        g_object_unref (visual);
    }
}
