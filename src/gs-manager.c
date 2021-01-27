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
#include <gdk/gdkx.h>
#include <gio/gio.h>

#include <X11/extensions/dpms.h>

#include "gs-debug.h"
#include "gs-grab.h"
#include "gs-job.h"
#include "gs-manager.h"
#include "gs-prefs.h"
#include "gs-window.h"

static void     gs_manager_finalize   (GObject        *object);

static void     remove_dpms_timer     (GSManager      *manager);
static void     add_dpms_timer        (GSManager      *manager,
                                       glong           timeout);

struct GSManagerPrivate {
    GSList         *windows;
    GHashTable     *jobs;

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
    guint           dpms_timeout_id;

    GSGrab         *grab;
    guint           deepsleep_idle_id;
    gboolean        deepsleep;
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

static void         remove_deepsleep_idle   (GSManager *manager);
static void         add_deepsleep_idle      (GSManager *manager);
static void
gs_manager_create_windows_for_display       (GSManager  *manager,
                                             GdkDisplay *display);

static gboolean
gs_manager_is_real_monitor (GdkMonitor *monitor) {
    // avoiding some weird gdk bug
    // federico> avb: or if you don't care about a little unexplained messiness,
    //                just discard monitors where both fields are null? :)
    if (gdk_monitor_get_manufacturer(monitor) == NULL && gdk_monitor_get_model(monitor) == NULL)
        return FALSE;
    return TRUE;
}

static int
gs_manager_get_n_monitors (GdkDisplay *display) {
    // Since gdk_display_get_n_monitors return wrong monitor count
    // this is a workaround for the problem
    int n_monitors;
    int i, count = 0;

    n_monitors = gdk_display_get_n_monitors (display);
    for (i = 0; i < n_monitors; i++) {
        GdkMonitor *monitor = gdk_display_get_monitor(display, i);
        if (gs_manager_is_real_monitor (monitor))
            count++;
    }
    return count;
}

static gint
manager_get_monitor_index (GdkMonitor *this_monitor) {
    GdkDisplay *display;
    gint        idx;

    if (!gs_manager_is_real_monitor (this_monitor))
        return -1;

    display = gdk_monitor_get_display (this_monitor);

    for (idx = 0; idx < gdk_display_get_n_monitors (display); idx++) {
        GdkMonitor *monitor = gdk_display_get_monitor (display, idx);
        if (monitor == this_monitor)
            return idx;
    }

    return -1;
}

static void
manager_add_job_for_window (GSManager *manager,
                            GSWindow  *window,
                            GSJob     *job) {
    if (manager->priv->jobs == NULL) {
        return;
    }

    g_hash_table_insert (manager->priv->jobs, window, job);
}

static GSJob *
lookup_job_for_window (GSManager *manager,
                       GSWindow  *window) {
    GSJob *job;

    if (manager->priv->jobs == NULL) {
        return NULL;
    }

    job = g_hash_table_lookup (manager->priv->jobs, window);

    return job;
}

static void
manager_maybe_stop_job_for_window (GSManager *manager,
                                   GSWindow  *window) {
    GSJob *job;

    job = lookup_job_for_window (manager, window);

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

    job = lookup_job_for_window (manager, window);

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
cycle_job (GSWindow  *window,
           GSJob     *job,
           GSManager *manager) {
    gs_job_stop (job);
    manager_maybe_start_job_for_window (manager, window);
}

static void
manager_cycle_jobs (GSManager *manager) {
    if (manager->priv->jobs != NULL) {
        g_hash_table_foreach (manager->priv->jobs, (GHFunc) cycle_job, manager);
    }
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
manager_throttle_jobs (GSManager *manager) {
    if (manager->priv->jobs != NULL) {
        g_hash_table_foreach (manager->priv->jobs, (GHFunc) throttle_job, manager);
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
manager_resume_jobs (GSManager *manager) {
    if (manager->priv->jobs != NULL) {
        g_hash_table_foreach (manager->priv->jobs, (GHFunc) resume_job, manager);
    }
}

static void
suspend_job (GSWindow  *window,
             GSJob     *job,
             GSManager *manager) {
    gs_job_suspend (job, TRUE);
}

static void
manager_suspend_jobs (GSManager *manager) {
    if (manager->priv->jobs != NULL) {
        g_hash_table_foreach (manager->priv->jobs, (GHFunc) suspend_job, manager);
    }
}

static void
manager_stop_jobs (GSManager *manager) {
    if (manager->priv->jobs != NULL) {
        g_hash_table_destroy (manager->priv->jobs);
    }
    manager->priv->jobs = NULL;
}

void
gs_manager_set_throttled (GSManager *manager,
                          gboolean   throttled) {
    g_return_if_fail (GS_IS_MANAGER (manager));

    if (manager->priv->throttled != throttled) {
        GSList *l;

        manager->priv->throttled = throttled;

        if (!manager->priv->dialog_up) {
            manager_throttle_jobs (manager);

            for (l = manager->priv->windows; l; l = l->next) {
                gs_window_clear (l->data);
            }
        }
    }
}

void
gs_manager_enable_locker (GSManager *manager,
                          gboolean   lock_active) {
    GSList *l;

    g_return_if_fail (GS_IS_MANAGER (manager));


    if (manager->priv->lock_active != lock_active) {
        manager->priv->lock_active = lock_active;
    }

    for (l = manager->priv->windows; l; l = l->next) {
        gs_debug ("Setting lock active: %d", lock_active);
        gs_window_set_lock_active(l->data, lock_active);
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
    GSList *l;

    g_return_if_fail (GS_IS_MANAGER (manager));

    g_free (manager->priv->status_message);

    manager->priv->status_message = g_strdup (message);

    for (l = manager->priv->windows; l; l = l->next) {
        gs_window_set_status_message (l->data, manager->priv->status_message);
    }
}

gboolean
gs_manager_cycle (GSManager *manager) {
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
    manager_cycle_jobs (manager);

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

    manager->priv->grab = gs_grab_new ();

    manager->priv->prefs = gs_prefs_new();

    add_deepsleep_idle(manager);
}

static gboolean
activate_dpms_timeout (gpointer user_data) {
    GSManager *manager = user_data;
    BOOL state;
    CARD16 power_level;

    if (manager->priv->active) {
        if (DPMSInfo(gdk_x11_get_default_xdisplay(), &power_level, &state)) {
            if (state) {
                if (power_level == DPMSModeOn) {
                    gs_debug("DPMS: On -> Standby");
                    DPMSForceLevel (gdk_x11_get_default_xdisplay(), DPMSModeStandby);
                    remove_dpms_timer (manager);
                    add_dpms_timer (manager, manager->priv->prefs->dpms_off_timeout * 60);
                    return FALSE;
                } else if (power_level == DPMSModeStandby || power_level == DPMSModeSuspend) {
                    gs_debug("DPMS: %s -> Off", power_level == DPMSModeStandby ? "Standby" : "Suspend");
                    DPMSForceLevel (gdk_x11_get_default_xdisplay(), DPMSModeOff);
                }
            }
        }
    }

    manager->priv->dpms_timeout_id = 0;
    return FALSE;
}

static void
remove_dpms_timer (GSManager *manager) {
    if (manager->priv->dpms_timeout_id != 0) {
        g_source_remove (manager->priv->dpms_timeout_id);
        manager->priv->dpms_timeout_id = 0;
    }
}

static void
add_dpms_timer (GSManager *manager,
                glong      timeout) {
    if (manager->priv->prefs->mode != GS_MODE_BLANK_ONLY)
        return;

    if (timeout == 0)
        return;

    gs_debug ("Scheduling DPMS change after screensaver is idling for %i seconds(s)", timeout);
    manager->priv->dpms_timeout_id = g_timeout_add_seconds (timeout, activate_dpms_timeout, manager);
}

static void
remove_timers (GSManager *manager) {
    remove_lock_timer (manager);
    remove_cycle_timer (manager);
    remove_dpms_timer (manager);
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
    GdkDisplay *display;
    GdkDevice  *device;
    GdkMonitor *monitor;
    int         x, y;
    GSList     *window = NULL;

    display = gdk_display_get_default ();

    device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
    gdk_device_get_position (device, NULL, &x, &y);
    monitor = gdk_display_get_monitor_at_point (display, x, y);

    /* Find the gs-window that is on that monitor */
    if (gs_manager_is_real_monitor (monitor))
        window = g_slist_nth (manager->priv->windows, manager_get_monitor_index (monitor));

    if (window == NULL) {
        gs_debug ("WARNING: Could not find the GSWindow for display %s",
                  gdk_display_get_name (display));

        /* bail if there are no windows available */
        if (manager->priv->windows == NULL)
            return NULL;

        /* take the first one */
        window = manager->priv->windows->data;
        if (window->data == NULL || !GS_IS_WINDOW (GS_WINDOW (window->data))) {
            return NULL;
        }
    } else {
        gs_debug ("Requesting unlock for display %s",
                  gdk_display_get_name (display));
    }

    return GS_WINDOW (window->data);
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

static void
remove_deepsleep_idle (GSManager *manager) {
    if (manager->priv->deepsleep_idle_id > 0) {
        g_source_remove (manager->priv->deepsleep_idle_id);
        manager->priv->deepsleep_idle_id = 0;
    }
}

static gboolean
deepsleep_idle (gpointer user_data) {
    GSManager *manager = user_data;
    BOOL state;
    CARD16 power_level;

    if (!DPMSInfo(gdk_x11_get_default_xdisplay(), &power_level, &state)) {
        if (manager->priv->deepsleep) {
            gs_debug ("Unable to read DPMS state, exiting deep sleep");
            manager->priv->deepsleep = FALSE;
        }
        return TRUE;
    }

    if (power_level == DPMSModeOn) {
        if (manager->priv->deepsleep) {
            gs_debug ("Exiting deep sleep");
            manager->priv->deepsleep = FALSE;
        }
    } else if (!manager->priv->throttled && !manager->priv->deepsleep) {
        gs_debug ("Entering deep sleep, suspending jobs");
        manager->priv->deepsleep = TRUE;
        manager_suspend_jobs (manager);
    }

    return TRUE;
}

static void
add_deepsleep_idle (GSManager *manager) {
    remove_deepsleep_idle(manager);
    manager->priv->deepsleep_idle_id = g_timeout_add_seconds (15, deepsleep_idle, manager);
}

static gboolean
window_map_event_cb (GSWindow  *window,
                     GdkEvent  *event,
                     GSManager *manager) {
    gs_debug ("Handling window map_event event");
    manager_maybe_grab_window (manager, window);
    manager_maybe_start_job_for_window (manager, window);
    return FALSE;
}

static void
manager_show_window (GSManager *manager,
                     GSWindow  *window) {
    GSJob *job;

    job = gs_job_new_for_widget (gs_window_get_drawing_area (window));

    manager_add_job_for_window (manager, window, job);

    remove_lock_timer (manager);
    add_lock_timer (manager, manager->priv->prefs->lock_timeout);

    if (manager->priv->prefs->cycle >= 10) {
        remove_cycle_timer (manager);
        add_cycle_timer (manager, manager->priv->prefs->cycle);
    }

    remove_dpms_timer (manager);
    add_dpms_timer (manager, manager->priv->prefs->dpms_sleep_timeout);

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
    GSList *l;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    gs_debug ("Handling dialog up");

    g_signal_emit (manager, signals[AUTH_REQUEST_BEGIN], 0);

    manager->priv->dialog_up = TRUE;
    /* make all other windows insensitive to not get events */
    for (l = manager->priv->windows; l; l = l->next) {
        if (l->data != window) {
            gtk_widget_set_sensitive (GTK_WIDGET (l->data), FALSE);
        }
    }

    /* move devices grab so that dialog can be used;
       release the pointer grab while dialog is up so that
       the dialog can be used. We'll regrab it when the dialog goes down */
    gs_debug ("Initiate pointer-less grab move to %p", window);
    gs_grab_move_to_window (manager->priv->grab,
                            gs_window_get_gdk_window (window),
                            gs_window_get_display (window),
                            TRUE, FALSE);

    if (!manager->priv->throttled) {
        gs_debug ("Suspending jobs");

        manager_suspend_jobs (manager);
    }

    remove_dpms_timer (manager);
}

static void
handle_window_dialog_down (GSManager *manager,
                           GSWindow  *window) {
    GSList *l;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    gs_debug ("Handling dialog down");

    /* regrab pointer */
    gs_grab_move_to_window (manager->priv->grab,
                            gs_window_get_gdk_window (window),
                            gs_window_get_display (window),
                            FALSE, FALSE);

    /* make all windows sensitive to get events */
    for (l = manager->priv->windows; l; l = l->next) {
        gtk_widget_set_sensitive (GTK_WIDGET (l->data), TRUE);
    }

    manager->priv->dialog_up = FALSE;

    if (!manager->priv->throttled) {
        manager_resume_jobs (manager);
    }

    remove_dpms_timer (manager);
    add_dpms_timer (manager, manager->priv->prefs->dpms_sleep_timeout);

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
    g_signal_handlers_disconnect_by_func (window, window_grab_broken_cb, manager);
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
    g_signal_connect_object (window, "grab_broken_event",
                             G_CALLBACK (window_grab_broken_cb), manager, G_CONNECT_AFTER);
}


static void
gs_manager_create_window_for_monitor (GSManager  *manager,
                                      GdkMonitor *monitor) {
    GSWindow    *window;
    GdkRectangle rect;

    if (!gs_manager_is_real_monitor (monitor))
        return;

    if (g_slist_nth (manager->priv->windows, manager_get_monitor_index(monitor))) {
        gs_debug ("Found already created window for Monitor %d", manager_get_monitor_index(monitor));
        return;
    }

    gdk_monitor_get_geometry (monitor, &rect);

    gs_debug ("Creating a Window [%d,%d] (%dx%d) for monitor %d",
              rect.x, rect.y, rect.width, rect.height, manager_get_monitor_index (monitor));

    window = gs_window_new (monitor);
    gs_window_set_status_message (window, manager->priv->status_message);
    gs_window_set_lock_active (window, manager->priv->lock_active);
    connect_window_signals (manager, window);

    manager->priv->windows = g_slist_insert (manager->priv->windows, window, manager_get_monitor_index(monitor));

    if (manager->priv->active) {
        gtk_widget_show (GTK_WIDGET (window));
    }
}

static GSList *
add_overlays (GSManager *manager) {
    GSList     *l;
    GSList     *windows = NULL;

    gs_debug("Reconfiguring monitors, adding overlays");

    l = manager->priv->windows;
    while (l != NULL) {
        GdkMonitor *this_monitor;
        GSList     *next = l->next;
        GtkWidget  *window;
        GdkRectangle rect;

        this_monitor = gs_window_get_monitor (GS_WINDOW (l->data));

        // Display an overlay to protect each window as we redraw everything
        if (GDK_IS_MONITOR (this_monitor)) {
            window = gtk_window_new (GTK_WINDOW_POPUP);
            gdk_monitor_get_geometry (this_monitor, &rect);
            gtk_window_move (GTK_WINDOW (window), rect.x, rect.y);
            gtk_window_resize (GTK_WINDOW (window), rect.width, rect.height);
            gtk_widget_show (GTK_WIDGET (window));
            windows = g_slist_append (windows, window);
        }

        l = next;
    }

    return windows;
}

static gboolean
remove_overlays (gpointer user_data) {
    GSList *windows = user_data;

    gs_debug("Done reconfiguring monitors, removing overlays");
    g_slist_free_full (windows, (GDestroyNotify) gtk_widget_destroy);
    return FALSE;
}

static void
reconfigure_monitors (GdkDisplay *display,
                      GSManager  *manager) {
    GSList     *l;
    GSList     *windows;

    windows = add_overlays (manager);

    /* Remove lost windows */
    l = manager->priv->windows;
    while (l != NULL) {
        GSList     *next = l->next;

        manager_maybe_stop_job_for_window (manager, GS_WINDOW (l->data));
        g_hash_table_remove (manager->priv->jobs, l->data);
        gs_window_destroy (GS_WINDOW (l->data));
        manager->priv->windows = g_slist_delete_link (manager->priv->windows, l);

        l = next;
    }

    gs_manager_create_windows_for_display (manager, display);

    gs_manager_request_unlock (manager);

    g_timeout_add_seconds (2, remove_overlays, windows);
}

static void
on_display_monitor_added (GdkDisplay *display,
                          GdkMonitor *monitor,
                          GSManager  *manager) {
    int         n_monitors;

    n_monitors = gs_manager_get_n_monitors (display);

    gs_debug ("Monitor %s added on display %s, now there are %d",
              gdk_monitor_get_model(monitor), gdk_display_get_name (display), n_monitors);

    reconfigure_monitors (display, manager);
}

static void
on_display_monitor_removed (GdkDisplay *display,
                            GdkMonitor *monitor,
                            GSManager  *manager) {
    int         n_monitors;

    n_monitors = gs_manager_get_n_monitors (display);

    gs_debug ("Monitor %p removed on display %s, now there are %d",
              monitor, gdk_display_get_name (display), n_monitors);

    reconfigure_monitors (display, manager);
}

static void
gs_manager_destroy_windows (GSManager *manager) {
    GdkDisplay  *display;
    GSList      *l;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    if (manager->priv->windows == NULL) {
        return;
    }

    display = gdk_display_get_default ();

    g_signal_handlers_disconnect_by_func (display,
                                          on_display_monitor_removed,
                                          manager);
    g_signal_handlers_disconnect_by_func (display,
                                          on_display_monitor_added,
                                          manager);

    for (l = manager->priv->windows; l; l = l->next) {
        gs_window_destroy (l->data);
    }
    g_slist_free (manager->priv->windows);
    manager->priv->windows = NULL;
}

static void
gs_manager_finalize (GObject *object) {
    GSManager *manager;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_MANAGER (object));

    manager = GS_MANAGER (object);

    g_return_if_fail (manager->priv != NULL);

    remove_deepsleep_idle (manager);
    remove_timers(manager);

    gs_grab_release (manager->priv->grab, TRUE);

    manager_stop_jobs (manager);

    gs_manager_destroy_windows (manager);

    manager->priv->active = FALSE;

    g_object_unref (manager->priv->grab);

    G_OBJECT_CLASS (gs_manager_parent_class)->finalize (object);
}

static void
gs_manager_create_windows_for_display (GSManager  *manager,
                                       GdkDisplay *display) {
    int n_monitors;
    int i;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));
    g_return_if_fail (GDK_IS_DISPLAY (display));

    g_object_ref (manager);
    g_object_ref (display);

    n_monitors = gdk_display_get_n_monitors (display);

    gs_debug ("Creating %d windows for display %s",
              n_monitors, gdk_display_get_name (display));

    for (i = 0; i < n_monitors; i++) {
        GdkMonitor *mon = gdk_display_get_monitor (display, i);
        gs_manager_create_window_for_monitor (manager, mon);
    }

    g_object_unref (display);
    g_object_unref (manager);
}

static void
gs_manager_create_windows (GSManager *manager) {
    GdkDisplay  *display;

    g_return_if_fail (manager != NULL);
    g_return_if_fail (GS_IS_MANAGER (manager));

    g_assert (manager->priv->windows == NULL);

    display = gdk_display_get_default ();
    g_signal_connect (display, "monitor-added",
                      G_CALLBACK (on_display_monitor_added),
                      manager);
    g_signal_connect (display, "monitor-removed",
                      G_CALLBACK (on_display_monitor_removed),
                      manager);

    gs_manager_create_windows_for_display (manager, display);
}

GSManager *
gs_manager_new (void) {
    GObject   *manager;
    GSManager *mgr;

    manager = g_object_new (GS_TYPE_MANAGER, NULL);

    mgr = GS_MANAGER (manager);

    return mgr;
}

static void
show_windows (GSList *windows) {
    GSList *l;

    for (l = windows; l; l = l->next) {
        gtk_widget_show (GTK_WIDGET (l->data));
    }
}

static void
remove_job (GSJob *job) {
    if (job == NULL) {
        return;
    }

    gs_job_stop (job);
    g_object_unref (job);
}

static gboolean
gs_manager_activate (GSManager *manager) {
    GSList     *windows;
    gboolean    res;

    g_return_val_if_fail (manager != NULL, FALSE);
    g_return_val_if_fail (GS_IS_MANAGER (manager), FALSE);

    if (manager->priv->active) {
        gs_debug ("Trying to activate manager when already active");
        return FALSE;
    }

    res = gs_grab_grab_root (manager->priv->grab, FALSE, FALSE);
    if (!res) {
        return FALSE;
    }

    if (manager->priv->windows == NULL) {
        gs_manager_create_windows (GS_MANAGER (manager));
    }

    manager->priv->jobs = g_hash_table_new_full (g_direct_hash,
                                                 g_direct_equal,
                                                 NULL,
                                                 (GDestroyNotify)remove_job);

    manager->priv->active = TRUE;

    windows = add_overlays (manager);

    show_windows (manager->priv->windows);

    g_timeout_add_seconds (2, remove_overlays, windows);

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

    gs_grab_release (manager->priv->grab, TRUE);

    manager_stop_jobs (manager);

    gs_manager_destroy_windows (manager);

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

    if (manager->priv->windows == NULL) {
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
