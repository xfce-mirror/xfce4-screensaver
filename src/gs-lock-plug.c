/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2018      Sean Davis <bluesabre@xfce.org>
 * Copyright (C) 2018      Simon Steinbeiss <ochosi@xfce.org>
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

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "gs-lock-plug.h"
#include "gs-debug.h"
#include "gs-prefs.h"
#include "xfce-bg.h"
#include "xfce-desktop-utils.h"
#include "xfce4-screensaver-dialog-ui.h"

#ifdef WITH_KBD_LAYOUT_INDICATOR
#include "xfcekbd-indicator.h"
#endif

#define MDM_FLEXISERVER_COMMAND "mdmflexiserver"
#define MDM_FLEXISERVER_ARGS    "--startnew Standard"

#define GDM_FLEXISERVER_COMMAND "gdmflexiserver"
#define GDM_FLEXISERVER_ARGS    "--startnew Standard"

#define FACE_ICON_SIZE 80
#define DIALOG_TIMEOUT_SEC 60

#define LOGGED_IN_EMBLEM_SIZE 20
#define LOGGED_IN_EMBLEM_ICON "emblem-default"

static void gs_lock_plug_finalize   (GObject         *object);

struct GSLockPlugPrivate {
    GtkWidget   *vbox;
    GtkWidget   *auth_action_area;

    GtkWidget   *auth_face_image;
    GtkWidget   *auth_datetime_label;
    GtkWidget   *auth_realname_label;
    GtkWidget   *auth_username_label;
    GtkWidget   *auth_prompt_label;
    GtkWidget   *auth_prompt_entry;
    GtkWidget   *auth_prompt_infobar;
    GtkWidget   *auth_capslock_label;
    GtkWidget   *auth_message_label;
    GtkWidget   *status_message_label;
    GtkWidget   *background_image;

    GtkWidget   *auth_unlock_button;
    GtkWidget   *auth_switch_button;
    GtkWidget   *auth_cancel_button;
    GtkWidget   *auth_logout_button;

    GtkWidget   *auth_prompt_kbd_layout_indicator;
    GtkWidget   *keyboard_toggle;

    gboolean     caps_lock_on;
    gboolean     switch_enabled;
    gboolean     logout_enabled;
    char        *logout_command;
    char        *status_message;

    guint        datetime_timeout_id;
    guint        cancel_timeout_id;
    guint        auth_check_idle_id;
    guint        response_idle_id;

    gint         monitor_index;

    GList       *key_events;

    GSPrefs       *prefs;
    XfconfChannel *channel;
};

typedef struct _ResponseData ResponseData;

struct _ResponseData {
    gint response_id;
};

enum {
    RESPONSE,
    CLOSE,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_LOGOUT_ENABLED,
    PROP_LOGOUT_COMMAND,
    PROP_SWITCH_ENABLED,
    PROP_STATUS_MESSAGE,
    PROP_MONITOR_INDEX
};

static guint lock_plug_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GSLockPlug, gs_lock_plug, GTK_TYPE_PLUG)

static void
gs_lock_plug_style_set (GtkWidget *widget,
                        GtkStyle  *previous_style) {
    GSLockPlug *plug;

    if (GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->style_set) {
        GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->style_set (widget, previous_style);
    }

    plug = GS_LOCK_PLUG (widget);

    if (!plug->priv->vbox) {
        return;
    }

    gtk_container_set_border_width (GTK_CONTAINER (plug->priv->vbox), 12);
    gtk_box_set_spacing (GTK_BOX (plug->priv->vbox), 12);

    gtk_container_set_border_width (GTK_CONTAINER (plug->priv->auth_action_area), 0);
    gtk_box_set_spacing (GTK_BOX (plug->priv->auth_action_area), 5);
}

static void
toggle_infobar_visibility (GSLockPlug *plug)
{
    gboolean visible = FALSE;
    if (gtk_widget_get_visible (plug->priv->status_message_label)) {
        visible = TRUE;
    } else if (gtk_widget_get_visible (plug->priv->auth_prompt_label)) {
        visible = TRUE;
    } else if (gtk_widget_get_visible (plug->priv->auth_capslock_label)) {
        visible = TRUE;
    } else if (gtk_widget_get_visible (plug->priv->auth_message_label)) {
        visible = TRUE;
    }
    gtk_widget_set_visible (plug->priv->auth_prompt_infobar, visible);
}

static gboolean
process_is_running (const char * name) {
    int    num_processes;
    gchar *command = g_strdup_printf ("pidof %s | wc -l", name);
    FILE  *fp = popen(command, "r");

    if (fscanf(fp, "%d", &num_processes) != 1)
            num_processes = 0;

    pclose(fp);
    g_free (command);

    if (num_processes > 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
do_user_switch (GSLockPlug *plug) {
    GError   *error;
    gboolean  res;
    char     *command;

    if (process_is_running ("mdm")) {
        /* MDM */
        command = g_strdup_printf ("%s %s",
                                   MDM_FLEXISERVER_COMMAND,
                                   MDM_FLEXISERVER_ARGS);

        error = NULL;
        res = xfce_gdk_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                     command,
                                                     &error);

        g_free (command);

        if (!res) {
            gs_debug ("Unable to start MDM greeter: %s", error->message);
            g_error_free (error);
        }
    } else if (process_is_running ("gdm") || process_is_running("gdm3") || process_is_running("gdm-binary")) {
        /* GDM */
        command = g_strdup_printf ("%s %s",
                                   GDM_FLEXISERVER_COMMAND,
                                   GDM_FLEXISERVER_ARGS);

        error = NULL;
        res = xfce_gdk_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                     command,
                                                     &error);

        g_free (command);

        if (!res) {
            gs_debug ("Unable to start GDM greeter: %s", error->message);
            g_error_free (error);
        }
    } else if (process_is_running ("lightdm")) {
        /* LightDM */
        GDBusProxyFlags flags = G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;
        GDBusProxy *proxy = NULL;
        const gchar *seat_path = g_getenv ("XDG_SEAT_PATH");
        if (seat_path == NULL) {
            seat_path = "/org/freedesktop/DisplayManager/Seat0";
        }

        error = NULL;
        proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                              flags,
                                              NULL,
                                              "org.freedesktop.DisplayManager",
                                              seat_path,
                                              "org.freedesktop.DisplayManager.Seat",
                                              NULL,
                                              &error);
        if (proxy != NULL) {
            g_dbus_proxy_call_sync (proxy,
                                    "SwitchToGreeter",
                                    g_variant_new ("()"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    NULL);
            g_object_unref (proxy);
        } else {
            gs_debug ("Unable to start LightDM greeter: %s", error->message);
            g_error_free (error);
        }
    }
}

static void
set_status_text (GSLockPlug *plug,
                 const char *text) {
    if (plug->priv->auth_message_label != NULL) {
        gtk_label_set_text (GTK_LABEL (plug->priv->auth_message_label), text);
        if (g_utf8_strlen (text, 1) == 0) {
            gtk_widget_hide (GTK_WIDGET (plug->priv->auth_message_label));
        } else {
            gtk_widget_show (GTK_WIDGET (plug->priv->auth_message_label));
        }
        toggle_infobar_visibility (plug);
    }
}

static gboolean
date_time_update (gpointer user_data) {
    GSLockPlug *plug = user_data;
    GDateTime *datetime;
    gchar     *datetime_format;
    gchar     *str;

    datetime = g_date_time_new_now_local ();
    if (datetime == NULL)
        goto out;

    /* TRANSLATORS: adjust this accordingly for your locale format */
    datetime_format = g_date_time_format (datetime, NC_("Date",
                                                        "%A, %B %e   %H:%M"));
    if (datetime_format == NULL)
        goto out_datetime;

    str = g_strdup_printf ("<b>%s</b>", datetime_format);
    if (str == NULL)
        goto out_str;

    gtk_label_set_markup (GTK_LABEL (plug->priv->auth_datetime_label), str);
    g_free (str);

out_str:
    g_free (datetime_format);
out_datetime:
    g_date_time_unref (datetime);
out:
    return TRUE;
}

void
gs_lock_plug_set_sensitive (GSLockPlug *plug,
                            gboolean    sensitive) {
    g_return_if_fail (GS_IS_LOCK_PLUG (plug));

    gtk_widget_set_sensitive (plug->priv->auth_prompt_entry, sensitive);
    gtk_widget_set_sensitive (plug->priv->auth_action_area, sensitive);
}

static void
remove_datetime_timeout (GSLockPlug *plug) {
    if (plug->priv->datetime_timeout_id > 0) {
        g_source_remove (plug->priv->datetime_timeout_id);
        plug->priv->datetime_timeout_id = 0;
    }
}

static void
remove_cancel_timeout (GSLockPlug *plug) {
    if (plug->priv->cancel_timeout_id > 0) {
        g_source_remove (plug->priv->cancel_timeout_id);
        plug->priv->cancel_timeout_id = 0;
    }
}

static void
remove_response_idle (GSLockPlug *plug) {
    if (plug->priv->response_idle_id > 0) {
        g_source_remove (plug->priv->response_idle_id);
        plug->priv->response_idle_id = 0;
    }
}

static void
gs_lock_plug_response (GSLockPlug *plug,
                       gint        response_id) {
    g_return_if_fail (GS_IS_LOCK_PLUG (plug));

    /* Act only on response IDs we recognize */
    if (!(response_id == GS_LOCK_PLUG_RESPONSE_OK ||
          response_id == GS_LOCK_PLUG_RESPONSE_CANCEL)) {
        return;
    }

    remove_cancel_timeout (plug);
    remove_response_idle (plug);

    if (response_id == GS_LOCK_PLUG_RESPONSE_CANCEL) {
        gtk_entry_set_text (GTK_ENTRY (plug->priv->auth_prompt_entry), "");
    }

    g_signal_emit (plug, lock_plug_signals[RESPONSE], 0, response_id);
}

static gboolean
response_cancel_idle (gpointer user_data) {
    GSLockPlug *plug = user_data;

    plug->priv->response_idle_id = 0;

    gs_lock_plug_response (plug, GS_LOCK_PLUG_RESPONSE_CANCEL);

    return FALSE;
}

static gboolean
dialog_timed_out (gpointer user_data) {
    GSLockPlug *plug = user_data;

    plug->priv->cancel_timeout_id = 0;

    gs_lock_plug_set_sensitive (plug, FALSE);
    set_status_text (plug, _("Time has expired."));

    if (plug->priv->response_idle_id != 0) {
        g_warning ("Response idle ID already set but shouldn't be");
    }

    remove_response_idle (plug);

    plug->priv->response_idle_id = g_timeout_add_seconds (2, response_cancel_idle, plug);
    return FALSE;
}


static void
capslock_update (GSLockPlug *plug,
                 gboolean    is_on) {
    plug->priv->caps_lock_on = is_on;

    if (plug->priv->auth_capslock_label == NULL) {
        return;
    }

    if (is_on) {
        gtk_widget_show (GTK_WIDGET (plug->priv->auth_capslock_label));
    } else {
        gtk_widget_hide (GTK_WIDGET (plug->priv->auth_capslock_label));
    }
    toggle_infobar_visibility (plug);
}

static gboolean
is_capslock_on (void) {
    GdkKeymap *keymap;

    keymap = gdk_keymap_get_for_display (gdk_display_get_default());;
    if (keymap == NULL)
        return FALSE;

    return gdk_keymap_get_caps_lock_state (keymap);
}

static void
restart_cancel_timeout (GSLockPlug *plug) {
    remove_cancel_timeout (plug);
    plug->priv->cancel_timeout_id = g_timeout_add_seconds (DIALOG_TIMEOUT_SEC, dialog_timed_out, plug);
}

void
gs_lock_plug_get_text (GSLockPlug  *plug,
                       char       **text) {
    const char *typed_text;
    char       *null_text;
    char       *local_text;

    typed_text = gtk_entry_get_text (GTK_ENTRY (plug->priv->auth_prompt_entry));
    local_text = g_locale_from_utf8 (typed_text, strlen (typed_text), NULL, NULL, NULL);

    null_text = g_strnfill (strlen (typed_text) + 1, '\b');
    gtk_entry_set_text (GTK_ENTRY (plug->priv->auth_prompt_entry), null_text);
    gtk_entry_set_text (GTK_ENTRY (plug->priv->auth_prompt_entry), "");
    g_free (null_text);

    if (text != NULL) {
        *text = local_text;
    } else {
        g_free (local_text);
    }
}

typedef struct {
    GSLockPlug *plug;
    gint        response_id;
    GMainLoop  *loop;
    gboolean    destroyed;
} RunInfo;

static void
shutdown_loop (RunInfo *ri) {
    if (g_main_loop_is_running (ri->loop))
        g_main_loop_quit (ri->loop);
}

static void
run_unmap_handler (GSLockPlug *plug,
                   gpointer    data) {
    RunInfo *ri = data;

    shutdown_loop (ri);
}

static void
run_response_handler (GSLockPlug *plug,
                      gint        response_id,
                      gpointer    data) {
    RunInfo *ri;

    ri = data;

    ri->response_id = response_id;

    shutdown_loop (ri);
}

static gint
run_delete_handler (GSLockPlug  *plug,
                    GdkEventAny *event,
                    gpointer     data) {
    RunInfo *ri = data;

    shutdown_loop (ri);

    return TRUE; /* Do not destroy */
}

static void
run_destroy_handler (GSLockPlug *plug,
                     gpointer    data) {
    RunInfo *ri = data;

    /* shutdown_loop will be called by run_unmap_handler */
    ri->destroyed = TRUE;
}

static void
run_keymap_handler (GdkKeymap  *keymap,
                    GSLockPlug *plug) {
    capslock_update (plug, is_capslock_on ());
}

/* adapted from GTK+ gtkdialog.c */
int
gs_lock_plug_run (GSLockPlug *plug) {
    RunInfo    ri = { NULL, GTK_RESPONSE_NONE, NULL, FALSE };
    gboolean   was_modal;
    gulong     response_handler;
    gulong     unmap_handler;
    gulong     destroy_handler;
    gulong     deletion_handler;
    gulong     keymap_handler;
    GdkKeymap *keymap;

    g_return_val_if_fail (GS_IS_LOCK_PLUG (plug), -1);

    g_object_ref (plug);

    was_modal = gtk_window_get_modal (GTK_WINDOW (plug));
    if (!was_modal) {
        gtk_window_set_modal (GTK_WINDOW (plug), TRUE);
    }

    if (!gtk_widget_get_visible (GTK_WIDGET (plug))) {
        gtk_widget_show (GTK_WIDGET (plug));
    }

    keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (plug)));

    keymap_handler =
        g_signal_connect (keymap,
                          "state-changed",
                          G_CALLBACK (run_keymap_handler),
                          plug);

    response_handler =
        g_signal_connect (plug,
                          "response",
                          G_CALLBACK (run_response_handler),
                          &ri);

    unmap_handler =
        g_signal_connect (plug,
                          "unmap",
                          G_CALLBACK (run_unmap_handler),
                          &ri);

    deletion_handler =
        g_signal_connect (plug,
                          "delete_event",
                          G_CALLBACK (run_delete_handler),
                          &ri);

    destroy_handler =
        g_signal_connect (plug,
                          "destroy",
                          G_CALLBACK (run_destroy_handler),
                          &ri);

    ri.loop = g_main_loop_new (NULL, FALSE);

    g_main_loop_run (ri.loop);

    g_main_loop_unref (ri.loop);

    ri.loop = NULL;

    if (!ri.destroyed) {
        if (!was_modal) {
            gtk_window_set_modal (GTK_WINDOW (plug), FALSE);
        }

        g_signal_handler_disconnect (plug, response_handler);
        g_signal_handler_disconnect (plug, unmap_handler);
        g_signal_handler_disconnect (plug, deletion_handler);
        g_signal_handler_disconnect (plug, destroy_handler);
        g_signal_handler_disconnect (keymap, keymap_handler);
    }

    g_object_unref (plug);

    return ri.response_id;
}

static GdkPixbuf *
get_user_icon_from_accounts_service (void) {
    GDBusConnection *bus;
    GError          *error = NULL;
    GVariant        *variant, *res;
    const gchar     *user_path;
    GdkPixbuf       *pixbuf = NULL;

    bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (bus == NULL) {
        g_warning ("Failed to get system bus: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    variant = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.Accounts",
                                           "/org/freedesktop/Accounts",
                                           "org.freedesktop.Accounts",
                                           "FindUserByName",
                                           g_variant_new ("(s)",
                                                          g_get_user_name ()),
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1, NULL, &error);
    if (variant == NULL) {
        gs_debug ("Could not find user: %s", error->message);
        g_error_free (error);
        g_object_unref (bus);
        return NULL;
    }

    user_path = g_variant_get_string (g_variant_get_child_value (variant,
                                                                 0),
                                      NULL);
    g_variant_unref (variant);
    variant = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.Accounts",
                                           user_path,
                                           "org.freedesktop.DBus.Properties",
                                           "Get",
                                           g_variant_new ("(ss)",
                                                          "org.freedesktop.Accounts.User",
                                                          "IconFile"),
                                           G_VARIANT_TYPE ("(v)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1, NULL, &error);
    if (variant == NULL) {
        gs_debug ("Could not find user icon: %s", error->message);
        g_error_free (error);
        g_object_unref (bus);
        return NULL;
    }

    g_variant_get_child (variant, 0, "v", &res);
    pixbuf = gdk_pixbuf_new_from_file_at_scale (g_variant_get_string (res,
                                                                      NULL),
                                                FACE_ICON_SIZE, FACE_ICON_SIZE, FALSE,
                                                &error);
    if (pixbuf == NULL) {
        g_warning ("Could not load user avatar: %s", error->message);
        g_error_free (error);
    }

    g_variant_unref (res);
    g_variant_unref (variant);
    g_object_unref (bus);

    return pixbuf;
}

static GdkPixbuf *
logged_in_pixbuf (GdkPixbuf *pixbuf)
{
    GdkPixbuf *composite = NULL, *emblem = NULL;
    gint width, height;
    GError *error = NULL;

    emblem = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       LOGGED_IN_EMBLEM_ICON,
                                       LOGGED_IN_EMBLEM_SIZE,
                                       GTK_ICON_LOOKUP_FORCE_SIZE,
                                       &error);

    if (!emblem) {
        g_warning ("Failed to load the logged icon: %s", error->message);
        g_clear_error (&error);
        return NULL;
    }

    composite = gdk_pixbuf_copy (pixbuf);

    width = gdk_pixbuf_get_width (composite);
    height = gdk_pixbuf_get_height (composite);

    gdk_pixbuf_composite (emblem, composite,
                          width - LOGGED_IN_EMBLEM_SIZE,
                          height - LOGGED_IN_EMBLEM_SIZE,
                          LOGGED_IN_EMBLEM_SIZE,
                          LOGGED_IN_EMBLEM_SIZE,
                          width - LOGGED_IN_EMBLEM_SIZE,
                          height - LOGGED_IN_EMBLEM_SIZE,
                          1.0,
                          1.0,
                          GDK_INTERP_BILINEAR,
                          255);

    g_object_unref (emblem);

    return composite;
}

static GdkPixbuf *
round_image (GdkPixbuf *pixbuf)
{
    GdkPixbuf *dest = NULL;
    cairo_surface_t *surface;
    cairo_t *cr;
    gint size;

    size = gdk_pixbuf_get_width (pixbuf);
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
    cr = cairo_create (surface);

    /* Clip a circle */
    cairo_arc (cr, size/2, size/2, size/2, 0, 2 * G_PI);
    cairo_clip (cr);
    cairo_new_path (cr);

    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_paint (cr);

    dest = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    return dest;
}

static gboolean
set_face_image (GSLockPlug *plug) {
    char      *path;
    GError    *error = NULL;
    GdkPixbuf *pixbuf, *temp_image;

    pixbuf = get_user_icon_from_accounts_service ();
    if (pixbuf == NULL) {
        path = g_build_filename (g_get_home_dir(), ".face", NULL);
        pixbuf = gdk_pixbuf_new_from_file_at_scale (path, FACE_ICON_SIZE, FACE_ICON_SIZE, FALSE, &error);
        if (pixbuf == NULL) {
            g_warning ("Could not load the user avatar: %s", error->message);
            g_error_free (error);
            return FALSE;
        }
    }

    temp_image = round_image (pixbuf);
    if (temp_image != NULL) {
        g_object_unref (pixbuf);
        pixbuf = temp_image;
    }

    temp_image = logged_in_pixbuf (pixbuf);
    if (temp_image != NULL) {
        g_object_unref (pixbuf);
        pixbuf = temp_image;
    }

    gtk_image_set_from_pixbuf (GTK_IMAGE (plug->priv->auth_face_image), pixbuf);
    g_object_unref (pixbuf);

    return TRUE;
}

#if !GTK_CHECK_VERSION (3, 23, 0)
static void
gs_lock_plug_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum_width,
                                  gint      *natural_width) {
    gint scale;

    GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

    scale = gtk_widget_get_scale_factor (widget);
    *minimum_width /= scale;
    *natural_width /= scale;
}

static void
gs_lock_plug_get_preferred_height_for_width (GtkWidget *widget,
                                             gint       width,
                                             gint      *minimum_height,
                                             gint      *natural_height) {
    gint scale;

    GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->get_preferred_height_for_width (widget,
                                                                                  width,
                                                                                  minimum_height,
                                                                                  natural_height);

    scale = gtk_widget_get_scale_factor (widget);
    *minimum_height /= scale;
    *natural_height /= scale;
}
#endif

static void
gs_lock_plug_show (GtkWidget *widget) {
    GSLockPlug *plug = GS_LOCK_PLUG (widget);

    gs_profile_start (NULL);

    gs_profile_start ("parent");
    if (GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->show) {
        GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->show (widget);
    }

    gs_profile_end ("parent");

    if (plug->priv->auth_face_image) {
        set_face_image (plug);
    }

    capslock_update (plug, is_capslock_on ());

    restart_cancel_timeout (plug);

    gs_profile_end (NULL);
}

static void
gs_lock_plug_hide (GtkWidget *widget) {
    if (GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->hide) {
        GTK_WIDGET_CLASS (gs_lock_plug_parent_class)->hide (widget);
    }
}

static void
queue_key_event (GSLockPlug  *plug,
                 GdkEventKey *event) {
    GdkEvent *saved_event;

    saved_event = gdk_event_copy ((GdkEvent *)event);
    plug->priv->key_events = g_list_prepend (plug->priv->key_events,
                             saved_event);
}

static void
forward_key_events (GSLockPlug *plug) {
    plug->priv->key_events = g_list_reverse (plug->priv->key_events);
    while (plug->priv->key_events != NULL) {
        GdkEventKey *event = plug->priv->key_events->data;

        gtk_window_propagate_key_event (GTK_WINDOW (plug), event);

        gdk_event_free ((GdkEvent *)event);

        plug->priv->key_events = g_list_delete_link (plug->priv->key_events,
                                 plug->priv->key_events);
    }
}

static void
gs_lock_plug_set_logout_enabled (GSLockPlug *plug,
                                 gboolean    logout_enabled) {
    g_return_if_fail (GS_LOCK_PLUG (plug));

    if (plug->priv->logout_enabled == logout_enabled) {
        return;
    }

    plug->priv->logout_enabled = logout_enabled;
    g_object_notify (G_OBJECT (plug), "logout-enabled");

    if (plug->priv->auth_logout_button == NULL) {
        return;
    }

    if (logout_enabled) {
        gtk_widget_show (plug->priv->auth_logout_button);
    } else {
        gtk_widget_hide (plug->priv->auth_logout_button);
    }
}

static void
gs_lock_plug_set_monitor_index (GSLockPlug *plug,
                                gint        monitor_index) {
    g_return_if_fail (GS_LOCK_PLUG (plug));

    if (plug->priv->monitor_index == monitor_index) {
        return;
    }

    plug->priv->monitor_index = monitor_index;
    g_object_notify (G_OBJECT (plug), "monitor-index");
}

static void
gs_lock_plug_set_logout_command (GSLockPlug *plug,
                                 const char *command) {
    g_return_if_fail (GS_LOCK_PLUG (plug));

    g_free (plug->priv->logout_command);

    if (command) {
        plug->priv->logout_command = g_strdup (command);
    } else {
        plug->priv->logout_command = NULL;
    }
}

static void
gs_lock_plug_set_status_message (GSLockPlug *plug,
                                 const char *status_message) {
    g_return_if_fail (GS_LOCK_PLUG (plug));

    g_free (plug->priv->status_message);
    plug->priv->status_message = g_strdup (status_message);

    if (plug->priv->status_message_label) {
        if (plug->priv->status_message) {
            gtk_label_set_text (GTK_LABEL (plug->priv->status_message_label),
                                plug->priv->status_message);
            gtk_widget_show (plug->priv->status_message_label);
        } else {
            gtk_widget_hide (plug->priv->status_message_label);
        }
        toggle_infobar_visibility (plug);
    }
}

static void
gs_lock_plug_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec) {
    GSLockPlug *self;

    self = GS_LOCK_PLUG (object);

    switch (prop_id) {
        case PROP_LOGOUT_ENABLED:
            g_value_set_boolean (value, self->priv->logout_enabled);
            break;
        case PROP_LOGOUT_COMMAND:
            g_value_set_string (value, self->priv->logout_command);
            break;
        case PROP_SWITCH_ENABLED:
            g_value_set_boolean (value, self->priv->switch_enabled);
            break;
        case PROP_STATUS_MESSAGE:
            g_value_set_string (value, self->priv->status_message);
            break;
        case PROP_MONITOR_INDEX:
            g_value_set_int (value, self->priv->monitor_index);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_lock_plug_set_switch_enabled (GSLockPlug *plug,
                                 gboolean    switch_enabled) {
    g_return_if_fail (GS_LOCK_PLUG (plug));

    if (plug->priv->switch_enabled == switch_enabled) {
        return;
    }

    plug->priv->switch_enabled = switch_enabled;
    g_object_notify (G_OBJECT (plug), "switch-enabled");

    if (plug->priv->auth_switch_button == NULL) {
        return;
    }

    if (switch_enabled) {
        if (process_is_running ("mdm")) {
            /* MDM  */
            gtk_widget_show (plug->priv->auth_switch_button);
        } else if (process_is_running ("gdm") || process_is_running("gdm3") || process_is_running("gdm-binary")) {
            /* GDM */
            gtk_widget_show (plug->priv->auth_switch_button);
        } else if (process_is_running ("lightdm")) {
            /* LightDM */
            gtk_widget_show (plug->priv->auth_switch_button);
        } else {
            gs_debug ("Warning: Unknown DM for switch button");
            gtk_widget_hide (plug->priv->auth_switch_button);
        }
    } else {
        gtk_widget_hide (plug->priv->auth_switch_button);
    }
}

static void
gs_lock_plug_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec) {
    GSLockPlug *self;

    self = GS_LOCK_PLUG (object);

    switch (prop_id) {
        case PROP_LOGOUT_ENABLED:
            gs_lock_plug_set_logout_enabled (self, g_value_get_boolean (value));
            break;
        case PROP_LOGOUT_COMMAND:
            gs_lock_plug_set_logout_command (self, g_value_get_string (value));
            break;
        case PROP_STATUS_MESSAGE:
            gs_lock_plug_set_status_message (self, g_value_get_string (value));
            break;
        case PROP_SWITCH_ENABLED:
            gs_lock_plug_set_switch_enabled (self, g_value_get_boolean (value));
            break;
        case PROP_MONITOR_INDEX:
            gs_lock_plug_set_monitor_index (self, g_value_get_int (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_lock_plug_close (GSLockPlug *plug) {
    /* Synthesize delete_event to close dialog. */

    GtkWidget *widget = GTK_WIDGET (plug);
    GdkEvent  *event;

    event = gdk_event_new (GDK_DELETE);
    event->any.window = g_object_ref (gtk_widget_get_window(widget));
    event->any.send_event = TRUE;

    gtk_main_do_event (event);
    gdk_event_free (event);
}

static void
gs_lock_plug_class_init (GSLockPlugClass *klass) {
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkBindingSet  *binding_set;

    object_class->finalize     = gs_lock_plug_finalize;
    object_class->get_property = gs_lock_plug_get_property;
    object_class->set_property = gs_lock_plug_set_property;

    widget_class->style_set                      = gs_lock_plug_style_set;
    widget_class->show                           = gs_lock_plug_show;
    widget_class->hide                           = gs_lock_plug_hide;
#if !GTK_CHECK_VERSION (3, 23, 0)
    widget_class->get_preferred_width            = gs_lock_plug_get_preferred_width;
    widget_class->get_preferred_height_for_width = gs_lock_plug_get_preferred_height_for_width;
#endif

    klass->close = gs_lock_plug_close;

    lock_plug_signals[RESPONSE] = g_signal_new ("response",
                                                G_OBJECT_CLASS_TYPE (klass),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GSLockPlugClass, response),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__INT,
                                                G_TYPE_NONE, 1,
                                                G_TYPE_INT);
    lock_plug_signals[CLOSE] = g_signal_new ("close",
                                             G_OBJECT_CLASS_TYPE (klass),
                                             G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                             G_STRUCT_OFFSET (GSLockPlugClass, close),
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__VOID,
                                             G_TYPE_NONE, 0);

    g_object_class_install_property (object_class,
                                     PROP_LOGOUT_ENABLED,
                                     g_param_spec_boolean ("logout-enabled",
                                                           NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_LOGOUT_COMMAND,
                                     g_param_spec_string ("logout-command",
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_STATUS_MESSAGE,
                                     g_param_spec_string ("status-message",
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_SWITCH_ENABLED,
                                     g_param_spec_boolean ("switch-enabled",
                                                           NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_MONITOR_INDEX,
                                     g_param_spec_int ("monitor-index",
                                                       NULL,
                                                       NULL,
                                                       0,
                                                       200,
                                                       0,
                                                       G_PARAM_READWRITE));

    binding_set = gtk_binding_set_by_class (klass);

    gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
                                  "close", 0);
}

static void
clear_clipboards (GSLockPlug *plug) {
    GtkClipboard *clipboard;

    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (plug), GDK_SELECTION_PRIMARY);
    gtk_clipboard_clear (clipboard);
    gtk_clipboard_set_text (clipboard, "", -1);
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (plug), GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_clear (clipboard);
    gtk_clipboard_set_text (clipboard, "", -1);
}

static void
logout_button_clicked (GtkButton  *button,
                       GSLockPlug *plug) {
    char     **argv  = NULL;
    GError    *error = NULL;
    gboolean   res;

    if (!plug->priv->logout_command) {
        return;
    }

    res = g_shell_parse_argv (plug->priv->logout_command, NULL, &argv, &error);

    if (!res) {
        g_warning ("Could not parse logout command: %s", error->message);
        g_error_free (error);
        return;
    }

    g_spawn_async (g_get_home_dir (),
                   argv,
                   NULL,
                   G_SPAWN_SEARCH_PATH,
                   NULL,
                   NULL,
                   NULL,
                   &error);

    g_strfreev (argv);

    if (error) {
        g_warning ("Could not run logout command: %s", error->message);
        g_error_free (error);
    }
}

void
gs_lock_plug_set_busy (GSLockPlug *plug) {
    GdkDisplay *display;
    GdkCursor  *cursor;
    GtkWidget  *top_level;

    top_level = gtk_widget_get_toplevel (GTK_WIDGET (plug));

    display = gtk_widget_get_display (GTK_WIDGET (plug));
    cursor = gdk_cursor_new_for_display (display, GDK_WATCH);

    gdk_window_set_cursor (gtk_widget_get_window (top_level), cursor);
    g_object_unref (cursor);
}

void
gs_lock_plug_set_ready (GSLockPlug *plug) {
    GdkDisplay *display;
    GdkCursor  *cursor;
    GtkWidget  *top_level;

    top_level = gtk_widget_get_toplevel (GTK_WIDGET (plug));

    display = gtk_widget_get_display (GTK_WIDGET (plug));
    cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);
    gdk_window_set_cursor (gtk_widget_get_window (top_level), cursor);
    g_object_unref (cursor);
}

void
gs_lock_plug_enable_prompt (GSLockPlug *plug,
                            const char *message,
                            gboolean    visible) {
    g_return_if_fail (GS_IS_LOCK_PLUG (plug));

    gs_debug ("Setting prompt to: %s", message);

    gtk_widget_set_sensitive (plug->priv->auth_unlock_button, TRUE);
    gtk_widget_show (plug->priv->auth_unlock_button);
    gtk_widget_grab_default (plug->priv->auth_unlock_button);

    gtk_label_set_text(GTK_LABEL(plug->priv->auth_prompt_label), message);
    if (g_utf8_strlen(message, -1) == 0) {
        gtk_widget_hide (GTK_WIDGET (plug->priv->auth_prompt_label));
    } else {
        gtk_widget_show (GTK_WIDGET (plug->priv->auth_prompt_label));
    }
    toggle_infobar_visibility (plug);

    gtk_entry_set_visibility (GTK_ENTRY (plug->priv->auth_prompt_entry), visible);
    gtk_widget_set_sensitive (plug->priv->auth_prompt_entry, TRUE);
    gtk_widget_show (plug->priv->auth_prompt_entry);

    if (!gtk_widget_has_focus (plug->priv->auth_prompt_entry)) {
        gtk_widget_grab_focus (plug->priv->auth_prompt_entry);
    }

    /* were there any key events sent to the plug while the
     * entry wasnt ready? If so, forward them along
     */
    forward_key_events (plug);

    restart_cancel_timeout (plug);
}

void
gs_lock_plug_disable_prompt (GSLockPlug *plug) {
    g_return_if_fail (GS_IS_LOCK_PLUG (plug));

    /* gtk_widget_hide (plug->priv->auth_prompt_entry); */
    /* gtk_widget_hide (plug->priv->auth_prompt_label); */
    gtk_widget_set_sensitive (plug->priv->auth_unlock_button, FALSE);
    gtk_widget_set_sensitive (plug->priv->auth_prompt_entry, FALSE);
    /* gtk_widget_hide (plug->priv->auth_unlock_button); */

    gtk_widget_grab_default (plug->priv->auth_cancel_button);
}

void
gs_lock_plug_show_message (GSLockPlug *plug,
                           const char *message) {
    g_return_if_fail (GS_IS_LOCK_PLUG (plug));

    set_status_text (plug, message ? message : "");
}

/* button press handler used to inhibit popup menu */
static gint
entry_button_press (GtkWidget      *widget,
                    GdkEventButton *event) {
    if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
        return TRUE;
    }

    return FALSE;
}

static gint
entry_key_press (GtkWidget   *widget,
                 GdkEventKey *event,
                 GSLockPlug  *plug) {
    restart_cancel_timeout (plug);

    /* if the input widget is visible and ready for input
     * then just carry on as usual
     */
    if (gtk_widget_get_visible (plug->priv->auth_prompt_entry) &&
            gtk_widget_is_sensitive (plug->priv->auth_prompt_entry)) {
        return FALSE;
    }

    if (strcmp (event->string, "") == 0) {
        return FALSE;
    }

    queue_key_event (plug, event);

    return TRUE;
}

static char *
get_user_display_name (void) {
    const char *name;
    char       *utf8_name;

    name = g_get_real_name ();

    if (name == NULL || g_strcmp0 (name, "") == 0 ||
        g_strcmp0 (name, "Unknown") == 0) {
        name = g_get_user_name ();
    }

    utf8_name = NULL;

    if (name != NULL) {
        utf8_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
    }

    return utf8_name;
}

static char *
get_user_name (void) {
    const char *name;
    char       *utf8_name;

    name = g_get_user_name ();

    utf8_name = NULL;
    if (name != NULL) {
        utf8_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
    }

    return utf8_name;
}

/* adapted from MDM */
static char *
expand_string (const char *text) {
    GString        *str;
    const char     *p;
    char           *username;
    int             i;
    int             n_chars;
    struct utsname  name;

    str = g_string_sized_new (strlen (text));

    p = text;
    n_chars = g_utf8_strlen (text, -1);
    i = 0;

    while (i < n_chars) {
        gunichar ch;

        ch = g_utf8_get_char (p);

        /* Backslash commands */
        if (ch == '\\') {
            p = g_utf8_next_char (p);
            i++;
            ch = g_utf8_get_char (p);

            if (i >= n_chars || ch == '\0') {
                g_warning ("Unescaped \\ at end of text\n");
                goto bail;
            } else if (ch == 'n') {
                g_string_append_unichar (str, '\n');
            } else {
                g_string_append_unichar (str, ch);
            }
        } else if (ch == '%') {
            p = g_utf8_next_char (p);
            i++;
            ch = g_utf8_get_char (p);

            if (i >= n_chars || ch == '\0') {
                g_warning ("Unescaped %% at end of text\n");
                goto bail;
            }

            switch (ch) {
                case '%':
                    g_string_append (str, "%");
                    break;
                case 'c':
                    /* clock */
                    break;
                case 'd':
                    /* display */
                    g_string_append (str, g_getenv ("DISPLAY"));
                    break;
                case 'h':
                    /* hostname */
                    g_string_append (str, g_get_host_name ());
                    break;
                case 'm':
                    /* machine name */
                    uname (&name);
                    g_string_append (str, name.machine);
                    break;
                case 'n':
                    /* nodename */
                    uname (&name);
                    g_string_append (str, name.nodename);
                    break;
                case 'r':
                    /* release */
                    uname (&name);
                    g_string_append (str, name.release);
                    break;
                case 'R':
                    /* Real name */
                    username = get_user_display_name ();
                    g_string_append (str, username);
                    g_free (username);
                    break;
                case 's':
                    /* system name */
                    uname (&name);
                    g_string_append (str, name.sysname);
                    break;
                case 'U':
                    /* Username */
                    username = get_user_name ();
                    g_string_append (str, username);
                    g_free (username);
                    break;
                default:
                    if (ch < 127) {
                        g_warning ("unknown escape code %%%c in text\n", (char)ch);
                    } else {
                        g_warning ("unknown escape code %%(U%x) in text\n", (int)ch);
                    }
            }
        } else {
            g_string_append_unichar (str, ch);
        }
        p = g_utf8_next_char (p);
        i++;
    }

bail:

    return g_string_free (str, FALSE);
}

static void
expand_string_for_label (GtkWidget *label) {
    const char *template;
    char       *str;

    template = gtk_label_get_label (GTK_LABEL (label));
    str = expand_string (template);
    gtk_label_set_label (GTK_LABEL (label), str);
    g_free (str);
}

static void
unlock_button_clicked (GtkButton  *button,
                       GSLockPlug *plug) {
    gs_lock_plug_response (plug, GS_LOCK_PLUG_RESPONSE_OK);
}

static void
cancel_button_clicked (GtkButton  *button,
                       GSLockPlug *plug) {
    gs_lock_plug_response (plug, GS_LOCK_PLUG_RESPONSE_CANCEL);
}

static void
switch_user_button_clicked (GtkButton  *button,
                            GSLockPlug *plug) {
    remove_response_idle (plug);

    gs_lock_plug_set_sensitive (plug, FALSE);

    plug->priv->response_idle_id = g_timeout_add_seconds (2, response_cancel_idle, plug);

    gs_lock_plug_set_busy (plug);
    do_user_switch (plug);
}

static void
get_draw_dimensions(GSLockPlug *plug,
                    XfceBG     *bg,
                    gint       *screen_width,
                    gint       *screen_height,
                    gint       *monitor_width,
                    gint       *monitor_height,
                    gint       *scale) {
    GdkWindow    *window;
    GdkDisplay   *display;
    GdkScreen    *screen;
    GdkMonitor   *monitor;
    GdkRectangle  geometry;

    window = gtk_widget_get_window (GTK_WIDGET(plug));
    if (window != NULL) {
        display = gdk_window_get_display(window);
    } else {
        display = gdk_display_get_default();
    }
    screen = gdk_display_get_default_screen(display);
    *scale = gdk_window_get_scale_factor(gdk_screen_get_root_window(screen));

    monitor = gdk_display_get_monitor (display, plug->priv->monitor_index);
    if (!monitor) {
        if (window != NULL)
            monitor = gdk_display_get_monitor_at_window(display, window);
        else
            monitor = gdk_display_get_primary_monitor(display);

        if (!monitor)
            monitor = gdk_display_get_monitor (display, 0);
    }

    xfce_bg_load_from_preferences(bg, monitor);

    gdk_monitor_get_geometry(monitor, &geometry);
    *monitor_width = geometry.width;
    *monitor_height = geometry.height;
    *screen_width = WidthOfScreen(gdk_x11_screen_get_xscreen(screen));
    *screen_height = HeightOfScreen(gdk_x11_screen_get_xscreen(screen));
}

static void
redraw_background (GSLockPlug *plug) {
    XfceBG    *bg;
    GdkPixbuf *pixbuf;
    gint       screen_width, screen_height, monitor_width, monitor_height, scale;

    gs_debug ("Redrawing background");
    bg = xfce_bg_new();
    get_draw_dimensions(plug, bg, &screen_width, &screen_height, &monitor_width, &monitor_height, &scale);
    pixbuf = xfce_bg_get_pixbuf(bg, screen_width, screen_height, monitor_width, monitor_height, scale);
    gtk_image_set_from_pixbuf(GTK_IMAGE(plug->priv->background_image), pixbuf);
}

static void
keyboard_toggled_cb (GtkToggleButton *button,
                    gpointer        *user_data) {
    GSLockPlug *plug = GS_LOCK_PLUG (user_data);

    gboolean active = gtk_toggle_button_get_active (button);
    xfconf_channel_set_bool (plug->priv->channel, KEY_KEYBOARD_DISPLAYED, active);

    gtk_widget_grab_focus (plug->priv->auth_prompt_entry);
}

static gboolean
gs_lock_plug_add_login_window (GSLockPlug *plug) {
    GtkBuilder *builder;
    GtkWidget  *lock_overlay;
    GtkWidget  *lock_panel;
    GtkWidget  *lock_dialog;
    GError     *error = NULL;

    builder = gtk_builder_new();
    if (!gtk_builder_add_from_string (builder, xfce4_screensaver_dialog_ui,
                                      xfce4_screensaver_dialog_ui_length, &error)) {
        g_warning ("Error loading UI: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    plug->priv->prefs = gs_prefs_new ();
    plug->priv->channel = xfconf_channel_get (SETTINGS_XFCONF_CHANNEL);

    lock_overlay = GTK_WIDGET(gtk_builder_get_object(builder, "lock-overlay"));
    lock_panel = GTK_WIDGET(gtk_builder_get_object(builder, "lock-panel"));
    lock_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));

    gtk_widget_set_halign(GTK_WIDGET(lock_panel), GTK_ALIGN_FILL);
    gtk_widget_set_valign(GTK_WIDGET(lock_panel), GTK_ALIGN_START);
    gtk_overlay_add_overlay(GTK_OVERLAY(lock_overlay), GTK_WIDGET(lock_panel));

    gtk_widget_set_halign(GTK_WIDGET(lock_dialog), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(lock_dialog), GTK_ALIGN_CENTER);
    gtk_overlay_add_overlay(GTK_OVERLAY(lock_overlay), GTK_WIDGET(lock_dialog));

    gtk_container_add(GTK_CONTAINER(plug), lock_overlay);

    plug->priv->vbox = NULL;

    plug->priv->auth_face_image = GTK_WIDGET (gtk_builder_get_object(builder, "auth-face-image"));
    plug->priv->auth_action_area = GTK_WIDGET (gtk_builder_get_object(builder, "auth-action-area"));
    plug->priv->auth_datetime_label = GTK_WIDGET (gtk_builder_get_object(builder, "auth-date-time-label"));
    plug->priv->auth_realname_label = GTK_WIDGET (gtk_builder_get_object(builder, "auth-realname-label"));
    plug->priv->auth_username_label = GTK_WIDGET (gtk_builder_get_object(builder, "auth-hostname-label"));
    plug->priv->auth_prompt_entry = GTK_WIDGET (gtk_builder_get_object(builder, "auth-prompt-entry"));
    plug->priv->auth_unlock_button = GTK_WIDGET (gtk_builder_get_object(builder, "auth-unlock-button"));
    plug->priv->auth_cancel_button = GTK_WIDGET (gtk_builder_get_object(builder, "auth-cancel-button"));
    plug->priv->auth_logout_button = GTK_WIDGET (gtk_builder_get_object(builder, "auth-logout-button"));
    plug->priv->auth_switch_button = GTK_WIDGET (gtk_builder_get_object(builder, "auth-switch-button"));
    plug->priv->background_image = GTK_WIDGET (gtk_builder_get_object(builder, "lock-image"));

    plug->priv->auth_prompt_infobar = GTK_WIDGET (gtk_builder_get_object(builder, "greeter_infobar"));
    plug->priv->status_message_label = GTK_WIDGET (gtk_builder_get_object(builder, "status-message-label"));
    plug->priv->auth_prompt_label = GTK_WIDGET (gtk_builder_get_object(builder, "auth-prompt-label"));
    plug->priv->auth_capslock_label = GTK_WIDGET (gtk_builder_get_object(builder, "auth-capslock-label"));
    plug->priv->auth_message_label = GTK_WIDGET (gtk_builder_get_object(builder, "auth-status-label"));

    plug->priv->keyboard_toggle = GTK_WIDGET (gtk_builder_get_object(builder, "keyboard-toggle"));
    if (plug->priv->prefs->keyboard_enabled) {
        gtk_widget_show (plug->priv->keyboard_toggle);
        gtk_widget_set_no_show_all (plug->priv->keyboard_toggle, FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plug->priv->keyboard_toggle), plug->priv->prefs->keyboard_displayed);
        g_signal_connect (GTK_TOGGLE_BUTTON (plug->priv->keyboard_toggle), "toggled",
                          G_CALLBACK (keyboard_toggled_cb), plug);
    }

    /* Placeholder for the keyboard indicator */
    plug->priv->auth_prompt_kbd_layout_indicator = GTK_WIDGET (
            gtk_builder_get_object(builder, "auth-prompt-kbd-layout-indicator"));
    if (plug->priv->auth_logout_button != NULL) {
        gtk_widget_set_no_show_all (plug->priv->auth_logout_button, TRUE);
    }
    if (plug->priv->auth_switch_button != NULL) {
        gtk_widget_set_no_show_all (plug->priv->auth_switch_button, TRUE);
    }

    redraw_background (plug);

    date_time_update (plug);
    gtk_widget_show_all (lock_dialog);

    return TRUE;
}

static int
delete_handler (GSLockPlug  *plug,
                GdkEventAny *event,
                gpointer     data) {
    gs_lock_plug_response (plug, GS_LOCK_PLUG_RESPONSE_CANCEL);

    return TRUE; /* Do not destroy */
}

static void
gs_lock_plug_init (GSLockPlug *plug) {
    gs_profile_start (NULL);

    plug->priv = gs_lock_plug_get_instance_private (plug);

    clear_clipboards (plug);

    gs_lock_plug_add_login_window (plug);
    plug->priv->datetime_timeout_id = g_timeout_add_seconds (60, date_time_update, plug);

    /* Layout indicator */
#ifdef WITH_KBD_LAYOUT_INDICATOR
    if (plug->priv->auth_prompt_kbd_layout_indicator != NULL) {
        XklEngine *engine;

        engine = xkl_engine_get_instance (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
        if (xkl_engine_get_num_groups (engine) > 1) {
            GtkWidget *layout_indicator;

            layout_indicator = xfcekbd_indicator_new ();
            xfcekbd_indicator_set_parent_tooltips (XFCEKBD_INDICATOR (layout_indicator), TRUE);
            gtk_box_pack_start (GTK_BOX (plug->priv->auth_prompt_kbd_layout_indicator),
                                layout_indicator,
                                FALSE,
                                FALSE,
                                6);

            gtk_widget_show_all (layout_indicator);
            gtk_widget_show (plug->priv->auth_prompt_kbd_layout_indicator);
        } else {
            gtk_widget_hide (plug->priv->auth_prompt_kbd_layout_indicator);
        }

        g_object_unref (engine);
    }
#endif

    if (plug->priv->auth_switch_button != NULL) {
        if (plug->priv->switch_enabled) {
            gtk_widget_show_all (plug->priv->auth_switch_button);
        } else {
            gtk_widget_hide (plug->priv->auth_switch_button);
        }
    }

    gtk_widget_grab_default (plug->priv->auth_unlock_button);

    if (plug->priv->auth_username_label != NULL) {
        expand_string_for_label (plug->priv->auth_username_label);
    }

    if (plug->priv->auth_realname_label != NULL) {
        expand_string_for_label (plug->priv->auth_realname_label);
    }

    if (!plug->priv->logout_enabled || !plug->priv->logout_command) {
        if (plug->priv->auth_logout_button != NULL) {
            gtk_widget_hide (plug->priv->auth_logout_button);
        }
    }

    g_signal_connect (plug, "key_press_event",
                      G_CALLBACK (entry_key_press), plug);

    /* button press handler used to inhibit popup menu */
    g_signal_connect (plug->priv->auth_prompt_entry, "button_press_event",
                      G_CALLBACK (entry_button_press), NULL);
    gtk_entry_set_activates_default (GTK_ENTRY (plug->priv->auth_prompt_entry), TRUE);
    gtk_entry_set_visibility (GTK_ENTRY (plug->priv->auth_prompt_entry), FALSE);

    g_signal_connect (plug->priv->auth_unlock_button, "clicked",
                      G_CALLBACK (unlock_button_clicked), plug);

    g_signal_connect (plug->priv->auth_cancel_button, "clicked",
                      G_CALLBACK (cancel_button_clicked), plug);

    if (plug->priv->status_message_label) {
        if (plug->priv->status_message) {
            gtk_label_set_text (GTK_LABEL (plug->priv->status_message_label),
                                plug->priv->status_message);
            gtk_widget_show (plug->priv->status_message_label);
        } else {
            gtk_widget_hide (plug->priv->status_message_label);
        }
        toggle_infobar_visibility (plug);
    }

    if (plug->priv->auth_switch_button != NULL) {
        g_signal_connect (plug->priv->auth_switch_button, "clicked",
                          G_CALLBACK (switch_user_button_clicked), plug);
    }

    if (plug->priv->auth_logout_button != NULL) {
        g_signal_connect (plug->priv->auth_logout_button, "clicked",
                          G_CALLBACK (logout_button_clicked), plug);
    }

    g_signal_connect (plug, "delete_event", G_CALLBACK (delete_handler), NULL);

    gs_profile_end (NULL);
}

static void
gs_lock_plug_finalize (GObject *object) {
    GSLockPlug *plug;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_LOCK_PLUG (object));

    plug = GS_LOCK_PLUG (object);

    g_return_if_fail (plug->priv != NULL);

    g_free (plug->priv->logout_command);

    g_free (plug->priv->prefs);
    plug->priv->prefs = NULL;

    remove_response_idle (plug);
    remove_cancel_timeout (plug);
    remove_datetime_timeout (plug);

    G_OBJECT_CLASS (gs_lock_plug_parent_class)->finalize (object);
}

GtkWidget *
gs_lock_plug_new (void) {
    GtkWidget *result;

    result = g_object_new (GS_TYPE_LOCK_PLUG, NULL);

    gtk_window_set_focus_on_map (GTK_WINDOW (result), TRUE);

    return result;
}
