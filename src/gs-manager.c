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

#include <config.h>

#include <gdk/gdk.h>
#include <gio/gio.h>
#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include "gs-grab.h"
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include "gs-session-lock-manager.h"
#endif

#include "gs-debug.h"
#include "gs-job.h"
#include "gs-manager.h"
#include "gs-prefs.h"
#include "gs-window.h"

static void     gs_manager_finalize   (GObject        *object);

struct GSManagerPrivate {
    GHashTable     *windows;
    GHashTable     *jobs;
    GList          *overlays;
    guint           n_overlay_signal_received;

    /* Policy */
    GSPrefs         *prefs;
    guint           throttled : 1;
    char           *status_message;

    /* State */
    guint           active : 1;
    guint           lock_active : 1;

    guint           dialog_up : 1;

    guint           lock_timeout_id;
    guint           cycle_timeout_id;

#ifdef ENABLE_X11
    GSGrab         *grab;
#endif
#ifdef ENABLE_WAYLAND
    WleEmbeddedCompositor *compositor;
    GSSessionLockManager *lock_manager;
#endif
};

enum {
    ACTIVATED,
    DEACTIVATED,
    AUTH_REQUEST_BEGIN,
    AUTH_REQUEST_END,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_STATUS_MESSAGE,
    PROP_ACTIVE,
    PROP_THROTTLED,
};

static guint         signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GSManager, gs_manager, G_TYPE_OBJECT)

static void
manager_maybe_stop_job_for_window (GSManager *manager,
                                   GSWindow  *window) {
    GSJob *job;

    job = g_hash_table_lookup (manager->priv->jobs, window);

    if (job == NULL) {
        gs_debug ("Job not found for window");
        return;
    }

    gs_job_stop (job);
}

static void
manager_maybe_start_job_for_window (GSManager *manager,
                                    GSWindow  *window) {
    GSJob *job;

    job = g_hash_table_lookup (manager->priv->jobs, window);

    if (job == NULL) {
        gs_debug ("Job not found for window");
        return;
    }
    if (manager->priv->dialog_up) {
        gs_debug ("Not starting job because dialog is up");
        return;
    }
    if (manager->priv->throttled) {
        gs_debug ("Not starting job because throttled");
        return;
    }
    if (gs_job_is_running (job)) {
        gs_debug ("Not starting job because job is running");
        return;
    }
    if (gs_window_is_obscured (window)) {
        gs_debug ("Window is obscured deferring start of job");
        return;
    }
    gs_debug ("Starting job for window");
    gs_job_start (job);
}

static void
throttle_job (GSWindow  *window,
              GSJob     *job,
              GSManager *manager) {
    if (manager->priv->throttled) {
        gs_job_stop (job);
    } else {
        manager_maybe_start_job_for_window (manager, window);
    }
}

static void
resume_job (GSWindow  *window,
            GSJob     *job,
            GSManager *manager) {
    if (gs_job_is_running (job)) {
        gs_job_suspend (job, FALSE);
    } else {
        manager_maybe_start_job_for_window (manager, window);
    }
}

static void
suspend_job (GSWindow  *window,
             GSJob     *job,
             GSManager *manager) {
    gs_job_suspend (job, TRUE);
}

static void
remove_job (GSJob *job) {
    gs_job_stop (job);
    g_object_unref (job);
}

void
gs_manager_set_throttled (GSManager *manager,
                          gboolean   throttled) {
    g_return_if_fail (GS_IS_MANAGER (manager));

    if (manager->priv->throttled != throttled) {
        manager->priv->throttled = throttled;

        if (!manager->priv->dialog_up) {
            GHashTableIter iter;
            gpointer window;

            g_hash_table_foreach (manager->priv->jobs, (GHFunc) throttle_job, manager);
            g_hash_table_iter_init (&iter, manager->priv->windows);
            while (g_hash_table_iter_next (&iter, NULL, &window)) {
                gs_window_clear (window);
            }
        }
    }
}

void
gs_manager_enable_locker (GSManager *manager,
                          gboolean   lock_active) {
    GHashTableIter iter;
    gpointer window;

    g_return_if_fail (GS_IS_MANAGER (manager));

    if (manager->priv->lock_active != lock_active) {
        manager->priv->lock_active = lock_active;
    }

    g_hash_table_iter_init (&iter, manager->priv->windows);
    while (g_hash_table_iter_next (&iter, NULL, &window)) {
        gs_debug ("Setting lock active: %d", lock_active);
        gs_window_set_lock_active (window, lock_active);
    }
}

static gboolean
activate_lock_timeout (gpointer user_data) {
    GSManager *manager = user_data;

    gs_debug ("Locking screen on idle timeout");
    gs_manager_enable_locker (manager, TRUE);
    manager->priv->lock_timeout_id = 0;

    return FALSE;
}

static void
remove_lock_timer (GSManager *manager) {
    if (manager->priv->lock_timeout_id != 0) {
        g_source_remove (manager->priv->lock_timeout_id);
        manager->priv->lock_timeout_id = 0;
    }
}

static void
add_lock_timer (GSManager *manager,
                guint      seconds) {
    if (!manager->priv->prefs->lock_enabled)
        return;
    if (!manager->priv->prefs->lock_with_saver_enabled)
        return;
    if (manager->priv->lock_active)
        return;

    gs_debug ("Scheduling screen lock after screensaver is idling for %u sec", seconds * 60);
    manager->priv->lock_timeout_id = g_timeout_add_seconds (seconds * 60, activate_lock_timeout, manager);
}

void
gs_manager_set_status_message (GSManager  *manager,
                               const char *message) {
    GHashTableIter iter;
    gpointer window;

    g_return_if_fail (GS_IS_MANAGER (manager));

    g_free (manager->priv->status_message);
    manager->priv->status_message = g_strdup (message);

    g_hash_table_iter_init (&iter, manager->priv->windows);
    while (g_hash_table_iter_next (&iter, NULL, &window)) {
        gs_window_set_status_message (window, manager->priv->status_message);
    }
}

gboolean
gs_manager_cycle (GSManager *manager) {
    GHashTableIter iter;
    gpointer window;

    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    if (!manager->priv->active) {
        return FALSE;
    }
    if (manager->priv->dialog_up) {
        return FALSE;
    }
    if (manager->priv->throttled) {
        return FALSE;
    }
    gs_debug ("Cycling jobs");
    g_hash_table_iter_init (&iter, manager->priv->jobs);
    while (g_hash_table_iter_next (&iter, &window, NULL)) {
        GSJob *job = gs_job_new_for_widget (gs_window_get_drawing_area (window));
        g_hash_table_iter_replace (&iter, job);
        manager_maybe_start_job_for_window (manager, window);
    }

    return TRUE;
}

static gboolean
cycle_timeout (gpointer user_data) {
    GSManager *manager = user_data;

    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    if (!manager->priv->dialog_up) {
        gs_manager_cycle (manager);
    }

    return TRUE;
}

static void
remove_cycle_timer (GSManager *manager) {
    if (manager->priv->cycle_timeout_id != 0) {
        g_source_remove (manager->priv->cycle_timeout_id);
        manager->priv->cycle_timeout_id = 0;
    }
}

static void
add_cycle_timer (GSManager *manager,
                 glong      seconds) {
    manager->priv->cycle_timeout_id = g_timeout_add_seconds (seconds, cycle_timeout, manager);
}

static void
gs_manager_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec) {
    GSManager *self;

    self = GS_MANAGER (object);

    switch (prop_id) {
        case PROP_THROTTLED:
            gs_manager_set_throttled (self, g_value_get_boolean (value));
            break;
        case PROP_STATUS_MESSAGE:
            gs_manager_set_status_message (self, g_value_get_string (value));
            break;
        case PROP_ACTIVE:
            gs_manager_activate_saver (self, g_value_get_boolean (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_manager_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec) {
    GSManager *self;

    self = GS_MANAGER (object);

    switch (prop_id) {
        case PROP_THROTTLED:
            g_value_set_boolean (value, self->priv->throttled);
            break;
        case PROP_STATUS_MESSAGE:
            g_value_set_string (value, self->priv->status_message);
            break;
        case PROP_ACTIVE:
            g_value_set_boolean (value, self->priv->active);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_manager_class_init (GSManagerClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize     = gs_manager_finalize;
    object_class->get_property = gs_manager_get_property;
    object_class->set_property = gs_manager_set_property;

    signals[ACTIVATED] =
        g_signal_new ("activated",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSManagerClass, activated),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[DEACTIVATED] =
        g_signal_new ("deactivated",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSManagerClass, deactivated),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[AUTH_REQUEST_BEGIN] =
        g_signal_new ("auth-request-begin",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSManagerClass, auth_request_begin),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[AUTH_REQUEST_END] =
        g_signal_new ("auth-request-end",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSManagerClass, auth_request_end),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    g_object_class_install_property (object_class,
                                     PROP_ACTIVE,
                                     g_param_spec_boolean ("active",
                                                           NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READABLE));
    g_object_class_install_property (object_class,
                                     PROP_THROTTLED,
                                     g_param_spec_boolean ("throttled",
                                                           NULL,
                                                           NULL,
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                 PROP_STATUS_MESSAGE,
                                 g_param_spec_string ("status-message",
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      G_PARAM_READWRITE));
}

static void
gs_manager_init (GSManager *manager) {
    manager->priv = gs_manager_get_instance_private (manager);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        manager->priv->grab = gs_grab_new ();
    }
#endif
    manager->priv->prefs = gs_prefs_new();
    manager->priv->jobs = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                 NULL, (GDestroyNotify) remove_job);
    manager->priv->windows = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                    NULL, (GDestroyNotify) gs_window_destroy);

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
        GError *error = NULL;
        manager->priv->compositor = wle_embedded_compositor_new ("xfce4-screensaver", wl_display, &error);
        if (manager->priv->compositor == NULL) {
            g_critical ("Failed to create embedded compositor: %s", error->message);
            g_error_free (error);
        }

        manager->priv->lock_manager = gs_session_lock_manager_new ();
    }
#endif
}

static void
remove_timers (GSManager *manager) {
    remove_lock_timer (manager);
    remove_cycle_timer (manager);
}

static gboolean
window_deactivated_idle (gpointer user_data) {
    GSManager *manager = user_data;
    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    /* don't deactivate directly but only emit a signal
       so that we let the parent deactivate */
    g_signal_emit (manager, signals[DEACTIVATED], 0);

    return FALSE;
}

static void
window_deactivated_cb (GSWindow  *window,
                       GSManager *manager) {
    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    g_idle_add (window_deactivated_idle, manager);
}

static GSWindow *
find_window_at_pointer (GSManager *manager) {
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        GdkDisplay *display = gdk_display_get_default ();
        GdkDevice *device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
        GdkMonitor *monitor;
        int x, y;

        gdk_device_get_position (device, NULL, &x, &y);
        monitor = gdk_display_get_monitor_at_point (display, x, y);
        return g_hash_table_lookup (manager->priv->windows, monitor);
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        GHashTableIter iter;
        gpointer window;

        g_hash_table_iter_init (&iter, manager->priv->windows);
        while (g_hash_table_iter_next (&iter, NULL, &window)) {
            if (gtk_window_is_active (GTK_WINDOW (window))) {
                break;
            }
        }
        return window;
    }
#endif

    return NULL;
}

void
gs_manager_show_message (GSManager  *manager,
                         const char *summary,
                         const char *body,
                         const char *icon) {
    GSWindow *window;

    g_return_if_fail (GS_IS_MANAGER (manager));

    /* Find the GSWindow that contains the pointer */
    window = find_window_at_pointer (manager);
    if (window == NULL)
        return;

    gs_window_show_message (window, summary, body, icon);
    gs_manager_request_unlock (manager);
}

#ifdef ENABLE_X11
static gboolean
manager_maybe_grab_window (GSManager *manager,
                           GSWindow  *window) {
    GdkDisplay *display;
    GdkDevice  *device;
    GdkMonitor *monitor;
    int         x, y;
    gboolean    grabbed;

    display = gdk_display_get_default ();
    device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
    gdk_device_get_position (device, NULL, &x, &y);
    monitor = gdk_display_get_monitor_at_point (display, x, y);

    gdk_display_flush (display);
    grabbed = FALSE;
    if (gs_window_get_display (window) == display &&
            gs_window_get_monitor (window) == monitor) {
        gs_debug ("Initiate grab move to %p", window);
        gs_grab_move_to_window (manager->priv->grab,
                                gs_window_get_gdk_window (window),
                                gs_window_get_display (window),
                                FALSE, FALSE);
        grabbed = TRUE;
    }

    return grabbed;
}

static void
window_grab_broken_cb (GSWindow           *window,
                       GdkEventGrabBroken *event,
                       GSManager          *manager) {
    GdkDisplay *display;
    GdkSeat    *seat;
    GdkDevice  *device;

    display = gdk_window_get_display (gs_window_get_gdk_window (window));
    seat = gdk_display_get_default_seat (display);

    if (event->keyboard) {
        gs_debug ("KEYBOARD GRAB BROKEN!");
        device = gdk_seat_get_pointer (seat);
        if (!gdk_display_device_is_grabbed (display, device))
            gs_grab_reset (manager->priv->grab);
    } else {
        gs_debug ("POINTER GRAB BROKEN!");
        device = gdk_seat_get_keyboard (seat);
        if (!gdk_display_device_is_grabbed (display, device))
            gs_grab_reset (manager->priv->grab);
    }
}
#endif

static gboolean
window_map_event_cb (GSWindow  *window,
                     GdkEvent  *event,
                     GSManager *manager) {
    gs_debug ("Handling window map_event event");
#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        manager_maybe_grab_window (manager, window);
    }
#endif
    manager_maybe_start_job_for_window (manager, window);
    return FALSE;
}

static void
manager_show_window (GSManager *manager,
                     GSWindow  *window) {
    GSJob *job;

    job = gs_job_new_for_widget (gs_window_get_drawing_area (window));

    g_hash_table_insert (manager->priv->jobs, window, job);

    remove_lock_timer (manager);
    add_lock_timer (manager, manager->priv->prefs->lock_timeout);

    if (manager->priv->prefs->mode == GS_MODE_RANDOM) {
        remove_cycle_timer (manager);
        add_cycle_timer (manager, manager->priv->prefs->cycle);
    }

    /* FIXME: only emit signal once */
    g_signal_emit (manager, signals[ACTIVATED], 0);
}

static void
window_show_cb (GSWindow  *window,
                GSManager *manager) {
    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));
    g_return_if_fail (window != NULL);
    g_return_if_fail (GS_IS_WINDOW (window));

    gs_debug ("Handling window show");
    manager_show_window (manager, window);
}

static void
window_obscured_cb (GSWindow   *window,
                    GParamSpec *pspec,
                    GSManager  *manager) {
    gboolean obscured;

    obscured = gs_window_is_obscured (window);
    gs_debug ("Handling window obscured: %s", obscured ? "obscured" : "unobscured");

    if (obscured) {
        manager_maybe_stop_job_for_window (manager, window);
    } else {
        manager_maybe_start_job_for_window (manager, window);
    }
}

static void
handle_window_dialog_up (GSManager *manager,
                         GSWindow  *window) {
    GHashTableIter iter;
    gpointer pwindow;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    gs_debug ("Handling dialog up");

    g_signal_emit (manager, signals[AUTH_REQUEST_BEGIN], 0);

    manager->priv->dialog_up = TRUE;
    /* make all other windows insensitive to not get events */
    g_hash_table_iter_init (&iter, manager->priv->windows);
    while (g_hash_table_iter_next (&iter, NULL, &pwindow)) {
        if (pwindow != window) {
            gtk_widget_set_sensitive (GTK_WIDGET (pwindow), FALSE);
        }
    }

#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        /* move devices grab so that dialog can be used;
           release the pointer grab while dialog is up so that
           the dialog can be used. We'll regrab it when the dialog goes down */
        gs_debug ("Initiate pointer-less grab move to %p", window);
        gs_grab_move_to_window (manager->priv->grab,
                                gs_window_get_gdk_window (window),
                                gs_window_get_display (window),
                                TRUE, FALSE);
    }
#endif

    if (!manager->priv->throttled) {
        gs_debug ("Suspending jobs");
        g_hash_table_foreach (manager->priv->jobs, (GHFunc) suspend_job, manager);
    }
}

static void
handle_window_dialog_down (GSManager *manager,
                           GSWindow  *window) {
    GHashTableIter iter;
    gpointer pwindow;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    gs_debug ("Handling dialog down");

#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        /* regrab pointer */
        gs_grab_move_to_window (manager->priv->grab,
                                gs_window_get_gdk_window (window),
                                gs_window_get_display (window),
                                FALSE, FALSE);
    }
#endif

    /* make all windows sensitive to get events */
    g_hash_table_iter_init (&iter, manager->priv->windows);
    while (g_hash_table_iter_next (&iter, NULL, &pwindow)) {
        gtk_widget_set_sensitive (GTK_WIDGET (pwindow), TRUE);
    }

    manager->priv->dialog_up = FALSE;

    if (!manager->priv->throttled) {
        g_hash_table_foreach (manager->priv->jobs, (GHFunc) resume_job, manager);
    }

    g_signal_emit (manager, signals[AUTH_REQUEST_END], 0);
}

static void
window_dialog_up_changed_cb (GSWindow   *window,
                             GParamSpec *pspec,
                             GSManager  *manager) {
    gboolean up;

    up = gs_window_is_dialog_up (window);
    gs_debug ("Handling window dialog up changed: %s", up ? "up" : "down");
    if (up) {
        handle_window_dialog_up (manager, window);
    } else {
        handle_window_dialog_down (manager, window);
    }
}

static gboolean
window_activity_cb (GSWindow  *window,
                    GSManager *manager) {
    gboolean handled;
    handled = gs_manager_request_unlock (manager);

    return handled;
}

static void
disconnect_window_signals (GSManager *manager,
                           GSWindow  *window) {
    g_signal_handlers_disconnect_by_func (window, window_deactivated_cb, manager);
    g_signal_handlers_disconnect_by_func (window, window_activity_cb, manager);
    g_signal_handlers_disconnect_by_func (window, window_show_cb, manager);
    g_signal_handlers_disconnect_by_func (window, window_map_event_cb, manager);
    g_signal_handlers_disconnect_by_func (window, window_obscured_cb, manager);
    g_signal_handlers_disconnect_by_func (window, window_dialog_up_changed_cb, manager);
#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        g_signal_handlers_disconnect_by_func (window, window_grab_broken_cb, manager);
    }
#endif
}

static void
window_destroyed_cb (GtkWindow *window,
                     GSManager *manager) {
    disconnect_window_signals (manager, GS_WINDOW (window));
}

static void
connect_window_signals (GSManager *manager,
                        GSWindow  *window) {
    g_signal_connect_object (window, "destroy",
                             G_CALLBACK (window_destroyed_cb), manager, 0);
    g_signal_connect_object (window, "activity",
                             G_CALLBACK (window_activity_cb), manager, 0);
    g_signal_connect_object (window, "deactivated",
                             G_CALLBACK (window_deactivated_cb), manager, 0);
    g_signal_connect_object (window, "show",
                             G_CALLBACK (window_show_cb), manager, G_CONNECT_AFTER);
    g_signal_connect_object (window, "map_event",
                             G_CALLBACK (window_map_event_cb), manager, G_CONNECT_AFTER);
    g_signal_connect_object (window, "notify::obscured",
                             G_CALLBACK (window_obscured_cb), manager, G_CONNECT_AFTER);
    g_signal_connect_object (window, "notify::dialog-up",
                             G_CALLBACK (window_dialog_up_changed_cb), manager, 0);
#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        g_signal_connect_object (window, "grab-broken-event",
                                 G_CALLBACK (window_grab_broken_cb), manager, G_CONNECT_AFTER);
    }
#endif
}


static GtkWidget *
gs_manager_create_window_for_monitor (GSManager  *manager,
                                      GdkMonitor *monitor) {
    GSWindow    *window;
    GdkRectangle rect;

    gdk_monitor_get_geometry (monitor, &rect);

    gs_debug ("Creating a Window [%d,%d] (%dx%d) for monitor %s",
              rect.x, rect.y, rect.width, rect.height, gdk_monitor_get_model (monitor));

    window = gs_window_new (monitor);
    gs_window_set_status_message (window, manager->priv->status_message);
    gs_window_set_lock_active (window, manager->priv->lock_active);
    connect_window_signals (manager, window);
#ifdef ENABLE_WAYLAND
    if (manager->priv->lock_manager != NULL) {
        gs_session_lock_manager_add_window (manager->priv->lock_manager, window);
    }
#endif

    g_hash_table_insert (manager->priv->windows, monitor, window);

    if (manager->priv->active) {
        gtk_widget_show (GTK_WIDGET (window));
    }

    return GTK_WIDGET (window);
}

#ifdef ENABLE_WAYLAND
static void
remove_window (gpointer monitor,
               gpointer window,
               gpointer data) {
    GSManager *manager = data;
    gs_session_lock_manager_remove_window (manager->priv->lock_manager, window);
}
#endif

static gboolean
remove_overlays (GtkWidget *window,
                 cairo_t *cr,
                 GSManager *manager) {
    GdkDisplay *display = gdk_display_get_default ();
    guint n_monitors = gdk_display_get_n_monitors (display);

    g_signal_handlers_disconnect_by_func (window, remove_overlays, manager);
    if (++manager->priv->n_overlay_signal_received < n_monitors) {
        return FALSE;
    }

    gs_debug("Done reconfiguring monitors, removing overlays");

    g_list_free_full (manager->priv->overlays, (GDestroyNotify) gtk_widget_destroy);
    manager->priv->overlays = NULL;

    return FALSE;
}

static gboolean
recreate_windows (GtkWidget *overlay,
                  cairo_t *cr,
                  GSManager *manager) {
    GdkDisplay *display = gdk_display_get_default ();
    guint n_monitors = gdk_display_get_n_monitors (display);

    g_signal_handlers_disconnect_by_func (overlay, recreate_windows, manager);
    if (++manager->priv->n_overlay_signal_received < n_monitors) {
        return FALSE;
    }

    gs_debug("Reconfiguring monitors, recreating windows");

    g_hash_table_remove_all (manager->priv->jobs);
#ifdef ENABLE_WAYLAND
    if (manager->priv->lock_manager != NULL) {
        g_hash_table_foreach (manager->priv->windows, remove_window, manager);
    }
#endif
    g_hash_table_remove_all (manager->priv->windows);
    manager->priv->n_overlay_signal_received = 0;

    for (guint n = 0; n < n_monitors; n++) {
        GdkMonitor *monitor = gdk_display_get_monitor (display, n);
        GtkWidget *window = gs_manager_create_window_for_monitor (manager, monitor);
        g_signal_connect_after (window, "draw", G_CALLBACK (remove_overlays), manager);
    }

    return FALSE;
}

static void
add_overlays (GSManager *manager) {
    GdkDisplay *display = gdk_display_get_default ();
    gint n_monitors = gdk_display_get_n_monitors (display);
    GHashTableIter iter;
    gpointer window;

    gs_debug("Reconfiguring monitors, adding overlays");

    g_list_free_full (manager->priv->overlays, (GDestroyNotify) gtk_widget_destroy);
    manager->priv->overlays = NULL;
    manager->priv->n_overlay_signal_received = 0;
    g_hash_table_iter_init (&iter, manager->priv->windows);
    while (g_hash_table_iter_next (&iter, NULL, &window)) {
        g_signal_handlers_disconnect_by_func (window, remove_overlays, manager);
    }

    for (gint n = 0; n < n_monitors; n++) {
        GdkRectangle rect;
        GtkWidget *overlay = gtk_window_new (GTK_WINDOW_POPUP);
        gdk_monitor_get_geometry (gdk_display_get_monitor (display, n), &rect);
        gtk_window_move (GTK_WINDOW (overlay), rect.x, rect.y);
        gtk_window_resize (GTK_WINDOW (overlay), rect.width, rect.height);
        g_signal_connect_after (overlay, "draw", G_CALLBACK (recreate_windows), manager);
        gtk_widget_show (overlay);
        manager->priv->overlays = g_list_prepend (manager->priv->overlays, overlay);
    }
}

static void
on_display_monitor_added (GdkDisplay *display,
                          GdkMonitor *monitor,
                          GSManager  *manager) {
    gs_debug ("Monitor %s added on display %s, now there are %d",
              gdk_monitor_get_model (monitor), gdk_display_get_name (display), gdk_display_get_n_monitors (display));

    add_overlays (manager);
}

static void
on_display_monitor_removed (GdkDisplay *display,
                            GdkMonitor *monitor,
                            GSManager  *manager) {
    gs_debug ("Monitor %s removed on display %s, now there are %d",
              gdk_monitor_get_model (monitor), gdk_display_get_name (display), gdk_display_get_n_monitors (display));

    add_overlays (manager);
}

static void
gs_manager_destroy_windows (GSManager *manager) {
    GdkDisplay *display = gdk_display_get_default ();

    g_return_if_fail (GS_IS_MANAGER (manager));

    g_signal_handlers_disconnect_by_func (display,
                                          on_display_monitor_removed,
                                          manager);
    g_signal_handlers_disconnect_by_func (display,
                                          on_display_monitor_added,
                                          manager);

#ifdef ENABLE_WAYLAND
    if (manager->priv->lock_manager != NULL) {
        g_hash_table_foreach (manager->priv->windows, remove_window, manager);
    }
#endif
    g_hash_table_remove_all (manager->priv->windows);
    g_list_free_full (manager->priv->overlays, (GDestroyNotify) gtk_widget_destroy);
    manager->priv->overlays = NULL;
}

static void
gs_manager_finalize (GObject *object) {
    GSManager *manager;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_MANAGER (object));

    manager = GS_MANAGER (object);

    g_return_if_fail (manager->priv != NULL);

    remove_timers(manager);
#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        gs_grab_release (manager->priv->grab, TRUE);
        g_object_unref (manager->priv->grab);
    }
#endif
    g_hash_table_destroy (manager->priv->jobs);
    gs_manager_destroy_windows (manager);
    g_hash_table_destroy (manager->priv->windows);

    manager->priv->active = FALSE;

    g_object_unref (manager->priv->prefs);

#ifdef ENABLE_WAYLAND
    if (manager->priv->compositor != NULL) {
        g_object_unref (manager->priv->compositor);
    }
    if (manager->priv->lock_manager != NULL) {
        g_object_unref (manager->priv->lock_manager);
    }
#endif

    G_OBJECT_CLASS (gs_manager_parent_class)->finalize (object);
}

static void
gs_manager_create_windows (GSManager *manager) {
    GdkDisplay *display = gdk_display_get_default ();
    gint n_monitors = gdk_display_get_n_monitors (display);

    g_return_if_fail (GS_IS_MANAGER (manager));

    g_signal_connect (display, "monitor-added",
                      G_CALLBACK (on_display_monitor_added),
                      manager);
    g_signal_connect (display, "monitor-removed",
                      G_CALLBACK (on_display_monitor_removed),
                      manager);

    for (gint n = 0; n < n_monitors; n++) {
        GdkMonitor *monitor = gdk_display_get_monitor (display, n);
        gs_manager_create_window_for_monitor (manager, monitor);
    }
}

GSManager *
gs_manager_new (void) {
    static GSManager *manager = NULL;
    if (manager == NULL) {
        manager = g_object_new (GS_TYPE_MANAGER, NULL);
    } else {
        g_object_ref (manager);
    }
    return manager;
}

static void
show_windows (GHashTable *windows) {
    GHashTableIter iter;
    gpointer window;

    g_hash_table_iter_init (&iter, windows);
    while (g_hash_table_iter_next (&iter, NULL, &window)) {
        gtk_widget_show (GTK_WIDGET (window));
    }
}

static gboolean
gs_manager_activate (GSManager *manager) {
    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    if (manager->priv->active) {
        gs_debug ("Trying to activate manager when already active");
        return FALSE;
    }

#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        if (!gs_grab_grab_root (manager->priv->grab, FALSE, FALSE)) {
            return FALSE;
        }
    }
#endif

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        if (manager->priv->compositor == NULL
            || manager->priv->lock_manager == NULL
            || !gs_session_lock_manager_lock (manager->priv->lock_manager)) {
            return FALSE;
        }
    }
#endif

    gs_manager_create_windows (GS_MANAGER (manager));

    manager->priv->active = TRUE;

    show_windows (manager->priv->windows);

    return TRUE;
}

static gboolean
gs_manager_deactivate (GSManager *manager) {
    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    if (!manager->priv->active) {
        gs_debug ("Trying to deactivate a screensaver that is not active");
        return FALSE;
    }

    remove_timers (manager);
#ifdef ENABLE_X11
    if (manager->priv->grab != NULL) {
        gs_grab_release (manager->priv->grab, TRUE);
    }
#endif
    g_hash_table_remove_all (manager->priv->jobs);
    gs_manager_destroy_windows (manager);
#ifdef ENABLE_WAYLAND
    if (manager->priv->lock_manager != NULL) {
        gs_session_lock_manager_unlock (manager->priv->lock_manager);
    }
#endif

    /* reset state */
    manager->priv->active = FALSE;
    manager->priv->dialog_up = FALSE;

    gs_manager_enable_locker (manager, FALSE);

    return TRUE;
}

gboolean
gs_manager_activate_saver (GSManager *manager,
                           gboolean   active) {
    if (active) {
        return gs_manager_activate (manager);
    }
    return gs_manager_deactivate (manager);
}

gboolean
gs_manager_request_unlock (GSManager *manager) {
    GSWindow *window;

    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    if (!manager->priv->active) {
        gs_debug ("Request unlock but manager is not active");
        return FALSE;
    }

    if (manager->priv->dialog_up) {
        gs_debug ("Request unlock but dialog is already up");
        return FALSE;
    }

    if (g_hash_table_size (manager->priv->windows) == 0) {
        gs_debug ("Request unlock but we don't have any windows!");
        return FALSE;
    }

    /* Find the GSWindow that contains the pointer */
    window = find_window_at_pointer (manager);
    if (window == NULL) {
        gs_debug ("Request unlock but no window could be found!");
        return FALSE;
    }

    gs_window_request_unlock (window);

    return TRUE;
}

#ifdef ENABLE_WAYLAND
WleEmbeddedCompositor *
gs_manager_get_compositor (GSManager *manager) {
    return manager->priv->compositor;
}
#endif
