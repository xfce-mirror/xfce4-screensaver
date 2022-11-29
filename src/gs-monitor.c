/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/extensions/scrnsaver.h>

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdkx.h>

#include "gs-debug.h"
#include "gs-listener-dbus.h"
#include "gs-listener-x11.h"
#include "gs-manager.h"
#include "gs-monitor.h"
#include "gs-prefs.h"
#include "xfce4-screensaver.h"

static void gs_monitor_finalize(GObject* object);

struct GSMonitorPrivate {
    GSListener* listener;
    GSListenerX11 *listener_x11;
    GSManager* manager;
    GSPrefs* prefs;
};

G_DEFINE_TYPE_WITH_PRIVATE(GSMonitor, gs_monitor, G_TYPE_OBJECT)

static void gs_monitor_class_init(GSMonitorClass* klass) {
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gs_monitor_finalize;
}

static void manager_deactivated_cb(GSManager* manager, GSMonitor* monitor) {
    gs_listener_activate_saver (monitor->priv->listener, FALSE);
}

static void gs_monitor_simulate_user_activity(GSMonitor* monitor) {
    Display *display = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
    XScreenSaverSuspend (display, TRUE);
    XSync (display, FALSE);
    XScreenSaverSuspend (display, FALSE);
    XSync (display, FALSE);

    /* request that the manager unlock -
       will pop up a dialog if necessary */
    gs_manager_request_unlock(monitor->priv->manager);
}

static void listener_lock_cb(GSListener* listener, GSMonitor* monitor) {
    if (monitor->priv->prefs->lock_enabled) {
        gs_manager_enable_locker(monitor->priv->manager, TRUE);
        gs_listener_activate_saver(monitor->priv->listener, TRUE);
    } else {
        gs_debug("Locking disabled by the administrator");
    }
}

static void listener_x11_activate_cb(GSListenerX11* listener, GSMonitor* monitor) {
    if (gs_listener_is_inhibited(monitor->priv->listener)) {
        gs_debug("Idle screen saving inhibited via dbus");
        return;
    }

    gs_listener_activate_saver(monitor->priv->listener, TRUE);
}

static void listener_x11_lock_cb(GSListenerX11* listener, GSMonitor* monitor) {
    if (gs_listener_is_inhibited(monitor->priv->listener)) {
        gs_debug("Idle locking inhibited via dbus");
        return;
    }

    listener_lock_cb (NULL, monitor);
}

static void listener_quit_cb(GSListener* listener, GSMonitor* monitor) {
    gs_listener_activate_saver(monitor->priv->listener, FALSE);
    xfce4_screensaver_quit();
}

static void listener_cycle_cb(GSListener* listener, GSMonitor* monitor) {
    gs_manager_cycle(monitor->priv->manager);
}

static void listener_show_message_cb(GSListener* listener,
                                     const char* summary,
                                     const char* body,
                                     const char* icon,
                                     GSMonitor*  monitor) {
    if (!monitor->priv->prefs->lock_enabled)
        return;

    gs_manager_show_message(monitor->priv->manager, summary, body, icon);
}

static gboolean listener_active_changed_cb(GSListener* listener, gboolean active, GSMonitor* monitor) {
    gboolean res;

    res = gs_manager_activate_saver(monitor->priv->manager, active);
    if (!res) {
        gs_debug("Unable to set manager active: %d", active);
        return FALSE;
    }
    return TRUE;
}

static void listener_throttle_changed_cb(GSListener* listener, gboolean throttled, GSMonitor* monitor) {
    gs_manager_set_throttled(monitor->priv->manager, throttled);
}

static void listener_simulate_user_activity_cb(GSListener* listener, GSMonitor* monitor) {
    gs_monitor_simulate_user_activity(monitor);
}

static void disconnect_listener_signals(GSMonitor* monitor) {
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_lock_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_quit_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_cycle_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_active_changed_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_throttle_changed_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_simulate_user_activity_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener, listener_show_message_cb, monitor);

    g_signal_handlers_disconnect_by_func(monitor->priv->listener_x11, listener_x11_activate_cb, monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->listener_x11, listener_x11_lock_cb, monitor);
}

static void connect_listener_signals(GSMonitor* monitor) {
    g_signal_connect(monitor->priv->listener, "lock",
                     G_CALLBACK(listener_lock_cb), monitor);
    g_signal_connect(monitor->priv->listener, "quit",
                     G_CALLBACK(listener_quit_cb), monitor);
    g_signal_connect(monitor->priv->listener, "cycle",
                     G_CALLBACK(listener_cycle_cb), monitor);
    g_signal_connect(monitor->priv->listener, "active-changed",
                     G_CALLBACK(listener_active_changed_cb), monitor);
    g_signal_connect(monitor->priv->listener, "throttle-changed",
                     G_CALLBACK(listener_throttle_changed_cb), monitor);
    g_signal_connect(monitor->priv->listener, "simulate-user-activity",
                     G_CALLBACK(listener_simulate_user_activity_cb), monitor);
    g_signal_connect(monitor->priv->listener, "show-message",
                     G_CALLBACK(listener_show_message_cb), monitor);

    g_signal_connect(monitor->priv->listener_x11, "activate",
                     G_CALLBACK(listener_x11_activate_cb), monitor);
    g_signal_connect(monitor->priv->listener_x11, "lock",
                     G_CALLBACK(listener_x11_lock_cb), monitor);
}

static void gs_monitor_init(GSMonitor* monitor) {
    monitor->priv = gs_monitor_get_instance_private (monitor);

    monitor->priv->prefs = gs_prefs_new();

    monitor->priv->listener = gs_listener_new();
    monitor->priv->listener_x11 = gs_listener_x11_new();
    connect_listener_signals(monitor);

    monitor->priv->manager = gs_manager_new();
    g_signal_connect(monitor->priv->manager, "deactivated",
                     G_CALLBACK(manager_deactivated_cb), monitor);
}

static void gs_monitor_finalize(GObject* object) {
    GSMonitor* monitor;

    g_return_if_fail(object != NULL);
    g_return_if_fail(GS_IS_MONITOR(object));

    monitor = GS_MONITOR(object);

    g_return_if_fail(monitor->priv != NULL);

    disconnect_listener_signals(monitor);
    g_signal_handlers_disconnect_by_func(monitor->priv->manager, manager_deactivated_cb, monitor);

    g_object_unref(monitor->priv->listener);
    g_object_unref(monitor->priv->listener_x11);
    g_object_unref(monitor->priv->manager);
    g_object_unref(monitor->priv->prefs);

    G_OBJECT_CLASS(gs_monitor_parent_class)->finalize(object);
}

GSMonitor* gs_monitor_new(void) {
    GSMonitor* monitor;

    monitor = g_object_new(GS_TYPE_MONITOR, NULL);

    return GS_MONITOR(monitor);
}

gboolean gs_monitor_start(GSMonitor* monitor, GError** error) {
    g_return_val_if_fail(GS_IS_MONITOR(monitor), FALSE);

    if (!gs_listener_acquire(monitor->priv->listener, error)) {
        return FALSE;
    }

    gs_listener_x11_acquire(monitor->priv->listener_x11);

    return TRUE;
}
