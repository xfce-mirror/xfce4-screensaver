/*
 * Copyright (C) 2024 Gaël Bonithon <gael@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <gdk/gdkwayland.h>
#include <libxfce4windowing/libxfce4windowing.h>

#include "gs-listener-wayland.h"
#include "gs-manager.h"
#include "gs-prefs.h"
#include "gs-debug.h"
#include "protocols/ext-idle-notify-v1-client.h"

static void     gs_listener_wayland_finalize               (GObject        *object);
static void     gs_listener_wayland_set_timeouts           (GSListener     *listener,
                                                            gboolean        enabled,
                                                            guint           timeout,
                                                            guint           lock_timeout);

static void registry_global (void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
static void registry_global_remove (void *data, struct wl_registry *registry, uint32_t id);
static void notification_idled (void *data, struct ext_idle_notification_v1 *notification);
static void notification_resumed (void *data, struct ext_idle_notification_v1 *notification);

typedef enum _AlarmId {
    ALARM_ACTIVATE,
    ALARM_LOCK,
    ALARM_DEACTIVATE,
} AlarmId;

struct _GSListenerWayland {
    GSListener __parent__;

    struct wl_registry *wl_registry;
    struct ext_idle_notifier_v1 *wl_notifier;
    GSManager *manager;
    GSPrefs *prefs;
    GHashTable *alarms;
    gboolean enabled;
    guint timeout;
    guint lock_timeout;
    XfwScreen *screen;
};

typedef struct _Alarm {
    GSListenerWayland *listener;
    struct ext_idle_notification_v1 *wl_notification;
    AlarmId id;
    guint timeout;
} Alarm;

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static const struct ext_idle_notification_v1_listener notification_listener = {
    .idled = notification_idled,
    .resumed = notification_resumed,
};



G_DEFINE_TYPE (GSListenerWayland, gs_listener_wayland, GS_TYPE_LISTENER)



static void
gs_listener_wayland_class_init (GSListenerWaylandClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GSListenerClass *listener_class = GS_LISTENER_CLASS (klass);

    object_class->finalize = gs_listener_wayland_finalize;

    listener_class->set_timeouts = gs_listener_wayland_set_timeouts;
}

static void
alarm_free (gpointer data) {
    Alarm *alarm = data;
    ext_idle_notification_v1_destroy (alarm->wl_notification);
    g_free (alarm);
}

static Alarm *
alarm_new (GSListenerWayland *listener,
           AlarmId id,
           guint timeout) {
    struct wl_seat *wl_seat = gdk_wayland_seat_get_wl_seat (gdk_display_get_default_seat (gdk_display_get_default ()));
    Alarm *alarm = g_new0 (Alarm, 1);
    alarm->listener = listener;
    alarm->id = id;
    alarm->timeout = timeout;
    alarm->wl_notification = ext_idle_notifier_v1_get_idle_notification (listener->wl_notifier, timeout, wl_seat);
    ext_idle_notification_v1_add_listener (alarm->wl_notification, &notification_listener, alarm);
    return alarm;
}

static void
alarm_add (GSListenerWayland *listener,
           AlarmId id) {
    switch (id) {
        case ALARM_ACTIVATE:
            g_hash_table_insert (listener->alarms, GINT_TO_POINTER (ALARM_ACTIVATE), alarm_new (listener, ALARM_ACTIVATE, listener->timeout));
            break;
        case ALARM_LOCK:
            g_hash_table_insert (listener->alarms, GINT_TO_POINTER (ALARM_LOCK), alarm_new (listener, ALARM_LOCK, listener->lock_timeout));
            break;
        case ALARM_DEACTIVATE:
            g_hash_table_insert (listener->alarms, GINT_TO_POINTER (ALARM_DEACTIVATE), alarm_new (listener, ALARM_DEACTIVATE, 0));
            break;
        default:
            g_warn_if_reached ();
            break;
    }
}

static void
alarm_remove (GSListenerWayland *listener,
              AlarmId id) {
    g_hash_table_remove (listener->alarms, GINT_TO_POINTER (id));
}

static gboolean
alarm_is_active (GSListenerWayland *listener,
                 AlarmId id) {
    return g_hash_table_contains (listener->alarms, GINT_TO_POINTER (id));
}

static void
manager_activated (GSListenerWayland *listener) {
    if (listener->prefs->lock_with_saver_enabled) {
        if (listener->lock_timeout == 0) {
            gs_debug ("Saver activated, lock-with-saver enabled and lock-timeout == 0, emitting lock signal");
            g_signal_emit_by_name (listener, "lock");
        } else {
            alarm_add (listener, ALARM_LOCK);
        }
    }
    alarm_remove (listener, ALARM_ACTIVATE);
    alarm_add (listener, ALARM_DEACTIVATE);
}

static void
manager_deactivated (GSListenerWayland *listener) {
    alarm_remove (listener, ALARM_DEACTIVATE);
    alarm_remove (listener, ALARM_LOCK);
    if (listener->enabled) {
        alarm_add (listener, ALARM_ACTIVATE);
    }
}

static void
gs_listener_wayland_init (GSListenerWayland *listener) {
    struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
    listener->wl_registry = wl_display_get_registry (wl_display);
    wl_registry_add_listener (listener->wl_registry, &registry_listener, listener);
    wl_display_roundtrip (wl_display);
    if (listener->wl_notifier == NULL) {
        g_warning ("ext-idle-notify-v1 protocol unsupported: timeout-based features won't work");
        return;
    }

    listener->alarms = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, alarm_free);
    listener->prefs = gs_prefs_new ();
    listener->screen = xfw_screen_get_default ();
    listener->manager = gs_manager_new ();
    g_signal_connect_object (listener->manager, "activated", G_CALLBACK (manager_activated), listener, G_CONNECT_SWAPPED);
    g_signal_connect_object (listener->manager, "deactivated", G_CALLBACK (manager_deactivated), listener, G_CONNECT_SWAPPED);
}

static void
gs_listener_wayland_finalize (GObject *object) {
    GSListenerWayland *listener = GS_LISTENER_WAYLAND (object);

    if (listener->wl_notifier != NULL) {
        g_hash_table_destroy (listener->alarms);
        ext_idle_notifier_v1_destroy (listener->wl_notifier);
        g_object_unref (listener->prefs);
        g_object_unref (listener->manager);
        g_object_unref (listener->screen);
    }
    wl_registry_destroy (listener->wl_registry);

    G_OBJECT_CLASS (gs_listener_wayland_parent_class)->finalize (object);
}

static void
gs_listener_wayland_set_timeouts (GSListener *_listener,
                                  gboolean enabled,
                                  guint timeout,
                                  guint lock_timeout) {
    GSListenerWayland *listener = GS_LISTENER_WAYLAND (_listener);
    guint old_timeout = listener->timeout;

    if (listener->wl_notifier == NULL) {
        return;
    }

    listener->enabled = enabled;
    if (!enabled) {
        alarm_remove (listener, ALARM_ACTIVATE);
        alarm_remove (listener, ALARM_LOCK);
        return;
    }

    listener->timeout = timeout * 1000;
    listener->lock_timeout = lock_timeout * 1000;

    if (!listener->prefs->lock_with_saver_enabled) {
        alarm_remove (listener, ALARM_LOCK);
    }
    if (!alarm_is_active (listener, ALARM_DEACTIVATE)) {
        if (listener->timeout != old_timeout || !alarm_is_active (listener, ALARM_ACTIVATE)) {
            alarm_add (listener, ALARM_ACTIVATE);
        }
    }
}



static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t id,
                 const char *interface,
                 uint32_t version) {
    GSListenerWayland *listener = data;
    if (g_strcmp0 (ext_idle_notifier_v1_interface.name, interface) == 0) {
        listener->wl_notifier = wl_registry_bind (listener->wl_registry, id, &ext_idle_notifier_v1_interface,
                                                  MIN ((uint32_t) ext_idle_notifier_v1_interface.version, version));
    }
}

static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t id) {
}

static gboolean
check_fullscreen_window (GSListenerWayland *listener) {
    XfwWindow *window = xfw_screen_get_active_window (listener->screen);
    return window != NULL && xfw_window_is_fullscreen (window);
}

static void
notification_idled (void *data,
                    struct ext_idle_notification_v1 *notification) {
    Alarm *alarm = data;
    GSListenerWayland *listener = alarm->listener;

    switch (alarm->id) {
        case ALARM_ACTIVATE:
            if (listener->prefs->fullscreen_inhibit && check_fullscreen_window (listener)) {
                gs_debug ("Activate timer expired, fullscreen-inhibit enabled and activate window is fullscreen, resetting activate timer");
                alarm_add (listener, ALARM_ACTIVATE);
                break;
            }

            if (listener->prefs->lock_with_saver_enabled) {
                if (listener->lock_timeout == 0) {
                    gs_debug ("Activate timer expired, lock-with-saver enabled and lock-timeout == 0, emitting lock signal");
                    g_signal_emit_by_name (listener, "lock");
                } else {
                    gs_debug ("Activate timer expired, lock-with-saver enabled and lock-timeout > 0, emitting activate signal");
                    g_signal_emit_by_name (listener, "activate");
                }
            } else {
                gs_debug ("Activate timer expired, lock-with-saver disabled, emitting activate signal");
                g_signal_emit_by_name (listener, "activate");
            }
            break;

        case ALARM_LOCK:
            gs_debug ("Lock timer expired, emitting lock signal");
            g_signal_emit_by_name (listener, "lock");
            alarm_remove (listener, ALARM_LOCK);
            break;

        case ALARM_DEACTIVATE:
            break;

        default:
            g_warn_if_reached ();
            break;
    }
}

static void
notification_resumed (void *data,
                      struct ext_idle_notification_v1 *notification) {
    Alarm *alarm = data;
    gs_debug ("User activity detected, emitting poke signal");
    g_signal_emit_by_name (alarm->listener, "poke");
}
