/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *          Rodrigo Moya <rodrigo@novell.com>
 *
 */

#include <config.h>

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* For uid_t, gid_t */
#include <unistd.h>

#include <gio/gio.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include "gs-debug.h"
#include "gs-job.h"
#include "gs-prefs.h" /* for GS_MODE enum */
#include "gs-theme-manager.h"
#include "xfce-desktop-utils.h"
#include "xfce4-screensaver-preferences-ui.h"

#define GPM_COMMAND "xfce4-power-manager-settings"
#define CONFIGURE_COMMAND "xfce4-screensaver-configure"

enum {
    NAME_COLUMN = 0,
    ID_COLUMN,
    N_COLUMNS
};

/* Drag and drop info */
enum {
    TARGET_URI_LIST,
    TARGET_NS_URL
};

static GtkBuilder     *builder = NULL;
static GSThemeManager *theme_manager = NULL;
static GSJob          *job = NULL;
static XfconfChannel  *screensaver_channel = NULL;
static XfconfChannel  *xfpm_channel = NULL;

static gboolean        idle_delay_writable;
static gboolean        lock_delay_writable;
static gboolean        keyboard_command_writable;
static gboolean        logout_command_writable;
static gboolean        logout_delay_writable;
static gchar          *active_theme = NULL;

static gint opt_socket_id = 0;
static GOptionEntry entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id,
        N_("Settings manager socket"), N_("SOCKET ID") },
    { NULL }
};

static gint32
config_get_idle_delay (gboolean *is_writable) {
    gint32 delay;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                       KEY_IDLE_DELAY);
    }

    delay = xfconf_channel_get_int (screensaver_channel, KEY_IDLE_DELAY, DEFAULT_KEY_IDLE_DELAY);

    if (delay < 1) {
        delay = 1;
    }

    return delay;
}

static void
config_set_idle_delay (gint32 timeout) {
    xfconf_channel_set_int (screensaver_channel, KEY_IDLE_DELAY, timeout);
}

static gint32
config_get_lock_delay (gboolean *is_writable) {
    gint32 delay;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                       KEY_LOCK_WITH_SAVER_DELAY);
    }

    delay = xfconf_channel_get_int (screensaver_channel, KEY_LOCK_WITH_SAVER_DELAY, DEFAULT_KEY_LOCK_WITH_SAVER_DELAY);

    if (delay < 0) {
        delay = 0;
    }

    return delay;
}

static void
config_set_lock_delay (gint32 timeout) {
    xfconf_channel_set_int (screensaver_channel, KEY_LOCK_WITH_SAVER_DELAY, timeout);
}

static gint32
config_get_cycle_delay (gboolean *is_writable) {
    gint32 delay;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                       KEY_CYCLE_DELAY);
    }

    delay = xfconf_channel_get_int (screensaver_channel, KEY_CYCLE_DELAY, DEFAULT_KEY_CYCLE_DELAY);

    if (delay < 1) {
        delay = 1;
    }

    return delay;
}

static void
config_set_cycle_delay (gint32 timeout) {
    xfconf_channel_set_int (screensaver_channel, KEY_CYCLE_DELAY, timeout);
}

static gint32
config_get_logout_delay (gboolean *is_writable) {
    gint32 delay;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                       KEY_LOGOUT_DELAY);
    }

    delay = xfconf_channel_get_int (screensaver_channel, KEY_LOGOUT_DELAY, DEFAULT_KEY_LOGOUT_DELAY);

    if (delay < 1) {
        delay = 1;
    }

    return delay;
}

static void
config_set_logout_delay (gint32 timeout) {
    xfconf_channel_set_int (screensaver_channel, KEY_LOGOUT_DELAY, timeout);
}

static int
config_get_mode (gboolean *is_writable) {
    int mode;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                       KEY_MODE);
    }

    mode = xfconf_channel_get_int (screensaver_channel, KEY_MODE, DEFAULT_KEY_MODE);

    return mode;
}

static void
config_set_mode (int mode) {
    xfconf_channel_set_int (screensaver_channel, KEY_MODE, mode);

    gtk_widget_set_visible (GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_cycle_delay_label_left")),
                            mode == GS_MODE_RANDOM);
    gtk_widget_set_visible (GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_cycle_delay")),
                            mode == GS_MODE_RANDOM);
    gtk_widget_set_visible (GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_cycle_delay_label_right")),
                            mode == GS_MODE_RANDOM);
}

static char *
config_get_theme (gboolean *is_writable) {
    char *name;
    int   mode;

    if (is_writable) {
        gboolean can_write_theme = TRUE;
        gboolean can_write_mode = TRUE;

        can_write_theme = !xfconf_channel_is_property_locked (screensaver_channel,
                                                              KEY_THEMES);
        can_write_mode = !xfconf_channel_is_property_locked (screensaver_channel,
                                                             KEY_MODE);
        *is_writable = can_write_theme && can_write_mode;
    }

    mode = config_get_mode (NULL);

    name = NULL;
    if (mode == GS_MODE_BLANK_ONLY) {
        name = g_strdup ("__blank-only");
    } else if (mode == GS_MODE_RANDOM) {
        name = g_strdup ("__random");
    } else {
        gchar **strv;
        strv = xfconf_channel_get_string_list (screensaver_channel,
                                               KEY_THEMES);
        if (strv != NULL) {
            name = g_strdup (strv[0]);
        } else {
            /* TODO: handle error */
            /* default to blank */
            name = g_strdup ("__blank-only");
        }

        g_strfreev (strv);
    }

    return name;
}

static gchar **
get_all_theme_ids (GSThemeManager *local_theme_manager) {
    gchar  **ids = NULL;
    GSList  *themes;
    GSList  *l;
    guint    idx = 0;

    themes = gs_theme_manager_get_info_list (local_theme_manager);
    ids = g_new0 (gchar *, g_slist_length (themes) + 1);
    for (l = themes; l; l = l->next) {
        GSThemeInfo *info = l->data;

        ids[idx++] = g_strdup (gs_theme_info_get_id (info));
        gs_theme_info_unref (info);
    }
    g_slist_free (themes);

    return ids;
}

static void
config_set_theme (const char *theme_id) {
    gchar **strv = NULL;
    int     mode;

    if (theme_id && strcmp (theme_id, "__blank-only") == 0) {
        mode = GS_MODE_BLANK_ONLY;
        active_theme = g_strdup ("xfce-blank");
    } else if (theme_id && strcmp (theme_id, "__random") == 0) {
        mode = GS_MODE_RANDOM;

        /* set the themes key to contain all available screensavers */
        strv = get_all_theme_ids (theme_manager);
    } else {
        mode = GS_MODE_SINGLE;
        strv = g_strsplit (theme_id, "%%%", 1);
        active_theme = g_strdup (theme_id);
    }

    if (mode != GS_MODE_RANDOM) {
        GtkWidget *configure_button = GTK_WIDGET (gtk_builder_get_object (builder, "configure_button"));
        gtk_widget_set_sensitive (configure_button, TRUE);
    }

    config_set_mode (mode);

    if (strv) {
        xfconf_channel_set_string_list (screensaver_channel,
                                        KEY_THEMES,
                                        (const gchar * const*) strv);
        g_strfreev (strv);
    } else {
        xfconf_channel_reset_property (screensaver_channel,
                                       KEY_THEMES,
                                       FALSE);
    }
}

static gboolean
config_get_idle_activation_enabled (gboolean *is_writable) {
    int enabled;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_IDLE_ACTIVATION_ENABLED);
    }

    enabled = xfconf_channel_get_bool (screensaver_channel,
                                       KEY_IDLE_ACTIVATION_ENABLED,
                                       DEFAULT_KEY_IDLE_ACTIVATION_ENABLED);

    return enabled;
}

static void
config_set_idle_activation_enabled (gboolean enabled) {
    xfconf_channel_set_bool (screensaver_channel,
                             KEY_IDLE_ACTIVATION_ENABLED,
                             enabled);
}

static gboolean
config_get_saver_enabled (gboolean *is_writable) {
    gboolean lock;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_SAVER_ENABLED);
    }

    lock = xfconf_channel_get_bool (screensaver_channel,
                                    KEY_SAVER_ENABLED,
                                    DEFAULT_KEY_SAVER_ENABLED);

    return lock;
}

static void
config_set_saver_enabled (gboolean lock) {
    xfconf_channel_set_bool (screensaver_channel, KEY_SAVER_ENABLED, lock);
}

static gboolean
config_get_lock_on_suspend_hibernate_enabled (gboolean *is_writable) {
    gboolean lock;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (xfpm_channel,
                                                           KEY_LOCK_ON_SLEEP);
    }

    lock = xfconf_channel_get_bool (xfpm_channel,
                                    KEY_LOCK_ON_SLEEP,
                                    DEFAULT_KEY_LOCK_ON_SLEEP);

    return lock;
}

static void
config_set_lock_on_suspend_hibernate_enabled (gboolean lock) {
    xfconf_channel_set_bool (xfpm_channel, KEY_LOCK_ON_SLEEP, lock);
}

static gboolean
config_get_lock_enabled (gboolean *is_writable) {
    gboolean lock;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_LOCK_ENABLED);
    }

    lock = xfconf_channel_get_bool (screensaver_channel,
                                    KEY_LOCK_ENABLED,
                                    DEFAULT_KEY_LOCK_ENABLED);

    return lock;
}

static void
config_set_lock_enabled (gboolean lock) {
    xfconf_channel_set_bool (screensaver_channel, KEY_LOCK_ENABLED, lock);
}

static gboolean
config_get_lock_with_saver_enabled (gboolean *is_writable) {
    gboolean lock;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_LOCK_WITH_SAVER_ENABLED);
    }

    lock = xfconf_channel_get_bool (screensaver_channel,
                                    KEY_LOCK_WITH_SAVER_ENABLED,
                                    DEFAULT_KEY_LOCK_WITH_SAVER_ENABLED);

    return lock;
}

static void
config_set_lock_with_saver_enabled (gboolean lock) {
    xfconf_channel_set_bool (screensaver_channel, KEY_LOCK_WITH_SAVER_ENABLED, lock);
}

static gboolean
config_get_fullscreen_inhibit (gboolean *is_writable) {
    gboolean inhibit;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_FULLSCREEN_INHIBIT);
    }

    inhibit = xfconf_channel_get_bool (screensaver_channel,
                                       KEY_FULLSCREEN_INHIBIT,
                                       DEFAULT_KEY_FULLSCREEN_INHIBIT);

    return inhibit;
}

static void
config_set_fullscreen_inhibit (gboolean inhibit) {
    xfconf_channel_set_bool (screensaver_channel, KEY_FULLSCREEN_INHIBIT, inhibit);
}

static gboolean
config_get_keyboard_enabled (gboolean *is_writable) {
    gboolean enabled;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_KEYBOARD_ENABLED);
    }

    enabled = xfconf_channel_get_bool (screensaver_channel,
                                       KEY_KEYBOARD_ENABLED,
                                       DEFAULT_KEY_KEYBOARD_ENABLED);

    return enabled;
}

static void
config_set_keyboard_enabled (gboolean enabled) {
    xfconf_channel_set_bool (screensaver_channel, KEY_KEYBOARD_ENABLED, enabled);
}

static gboolean
config_get_status_message_enabled (gboolean *is_writable) {
    gboolean enabled;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_STATUS_MESSAGE_ENABLED);
    }

    enabled = xfconf_channel_get_bool (screensaver_channel,
                                       KEY_STATUS_MESSAGE_ENABLED,
                                       DEFAULT_KEY_STATUS_MESSAGE_ENABLED);

    return enabled;
}

static void
config_set_status_message_enabled (gboolean enabled) {
    xfconf_channel_set_bool (screensaver_channel, KEY_STATUS_MESSAGE_ENABLED, enabled);
}

static gboolean
config_get_logout_enabled (gboolean *is_writable) {
    gboolean enabled;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_LOGOUT_ENABLED);
    }

    enabled = xfconf_channel_get_bool (screensaver_channel,
                                       KEY_LOGOUT_ENABLED,
                                       DEFAULT_KEY_LOGOUT_ENABLED);

    return enabled;
}

static void
config_set_logout_enabled (gboolean enabled) {
    xfconf_channel_set_bool (screensaver_channel, KEY_LOGOUT_ENABLED, enabled);
}

static gboolean
config_get_user_switch_enabled (gboolean *is_writable) {
    gboolean enabled;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked (screensaver_channel,
                                                           KEY_USER_SWITCH_ENABLED);
    }

    enabled = xfconf_channel_get_bool (screensaver_channel,
                                       KEY_USER_SWITCH_ENABLED,
                                       DEFAULT_KEY_USER_SWITCH_ENABLED);

    return enabled;
}

static void
config_set_user_switch_enabled (gboolean enabled) {
    xfconf_channel_set_bool (screensaver_channel, KEY_USER_SWITCH_ENABLED, enabled);
}

static gchar*
config_get_keyboard_command (gboolean *is_writable) {
    gchar *command;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked(screensaver_channel,
                                                          KEY_KEYBOARD_COMMAND);
    }

    command = xfconf_channel_get_string (screensaver_channel,
                                         KEY_KEYBOARD_COMMAND,
                                         DEFAULT_KEY_KEYBOARD_COMMAND);

    return command;
}

static void
config_set_keyboard_command (const gchar *command) {
    xfconf_channel_set_string (screensaver_channel, KEY_KEYBOARD_COMMAND, command);
}

static gchar*
config_get_logout_command (gboolean *is_writable) {
    gchar *command;

    if (is_writable) {
        *is_writable = !xfconf_channel_is_property_locked(screensaver_channel,
                                                          KEY_LOGOUT_COMMAND);
    }

    command = xfconf_channel_get_string (screensaver_channel,
                                         KEY_LOGOUT_COMMAND,
                                         DEFAULT_KEY_LOGOUT_COMMAND);

    return command;
}

static void
config_set_logout_command (const gchar *command) {
    xfconf_channel_set_string (screensaver_channel, KEY_LOGOUT_COMMAND, command);
}

static gchar *
config_get_theme_arguments (const gchar *theme) {
    gchar *property;
    gchar *arguments;
    gchar *theme_name;

    theme_name = g_utf8_substring (theme, 13, g_utf8_strlen(theme, -1));
    property = g_strdup_printf ("/screensavers/%s/arguments", theme_name);
    arguments = xfconf_channel_get_string (screensaver_channel, property, "");

    g_free(theme_name);
    g_free(property);

    return arguments;
}

static void
job_set_theme (GSJob      *local_job,
               const char *theme) {
    GSThemeInfo *info;
    gchar       *command = NULL;
    gchar       *arguments = NULL;

    command = NULL;

    info = gs_theme_manager_lookup_theme_info (theme_manager, theme);
    if (info != NULL) {
        arguments = config_get_theme_arguments (theme);
        command = g_strdup_printf ("%s %s", gs_theme_info_get_exec (info), arguments);
    }

    gs_job_set_command (local_job, command);

    if (arguments)
        g_free (arguments);
    if (command)
        g_free (command);

    if (info != NULL) {
        gs_theme_info_unref (info);
    }
}

static gboolean
preview_on_draw (GtkWidget *widget,
                 cairo_t   *cr,
                 gpointer   data) {
    if (job == NULL || !gs_job_is_running (job)) {
        cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_paint (cr);
    }

    return FALSE;
}

static void
preview_set_theme (GtkWidget  *widget,
                   const char *theme,
                   const char *name) {
    GtkWidget *label;
    char      *markup;

    if (job != NULL) {
        gs_job_stop (job);
    }

    gtk_widget_queue_draw (widget);

    label = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_theme_label"));
    markup = g_markup_printf_escaped ("<i>%s</i>", name);
    gtk_label_set_markup (GTK_LABEL (label), markup);
    g_free (markup);

    if ((theme && strcmp (theme, "__blank-only") == 0)) {
        /* Do nothing */
    } else if (theme && strcmp (theme, "__random") == 0) {
        gchar **themes;

        themes = get_all_theme_ids (theme_manager);
        if (themes != NULL) {
            gint32  i;

            i = g_random_int_range (0, g_strv_length (themes));
                        job_set_theme (job, themes[i]);
                        g_strfreev (themes);

            gs_job_start (job);
        }
    } else {
        job_set_theme (job, theme);
        gs_job_start (job);
    }
}

static void
help_display (void) {
    GError *error;

    error = NULL;
    gtk_show_uri_on_window (NULL,
                            "https://docs.xfce.org/apps/screensaver/start",
                            GDK_CURRENT_TIME,
                            &error);

    if (error != NULL) {
        GtkWidget *d;

        d = gtk_message_dialog_new (NULL,
                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                    "%s", error->message);
        gtk_dialog_run (GTK_DIALOG (d));
        gtk_widget_destroy (d);
        g_error_free (error);
    }
}

static void
response_cb (GtkWidget *widget,
             int        response_id) {
    if (response_id == GTK_RESPONSE_HELP) {
        help_display ();
    } else if (response_id == GTK_RESPONSE_REJECT) {
        GError  *error;
        gboolean res;

        error = NULL;

        res = xfce_gdk_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                     GPM_COMMAND,
                                                     &error);
        if (!res) {
            g_warning ("Unable to start power management preferences: %s", error->message);
            g_error_free (error);
        }
    } else {
        gtk_widget_destroy (widget);
        gtk_main_quit ();
    }
}

static GSList *
get_theme_info_list (void) {
    return gs_theme_manager_get_info_list (theme_manager);
}

static void
populate_model (GtkTreeStore *store) {
    GtkTreeIter  iter;
    GSList      *themes = NULL;
    GSList      *l;

    gtk_tree_store_append (store, &iter, NULL);
    gtk_tree_store_set (store, &iter,
                        NAME_COLUMN, _("Blank screen"),
                        ID_COLUMN, "__blank-only",
                        -1);

    gtk_tree_store_append (store, &iter, NULL);
    gtk_tree_store_set (store, &iter,
                        NAME_COLUMN, _("Random"),
                        ID_COLUMN, "__random",
                        -1);

    gtk_tree_store_append (store, &iter, NULL);
    gtk_tree_store_set (store, &iter,
                        NAME_COLUMN, NULL,
                        ID_COLUMN, "__separator",
                        -1);

    themes = get_theme_info_list ();

    if (themes == NULL) {
        return;
    }

    for (l = themes; l; l = l->next) {
        const char  *name;
        const char  *id;
        GSThemeInfo *info = l->data;

        if (info == NULL) {
            continue;
        }

        name = gs_theme_info_get_name (info);
        id = gs_theme_info_get_id (info);

        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter,
                            NAME_COLUMN, name,
                            ID_COLUMN, id,
                            -1);

        gs_theme_info_unref (info);
    }

    g_slist_free (themes);
}

static void
tree_selection_previous (GtkTreeSelection *selection) {
    GtkTreeIter   iter;
    GtkTreeModel *model;
    GtkTreePath  *path;

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
        return;
    }

    path = gtk_tree_model_get_path (model, &iter);
    if (gtk_tree_path_prev (path)) {
        gtk_tree_selection_select_path (selection, path);
    }
}

static void
tree_selection_next (GtkTreeSelection *selection) {
    GtkTreeIter   iter;
    GtkTreeModel *model;
    GtkTreePath  *path;

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
        return;
    }

    path = gtk_tree_model_get_path (model, &iter);
    gtk_tree_path_next (path);
    gtk_tree_selection_select_path (selection, path);
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection,
                           GtkWidget        *preview) {
    GtkWidget    *configure_button;
    GtkTreeIter   iter;
    GtkTreeModel *model;
    char         *theme;
    char         *name;

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
        return;
    }

    if (active_theme != NULL) {
        g_free (active_theme);
        active_theme = NULL;

        configure_button = GTK_WIDGET (gtk_builder_get_object (builder, "configure_button"));
        gtk_widget_set_sensitive (configure_button, FALSE);
    }

    gtk_tree_model_get (model, &iter, ID_COLUMN, &theme, NAME_COLUMN, &name, -1);

    if (theme == NULL) {
        g_free (name);
        return;
    }

    preview_set_theme (preview, theme, name);
    config_set_theme (theme);

    g_free (theme);
    g_free (name);
}

static void
configure_button_clicked_cb (GtkToolButton *button,
                             gpointer       user_data) {
    gchar    *configure_cmd;
    GError   *error = NULL;
    gboolean  res;

    configure_cmd = g_strdup_printf ("%s %s", CONFIGURE_COMMAND, active_theme);

    res = xfce_gdk_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                 configure_cmd,
                                                 &error);
    if (!res) {
        g_warning ("Unable to start configure command: %s", error->message);
        g_error_free (error);
    }

    g_free(configure_cmd);
}

static void
idle_delay_value_changed_cb (GtkSpinButton *spin,
                                 gpointer  user_data) {
    gdouble value;

    value = gtk_spin_button_get_value (spin);
    config_set_idle_delay ((gint32)value);
}

static void
lock_delay_value_changed_cb (GtkSpinButton *spin,
                                 gpointer  user_data) {
    gdouble value;

    value = gtk_spin_button_get_value (spin);
    config_set_lock_delay ((gint32)value);
}

static void
cycle_delay_value_changed_cb (GtkSpinButton *spin,
                                 gpointer  user_data) {
    gdouble value;

    value = gtk_spin_button_get_value (spin);
    config_set_cycle_delay ((gint32)value);
}

static void
logout_delay_value_changed_cb (GtkSpinButton *spin,
                                 gpointer  user_data) {
    gdouble value;

    value = gtk_spin_button_get_value (spin);
    config_set_logout_delay ((gint32)value);
}

static void
keyboard_command_changed_cb (GtkEntry *widget,
                             gpointer  user_data)
{
    const gchar *value;

    value = gtk_entry_get_text (widget);
    config_set_keyboard_command (value);
}

static void
logout_command_changed_cb (GtkEntry *widget,
                           gpointer  user_data)
{
    const gchar *value;

    value = gtk_entry_get_text (widget);
    config_set_logout_command (value);
}

static int
compare_theme_names (char *name_a,
                     char *name_b,
                     char *id_a,
                     char *id_b) {
    if (id_a == NULL) {
        return 1;
    } else if (id_b == NULL) {
        return -1;
    }

    if (strcmp (id_a, "__blank-only") == 0) {
        return -1;
    } else if (strcmp (id_b, "__blank-only") == 0) {
        return 1;
    } else if (strcmp (id_a, "__random") == 0) {
        return -1;
    } else if (strcmp (id_b, "__random") == 0) {
        return 1;
    } else if (strcmp (id_a, "__separator") == 0) {
        return -1;
    } else if (strcmp (id_b, "__separator") == 0) {
        return 1;
    }

    if (name_a == NULL) {
        return 1;
    } else if (name_b == NULL) {
        return -1;
    }

    return g_utf8_collate (name_a, name_b);
}

static int
compare_theme  (GtkTreeModel *model,
                GtkTreeIter  *a,
                GtkTreeIter  *b,
                gpointer      user_data) {
    char *name_a;
    char *name_b;
    char *id_a;
    char *id_b;
    int   result;

    gtk_tree_model_get (model, a, NAME_COLUMN, &name_a, -1);
    gtk_tree_model_get (model, b, NAME_COLUMN, &name_b, -1);
    gtk_tree_model_get (model, a, ID_COLUMN, &id_a, -1);
    gtk_tree_model_get (model, b, ID_COLUMN, &id_b, -1);

    result = compare_theme_names (name_a, name_b, id_a, id_b);

    g_free (name_a);
    g_free (name_b);
    g_free (id_a);
    g_free (id_b);

    return result;
}

static gboolean
separator_func (GtkTreeModel *model,
                GtkTreeIter  *iter,
                gpointer      data) {
    int   column = GPOINTER_TO_INT (data);
    char *text;

    gtk_tree_model_get (model, iter, column, &text, -1);

    if (text != NULL && strcmp (text, "__separator") == 0) {
        return TRUE;
    }

    g_free (text);

    return FALSE;
}

static void
setup_treeview (GtkWidget *tree,
                GtkWidget *preview) {
    GtkTreeStore      *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer   *renderer;
    GtkTreeSelection  *select;

    store = gtk_tree_store_new (N_COLUMNS,
                                G_TYPE_STRING,
                                G_TYPE_STRING);
    populate_model (store);

    gtk_tree_view_set_model (GTK_TREE_VIEW (tree),
                             GTK_TREE_MODEL (store));

    g_object_unref (store);

    g_object_set (tree, "show-expanders", FALSE, NULL);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
             "text", NAME_COLUMN,
             NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

    gtk_tree_view_column_set_sort_column_id (column, NAME_COLUMN);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                     NAME_COLUMN,
                                     compare_theme,
                                     NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                          NAME_COLUMN,
                                          GTK_SORT_ASCENDING);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (tree),
                                          separator_func,
                                          GINT_TO_POINTER (ID_COLUMN),
                                          NULL);

    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (select), "changed",
                      G_CALLBACK (tree_selection_changed_cb),
                      preview);
}

static void
reload_theme (GtkWidget *treeview) {
    GtkWidget        *preview;
    GtkTreeIter       iter;
    GtkTreeModel     *model;
    GtkTreeSelection *selection;
    char             *theme;
    char             *name;

    if (active_theme == NULL) {
        return;
    }

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    if (model == NULL) {
        return;
    }

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
        return;
    }

    gtk_tree_model_get (model, &iter, ID_COLUMN, &theme, NAME_COLUMN, &name, -1);

    if (theme == NULL) {
        g_free (name);
        return;
    }

    preview  = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_preview_area"));
    preview_set_theme (preview, theme, name);

    g_free (theme);
    g_free (name);
}

static void
setup_treeview_selection (GtkWidget *tree) {
    char         *theme;
    GtkTreeModel *model;
    GtkTreeIter   iter;
    GtkTreePath  *path = NULL;
    gboolean      is_writable;

    theme = config_get_theme (&is_writable);

    if (!is_writable) {
        gtk_widget_set_sensitive (tree, FALSE);
    }

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));

    if (theme && gtk_tree_model_get_iter_first (model, &iter)) {
        do {
            char *id;
            gboolean found;

            gtk_tree_model_get (model, &iter,
                                ID_COLUMN, &id, -1);
            found = (id && strcmp (id, theme) == 0);
            g_free (id);

            if (found) {
                path = gtk_tree_model_get_path (model, &iter);
                break;
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }

    if (path) {
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree),
                                  path,
                                  NULL,
                                  FALSE);

        gtk_tree_path_free (path);
    }

    g_free (theme);
}

static void
saver_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    gboolean writable;

    config_set_saver_enabled (gtk_switch_get_active (widget));

    writable = gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "saver_grid")), writable);
}

static void
lock_on_suspend_hibernate_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    config_set_lock_on_suspend_hibernate_enabled (gtk_switch_get_active (widget));
}

static void
lock_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    gboolean writable;

    config_set_lock_enabled (gtk_switch_get_active (widget));

    writable = gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_grid")), writable);
}

static void
lock_with_saver_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    gboolean writable;

    config_set_lock_with_saver_enabled (gtk_switch_get_active (widget));

    writable = lock_delay_writable && gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay_label_left")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay_label_right")),
        writable);
}

static void
fullscreen_inhibit_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    config_set_fullscreen_inhibit (gtk_switch_get_active (widget));
}

static void
idle_activation_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    gboolean writable;

    config_set_idle_activation_enabled (gtk_switch_get_active (widget));

    writable = idle_delay_writable && gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay_label_left")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay_label_right")),
        writable);
}

static void
logout_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    gboolean writable;

    config_set_logout_enabled (gtk_switch_get_active (widget));

    writable = logout_command_writable && gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_command_label")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_command")),
        writable);

    writable = logout_delay_writable && gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay_label_left")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay_label_right")),
        writable);
}

static void
status_message_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    config_set_status_message_enabled (gtk_switch_get_active (widget));
}

static void
user_switch_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    config_set_user_switch_enabled (gtk_switch_get_active (widget));
}

static void
keyboard_toggled_cb (GtkSwitch *widget, gpointer user_data) {
    gboolean writable;

    config_set_keyboard_enabled (gtk_switch_get_active (widget));

    writable = keyboard_command_writable && gtk_switch_get_active (widget);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_command_label")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_command")),
        writable);
}

static void
ui_set_saver_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;
    gboolean   writable;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }

    writable = enabled;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "saver_grid")), writable);
}

static void
ui_set_lock_on_suspend_hibernate_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_suspend_hibernate_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }
}

static void
ui_set_lock_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;
    gboolean   writable;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }

    writable = enabled;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_grid")), writable);
}

static void
ui_set_lock_with_saver_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;
    gboolean   writable;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }

    writable = lock_delay_writable && enabled;
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay_label_left")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay_label_right")),
        writable);
}

static void
ui_set_fullscreen_inhibit_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_fullscreen_inhibit_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }
}

static void
ui_set_idle_activation_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;
    gboolean   writable;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_enabled"));
    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }

    writable = idle_delay_writable && enabled;
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay_label_left")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay_label_right")),
        writable);
}

static void
ui_set_keyboard_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;
    gboolean   writable;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }

    writable = keyboard_command_writable && enabled;
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_command_label")),
        writable);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_command")),
        writable);
}

static void
ui_set_logout_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;
    gboolean   writable;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }

    writable = logout_command_writable && enabled;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_command_label")), writable);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_command")), writable);

    writable = logout_delay_writable && enabled;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay_label_left")), writable);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay")), writable);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay_label_right")), writable);
}

static void
ui_set_idle_delay (int delay) {
    GtkWidget *widget;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), delay);
}

static void
ui_set_lock_delay (int delay) {
    GtkWidget *widget;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), delay);
}

static void
ui_set_cycle_delay (int delay) {
    GtkWidget *widget;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_cycle_delay"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), delay);
}

static void
ui_set_keyboard_command (gchar *command) {
    GtkWidget   *widget;
    const gchar *current;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_command"));

    current = gtk_entry_get_text (GTK_ENTRY (widget));
    if (g_strcmp0 (current, command) != 0) {
        gtk_entry_set_text (GTK_ENTRY (widget), command);
    }
}

static void
ui_set_status_message_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_status_messages_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }
}

static void
ui_set_logout_delay (int delay) {
    GtkWidget *widget;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), delay);
}

static void
ui_set_logout_command (gchar *command) {
    GtkWidget   *widget;
    const gchar *current;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_command"));

    current = gtk_entry_get_text (GTK_ENTRY (widget));
    if (g_strcmp0 (current, command) != 0) {
        gtk_entry_set_text (GTK_ENTRY (widget), command);
    }
}

static void
ui_set_user_switch_enabled (gboolean enabled) {
    GtkWidget *widget;
    gboolean   active;

    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_user_switching_enabled"));

    active = gtk_switch_get_active (GTK_SWITCH (widget));
    if (active != enabled) {
        gtk_switch_set_active (GTK_SWITCH (widget), enabled);
    }
}

static void
key_changed_cb (XfconfChannel *channel, const gchar *key, gpointer data) {
    if (strcmp (key, KEY_IDLE_DELAY) == 0) {
        int delay;
        delay = xfconf_channel_get_int (channel, key, DEFAULT_KEY_IDLE_DELAY);
        ui_set_idle_delay (delay);
    } else if (strcmp (key, KEY_LOCK_WITH_SAVER_DELAY) == 0) {
        int delay;
        delay = xfconf_channel_get_int (channel, key, DEFAULT_KEY_LOCK_WITH_SAVER_DELAY);
        ui_set_lock_delay (delay);
    } else if (strcmp (key, KEY_IDLE_ACTIVATION_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_IDLE_ACTIVATION_ENABLED);
        ui_set_idle_activation_enabled (enabled);
    } else if (strcmp (key, KEY_SAVER_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_SAVER_ENABLED);
        ui_set_saver_enabled (enabled);
    } else if (strcmp (key, KEY_LOCK_ON_SLEEP) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_LOCK_ON_SLEEP);
        ui_set_lock_on_suspend_hibernate_enabled (enabled);
    } else if (strcmp (key, KEY_LOCK_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_LOCK_ENABLED);
        ui_set_lock_enabled (enabled);
    } else if (strcmp (key, KEY_LOCK_WITH_SAVER_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_LOCK_WITH_SAVER_ENABLED);
        ui_set_lock_with_saver_enabled (enabled);
    } else if (strcmp (key, KEY_FULLSCREEN_INHIBIT) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_FULLSCREEN_INHIBIT);
        ui_set_fullscreen_inhibit_enabled (enabled);
    } else if (strcmp (key, KEY_CYCLE_DELAY) == 0) {
        int delay;
        delay = xfconf_channel_get_int (channel, key, DEFAULT_KEY_CYCLE_DELAY);
        ui_set_cycle_delay (delay);
    } else if (strcmp (key, KEY_KEYBOARD_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_KEYBOARD_ENABLED);
        ui_set_keyboard_enabled (enabled);
    } else if (strcmp (key, KEY_KEYBOARD_COMMAND) == 0) {
        gchar *cmd;
        cmd = xfconf_channel_get_string (channel, key, DEFAULT_KEY_KEYBOARD_COMMAND);
        ui_set_keyboard_command (cmd);
    } else if (strcmp (key, KEY_STATUS_MESSAGE_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_STATUS_MESSAGE_ENABLED);
        ui_set_status_message_enabled (enabled);
    } else if (strcmp (key, KEY_LOGOUT_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_LOGOUT_ENABLED);
        ui_set_logout_enabled (enabled);
    } else if (strcmp (key, KEY_LOGOUT_DELAY) == 0) {
        int delay;
        delay = xfconf_channel_get_int (channel, key, DEFAULT_KEY_LOGOUT_DELAY);
        ui_set_logout_delay (delay);
    } else if (strcmp (key, KEY_LOGOUT_COMMAND) == 0) {
        gchar *cmd;
        cmd = xfconf_channel_get_string (channel, key, DEFAULT_KEY_LOGOUT_COMMAND);
        ui_set_logout_command (cmd);
    } else if (strcmp (key, KEY_USER_SWITCH_ENABLED) == 0) {
        gboolean enabled;
        enabled = xfconf_channel_get_bool (channel, key, DEFAULT_KEY_USER_SWITCH_ENABLED);
        ui_set_user_switch_enabled (enabled);
    } else if (strcmp (key, KEY_THEMES) == 0) {
        GtkWidget *treeview;
        treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_treeview"));
        setup_treeview_selection (treeview);
    } else if (g_str_has_suffix (key, "arguments")) {
        GtkWidget *treeview;
        treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_treeview"));
        reload_theme (treeview);
    }
}

static void
fullscreen_preview_previous_cb (GtkWidget *fullscreen_preview_window,
                                gpointer   user_data) {
    GtkWidget        *treeview;
    GtkTreeSelection *selection;

    treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_treeview"));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    tree_selection_previous (selection);
}

static void
fullscreen_preview_next_cb (GtkWidget *fullscreen_preview_window,
                            gpointer   user_data) {
    GtkWidget        *treeview;
    GtkTreeSelection *selection;

    treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_treeview"));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    tree_selection_next (selection);
}

static void
fullscreen_preview_cancelled_cb (GtkWidget *button,
                                 gpointer   user_data) {
    GtkWidget *fullscreen_preview_area;
    GtkWidget *fullscreen_preview_window;
    GtkWidget *preview_area;
    GtkWidget *dialog;

    preview_area = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_preview_area"));
    gs_job_set_widget (job, preview_area);

    fullscreen_preview_area = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_area"));
    gtk_widget_queue_draw (fullscreen_preview_area);

    fullscreen_preview_window = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_window"));
    gtk_widget_hide (fullscreen_preview_window);

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "prefs_dialog"));
    gtk_widget_show (dialog);
    gtk_window_present (GTK_WINDOW (dialog));
}

static void
fullscreen_preview_start_cb (GtkWidget *widget,
                             gpointer   user_data) {
    GtkWidget *fullscreen_preview_area;
    GtkWidget *fullscreen_preview_window;
    GtkWidget *dialog;

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "prefs_dialog"));
    gtk_widget_hide (dialog);

    fullscreen_preview_window = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_window"));

    gtk_window_fullscreen (GTK_WINDOW (fullscreen_preview_window));
    gtk_window_set_keep_above (GTK_WINDOW (fullscreen_preview_window), TRUE);

    gtk_widget_show (fullscreen_preview_window);
    gtk_widget_grab_focus (fullscreen_preview_window);

    fullscreen_preview_area = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_area"));
    gtk_widget_queue_draw (fullscreen_preview_area);
    gs_job_set_widget (job, fullscreen_preview_area);
}

static void
constrain_list_size (GtkWidget      *widget,
                     GtkAllocation  *allocation,
                     GtkWidget      *to_size) {
    GtkRequisition req;
    int            max_height;

    /* constrain height to be the tree height up to a max */
    max_height = (HeightOfScreen (gdk_x11_screen_get_xscreen (gtk_widget_get_screen (widget)))) / 4;

    gtk_widget_get_preferred_size (to_size, &req, NULL);
    allocation->height = MIN (req.height, max_height);
}

static void
setup_list_size_constraint (GtkWidget *widget,
                            GtkWidget *to_size) {
    g_signal_connect (widget, "size-allocate",
                      G_CALLBACK (constrain_list_size), to_size);
}

static gboolean
check_is_root_user (void) {
#ifndef G_OS_WIN32
    uid_t ruid, euid, suid; /* Real, effective and saved user ID's */
    gid_t rgid, egid, sgid; /* Real, effective and saved group ID's */

#ifdef HAVE_GETRESUID
    if (getresuid (&ruid, &euid, &suid) != 0 ||
            getresgid (&rgid, &egid, &sgid) != 0)
#endif /* HAVE_GETRESUID */
    {
        suid = ruid = getuid ();
        sgid = rgid = getgid ();
        euid = geteuid ();
        egid = getegid ();
    }

    if (ruid == 0) {
        return TRUE;
    }

#endif /* G_OS_WIN32 */
    return FALSE;
}

static void
setup_for_root_user (void) {
    GtkWidget *lock_checkbox;
    GtkWidget *infobar;

    lock_checkbox = GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_enabled"));
    infobar = GTK_WIDGET (gtk_builder_get_object (builder, "root_warning_infobar"));
    gtk_switch_set_active (GTK_SWITCH (lock_checkbox), FALSE);
    gtk_widget_set_sensitive (lock_checkbox, FALSE);

    gtk_widget_show (infobar);
}

static void
resolve_lid_switch_cb (GtkWidget *widget,
                       gpointer   user_data) {
    XfconfChannel *channel = xfconf_channel_get("xfce4-power-manager");
    const gchar   *property = "/xfce4-power-manager/logind-handle-lid-switch";
    gboolean       locked = xfconf_channel_is_property_locked(channel, property);
    GtkWidget     *infobar = GTK_WIDGET (gtk_builder_get_object (builder, "logind_lid_infobar"));

    if (!locked) {
        xfconf_channel_set_bool(channel, property, FALSE);
        gtk_widget_hide (GTK_WIDGET (infobar));
    }
}

static void
setup_for_lid_switch (void) {
    XfconfChannel *channel = xfconf_channel_get ("xfce4-power-manager");
    const gchar   *property = "/xfce4-power-manager/logind-handle-lid-switch";
    gboolean       handled = xfconf_channel_get_bool (channel, property, FALSE);
    gboolean       locked = xfconf_channel_is_property_locked (channel, property);
    GtkWidget     *infobar;
    GtkWidget     *button;

    infobar = GTK_WIDGET (gtk_builder_get_object (builder, "logind_lid_infobar"));
    button = GTK_WIDGET (gtk_builder_get_object (builder, "logind_lid_resolve"));

    if (handled) {
        gtk_widget_show (infobar);
        if (locked) {
            gtk_widget_hide (button);
        } else {
            gtk_widget_show (button);
            g_signal_connect (button, "clicked", G_CALLBACK (resolve_lid_switch_cb), NULL);
        }
    } else {
        gtk_widget_hide (infobar);
    }
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
            VisualID visual_id;

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

    return visual;
}


static void
widget_set_best_visual (GtkWidget *widget) {
    GdkVisual *visual;

    g_return_if_fail (widget != NULL);

    visual = get_best_visual_for_display (gtk_widget_get_display (widget));
    if (visual != NULL) {
        gtk_widget_set_visual (widget, visual);
        g_object_unref (visual);
    }
}

static gboolean
setup_treeview_idle (gpointer user_data) {
    GtkWidget *preview;
    GtkWidget *treeview;

    preview  = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_preview_area"));
    treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_treeview"));

    setup_treeview (treeview, preview);
    setup_treeview_selection (treeview);

    return FALSE;
}

static gboolean
is_program_in_path (const char *program) {
    char *tmp = g_find_program_in_path (program);
    if (tmp != NULL) {
        g_free (tmp);
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
set_widget_writable (GtkWidget *widget,
                     gboolean writable)
{
    gtk_widget_set_sensitive (widget, writable);
    if (!writable) {
        gtk_widget_set_tooltip_text (widget, _("Setting locked by administrator."));
    }
}

static void
configure_capplet (void) {
    GtkWidget *dialog;
    GtkWidget *preview;
    GtkWidget *treeview;
    GtkWidget *list_scroller;
    GtkWidget *root_warning_infobar;
    GtkWidget *preview_button;
    GtkWidget *gpm_button;
    GtkWidget *configure_toolbar;
    GtkWidget *configure_button;
    GtkWidget *fullscreen_preview_window;
    GtkWidget *fullscreen_preview_area;
    GtkWidget *fullscreen_preview_previous;
    GtkWidget *fullscreen_preview_next;
    GtkWidget *fullscreen_preview_close;

    GtkWidget *widget;
    gdouble    delay;
    gboolean   enabled;
    gboolean   is_writable;
    gchar     *command;

    GError    *error = NULL;
    gint       mode;

    builder = gtk_builder_new();
    if (!gtk_builder_add_from_string (builder, xfce4_screensaver_preferences_ui,
                                      xfce4_screensaver_preferences_ui_length, &error)) {
        g_warning ("Error loading UI: %s", error->message);
        g_error_free(error);
    }

    if (builder == NULL) {
        dialog = gtk_message_dialog_new (NULL,
                                         0, GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("Could not load the main interface"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  _("Please make sure that the screensaver is properly installed"));

        gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                         GTK_RESPONSE_OK);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        exit (1);
    }

    preview                     = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_preview_area"));
    dialog                      = GTK_WIDGET (gtk_builder_get_object (builder, "prefs_dialog"));
    treeview                    = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_treeview"));
    list_scroller               = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_scrolled_window"));
    root_warning_infobar        = GTK_WIDGET (gtk_builder_get_object (builder, "root_warning_infobar"));
    preview_button              = GTK_WIDGET (gtk_builder_get_object (builder, "preview_button"));
    gpm_button                  = GTK_WIDGET (gtk_builder_get_object (builder, "power_management_button"));
    configure_button            = GTK_WIDGET (gtk_builder_get_object (builder, "configure_button"));
    configure_toolbar           = GTK_WIDGET (gtk_builder_get_object (builder, "configure_toolbar"));
    fullscreen_preview_window   = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_window"));
    fullscreen_preview_area     = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_area"));
    fullscreen_preview_close    = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_close"));
    fullscreen_preview_previous = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_previous_button"));
    fullscreen_preview_next     = GTK_WIDGET (gtk_builder_get_object (builder, "fullscreen_preview_next_button"));

    gtk_widget_set_no_show_all (root_warning_infobar, TRUE);
    widget_set_best_visual (preview);

    if (!is_program_in_path (GPM_COMMAND)) {
        gtk_widget_set_no_show_all (gpm_button, TRUE);
        gtk_widget_hide (gpm_button);
    }

    screensaver_channel = xfconf_channel_get(SETTINGS_XFCONF_CHANNEL);
    g_signal_connect (screensaver_channel,
                      "property-changed",
                      G_CALLBACK (key_changed_cb),
                      NULL);

    xfpm_channel = xfconf_channel_get(XFPM_XFCONF_CHANNEL);
    g_signal_connect (xfpm_channel,
                      "property-changed",
                      G_CALLBACK (key_changed_cb),
                      NULL);

    if (!is_program_in_path (CONFIGURE_COMMAND)) {
        gtk_widget_set_no_show_all (configure_toolbar, TRUE);
        gtk_widget_hide (configure_toolbar);
    } else {
        g_signal_connect (configure_button,
                          "clicked",
                          G_CALLBACK (configure_button_clicked_cb),
                          screensaver_channel);
    }

    /* Idle delay */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_delay"));
    delay = config_get_idle_delay (&idle_delay_writable);
    ui_set_idle_delay (delay);
    set_widget_writable (widget, idle_delay_writable);
    g_signal_connect (widget, "value-changed",
                      G_CALLBACK (idle_delay_value_changed_cb), NULL);

    /* Lock delay */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_delay"));
    delay = config_get_lock_delay (&lock_delay_writable);
    ui_set_lock_delay (delay);
    set_widget_writable (widget, lock_delay_writable);
    g_signal_connect (widget, "value-changed",
                      G_CALLBACK (lock_delay_value_changed_cb), NULL);

    /* Keyboard command */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_command"));
    command = config_get_keyboard_command (&keyboard_command_writable);
    ui_set_keyboard_command (command);
    set_widget_writable (widget, keyboard_command_writable);
    g_signal_connect (widget, "changed",
                      G_CALLBACK (keyboard_command_changed_cb), NULL);
    g_free (command);

    /* Logout command */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_command"));
    command = config_get_logout_command (&logout_command_writable);
    ui_set_logout_command (command);
    set_widget_writable (widget, logout_command_writable);
    g_signal_connect (widget, "changed",
                      G_CALLBACK (logout_command_changed_cb), NULL);
    g_free (command);

    /* Logout delay */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_delay"));
    delay = config_get_logout_delay (&logout_delay_writable);
    ui_set_logout_delay (delay);
    set_widget_writable (widget, logout_delay_writable);
    g_signal_connect (widget, "value-changed",
                      G_CALLBACK (logout_delay_value_changed_cb), NULL);

    /* Idle activation enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_idle_activation_enabled"));
    enabled = config_get_idle_activation_enabled (&is_writable);
    ui_set_idle_activation_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (idle_activation_toggled_cb), NULL);

    /* Saver enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_enabled"));
    enabled = config_get_saver_enabled (&is_writable);
    ui_set_saver_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (saver_toggled_cb), NULL);

    /* Lock on suspend/hibernate enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_suspend_hibernate_enabled"));
    enabled = config_get_lock_on_suspend_hibernate_enabled (&is_writable);
    ui_set_lock_on_suspend_hibernate_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (lock_on_suspend_hibernate_toggled_cb), NULL);

    /* Lock enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_enabled"));
    enabled = config_get_lock_enabled (&is_writable);
    ui_set_lock_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (lock_toggled_cb), NULL);

    /* Lock with saver enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_saver_activation_enabled"));
    enabled = config_get_lock_with_saver_enabled (&is_writable);
    ui_set_lock_with_saver_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (lock_with_saver_toggled_cb), NULL);

    /* Fullscreen inhibit enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_fullscreen_inhibit_enabled"));
    enabled = config_get_fullscreen_inhibit (&is_writable);
    ui_set_fullscreen_inhibit_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (fullscreen_inhibit_toggled_cb), NULL);

    /* Cycle delay */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "saver_themes_cycle_delay"));
    delay = config_get_cycle_delay (&is_writable);
    ui_set_cycle_delay (delay);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "changed",
                      G_CALLBACK (cycle_delay_value_changed_cb), NULL);

    /* Keyboard enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_embedded_keyboard_enabled"));
    enabled = config_get_keyboard_enabled (&is_writable);
    ui_set_keyboard_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (keyboard_toggled_cb), NULL);

    /* Status message enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_status_messages_enabled"));
    enabled = config_get_status_message_enabled (&is_writable);
    ui_set_status_message_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (status_message_toggled_cb), NULL);

    /* Status message enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_logout_enabled"));
    enabled = config_get_logout_enabled (&is_writable);
    ui_set_logout_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (logout_toggled_cb), NULL);

    /* User switch enabled */
    widget = GTK_WIDGET (gtk_builder_get_object (builder, "lock_user_switching_enabled"));
    enabled = config_get_user_switch_enabled (&is_writable);
    ui_set_user_switch_enabled (enabled);
    set_widget_writable (widget, is_writable);
    g_signal_connect (widget, "notify::active",
                      G_CALLBACK (user_switch_toggled_cb), NULL);

    setup_list_size_constraint (list_scroller, treeview);
    gtk_window_set_default_size (GTK_WINDOW (dialog), 640, 640);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), "org.xfce.ScreenSaver");
    gtk_window_set_icon_name (GTK_WINDOW (fullscreen_preview_window), "org.xfce.ScreenSaver");

    g_signal_connect (fullscreen_preview_area,
                      "draw", G_CALLBACK (preview_on_draw),
                      NULL);

    /* Update list of themes if using random screensaver */
    mode = xfconf_channel_get_int (screensaver_channel, KEY_MODE, DEFAULT_KEY_MODE);
    if (mode == GS_MODE_RANDOM) {
        gchar **list;
        list = get_all_theme_ids (theme_manager);
        xfconf_channel_set_string_list (screensaver_channel, KEY_THEMES, (const gchar * const*) list);
        g_strfreev (list);
    }

    g_signal_connect (preview, "draw", G_CALLBACK (preview_on_draw), NULL);
    gs_job_set_widget (job, preview);

    setup_for_lid_switch ();

    if (check_is_root_user ()) {
        setup_for_root_user ();
    }

    g_signal_connect (preview_button, "clicked",
                      G_CALLBACK (fullscreen_preview_start_cb),
                      treeview);

    g_signal_connect (fullscreen_preview_close, "clicked",
                      G_CALLBACK (fullscreen_preview_cancelled_cb), NULL);
    g_signal_connect (fullscreen_preview_previous, "clicked",
                      G_CALLBACK (fullscreen_preview_previous_cb), NULL);
    g_signal_connect (fullscreen_preview_next, "clicked",
                      G_CALLBACK (fullscreen_preview_next_cb), NULL);

    g_idle_add (setup_treeview_idle, NULL);
}

static void
finalize_capplet (void) {
    if (screensaver_channel)
      g_signal_handlers_disconnect_by_func (screensaver_channel, key_changed_cb, NULL);

    if (xfpm_channel)
      g_signal_handlers_disconnect_by_func (xfpm_channel, key_changed_cb, NULL);

    if (active_theme)
        g_free (active_theme);
}

int
main (int    argc,
      char **argv) {
    GError    *error = NULL;

    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

    if (!gtk_init_with_args (&argc, &argv, "", entries, NULL, &error)) {
        if (G_LIKELY (error)) {
            /* print error */
            g_error ("%s\n", error->message);
            g_error_free (error);
        } else {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type() == 0)
        exit(1);

    if (!xfconf_init(&error)) {
        g_error("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free(error);

        return EXIT_FAILURE;
    }

    job = gs_job_new ();
    theme_manager = gs_theme_manager_new ();

    configure_capplet ();

    if (G_UNLIKELY(opt_socket_id == 0)) {
        GtkWidget *dialog = GTK_WIDGET (gtk_builder_get_object (builder, "prefs_dialog"));

        g_signal_connect(dialog, "response",
                         G_CALLBACK(response_cb), NULL);

        gtk_widget_show_all (dialog);

        /* To prevent the settings dialog to be saved in the session */
        gdk_x11_set_sm_client_id("FAKE ID");

        gtk_main();
    } else {
        GtkWidget *plug;
        GObject   *plug_child;

        /* Create plug widget */
        plug = gtk_plug_new(opt_socket_id);
        g_signal_connect(plug, "delete-event", G_CALLBACK(gtk_main_quit), NULL);
        gtk_widget_show(plug);

        /* Stop startup notification */
        gdk_notify_startup_complete();

        /* Get plug child widget */
        plug_child = gtk_builder_get_object (builder, "plug-child");

#if LIBXFCE4UI_CHECK_VERSION (4, 13, 2)
        xfce_widget_reparent (GTK_WIDGET(plug_child), plug);
#else
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS /* GTK 3.14 */
        gtk_widget_reparent (GTK_WIDGET(plug_child), plug);
        G_GNUC_END_IGNORE_DEPRECATIONS
#endif

        gtk_widget_show_all (GTK_WIDGET(plug_child));

        /* To prevent the settings dialog to be saved in the session */
        gdk_x11_set_sm_client_id("FAKE ID");

        /* Enter main loop */
        gtk_main();
    }

    finalize_capplet ();

    if (theme_manager)
        g_object_unref (theme_manager);

    if (job)
        g_object_unref (job);

    xfconf_shutdown ();

    return 0;
}
