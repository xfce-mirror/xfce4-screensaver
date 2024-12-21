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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>

#include "protocols/ext-session-lock-v1-client.h"

#include "gs-debug.h"
#include "gs-session-lock-manager.h"

static void
gs_session_lock_manager_finalize (GObject *object);

static void
registry_global (void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
static void
registry_global_remove (void *data, struct wl_registry *registry, uint32_t id);
static void
lock_locked (void *data, struct ext_session_lock_v1 *lock);
static void
lock_finished (void *data, struct ext_session_lock_v1 *lock);
static void
surface_configure (void *data, struct ext_session_lock_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height);

struct _GSSessionLockManager {
    GObject __parent__;

    struct wl_registry *wl_registry;
    struct ext_session_lock_manager_v1 *wl_manager;
    struct ext_session_lock_v1 *wl_lock;
    GHashTable *lock_surfaces;
    gboolean locked;
};

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static const struct ext_session_lock_v1_listener lock_listener = {
    .locked = lock_locked,
    .finished = lock_finished,
};

static const struct ext_session_lock_surface_v1_listener surface_listener = {
    .configure = surface_configure,
};



G_DEFINE_TYPE (GSSessionLockManager, gs_session_lock_manager, G_TYPE_OBJECT)



static void
gs_session_lock_manager_class_init (GSSessionLockManagerClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gs_session_lock_manager_finalize;
}

static void
gs_session_lock_manager_init (GSSessionLockManager *manager) {
    struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());

    manager->wl_registry = wl_display_get_registry (wl_display);
    wl_registry_add_listener (manager->wl_registry, &registry_listener, manager);
    wl_display_roundtrip (wl_display);
    if (manager->wl_manager != NULL) {
        manager->lock_surfaces = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) ext_session_lock_surface_v1_destroy);
    } else {
        g_warning ("ext-session-lock-v1 protocol unsupported: xfce4-screensaver will not be able to activate or lock the session");
    }
}

static void
gs_session_lock_manager_finalize (GObject *object) {
    GSSessionLockManager *manager = GS_SESSION_LOCK_MANAGER (object);

    if (manager->wl_manager != NULL) {
        if (manager->wl_lock != NULL) {
            lock_finished (manager, manager->wl_lock);
        }
        g_hash_table_destroy (manager->lock_surfaces);
        ext_session_lock_manager_v1_destroy (manager->wl_manager);
    }
    wl_registry_destroy (manager->wl_registry);

    G_OBJECT_CLASS (gs_session_lock_manager_parent_class)->finalize (object);
}

static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t id,
                 const char *interface,
                 uint32_t version) {
    GSSessionLockManager *manager = data;
    if (g_strcmp0 (ext_session_lock_manager_v1_interface.name, interface) == 0) {
        manager->wl_manager = wl_registry_bind (manager->wl_registry, id, &ext_session_lock_manager_v1_interface,
                                                MIN ((uint32_t) ext_session_lock_manager_v1_interface.version, version));
    }
}

static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t id) {
}

static void
lock_locked (void *data,
             struct ext_session_lock_v1 *lock) {
    GSSessionLockManager *manager = data;
    gs_debug ("Locked event received from compositor");
    manager->locked = TRUE;
}

static void
lock_finished (void *data,
               struct ext_session_lock_v1 *lock) {
    GSSessionLockManager *manager = data;
    gs_debug ("Finished event received from compositor");
    gs_session_lock_manager_unlock (manager);
}

static void
surface_configure (void *data,
                   struct ext_session_lock_surface_v1 *surface,
                   uint32_t serial,
                   uint32_t width,
                   uint32_t height) {
    gs_debug ("Configuring lock surface for monitor %s: width %d, height %d",
              gdk_monitor_get_model (gs_window_get_monitor (data)), width, height);
    gdk_window_move_resize (gtk_widget_get_window (data), 0, 0, width, height);
    ext_session_lock_surface_v1_ack_configure (surface, serial);
}



GSSessionLockManager *
gs_session_lock_manager_new (void) {
    GSSessionLockManager *manager = g_object_new (GS_TYPE_SESSION_LOCK_MANAGER, NULL);
    if (manager->wl_manager == NULL) {
        g_object_unref (manager);
        manager = NULL;
    }
    return manager;
}

gboolean
gs_session_lock_manager_lock (GSSessionLockManager *manager) {
    g_return_val_if_fail (GS_IS_SESSION_LOCK_MANAGER (manager), FALSE);
    g_return_val_if_fail (manager->wl_lock == NULL, FALSE);

    gs_debug ("Locking session");
    manager->wl_lock = ext_session_lock_manager_v1_lock (manager->wl_manager);
    ext_session_lock_v1_add_listener (manager->wl_lock, &lock_listener, manager);
    wl_display_roundtrip (gdk_wayland_display_get_wl_display (gdk_display_get_default ()));

    return manager->wl_lock != NULL;
}

void
gs_session_lock_manager_unlock (GSSessionLockManager *manager) {
    g_return_if_fail (GS_IS_SESSION_LOCK_MANAGER (manager));
    g_return_if_fail (manager->wl_lock != NULL);

    gs_debug ("Unlocking session");
    if (manager->locked) {
        ext_session_lock_v1_unlock_and_destroy (manager->wl_lock);
        wl_display_roundtrip (gdk_wayland_display_get_wl_display (gdk_display_get_default ()));
    } else {
        ext_session_lock_v1_destroy (manager->wl_lock);
    }
    g_hash_table_remove_all (manager->lock_surfaces);
    manager->wl_lock = NULL;
    manager->locked = FALSE;
}

static void
window_realized (GtkWidget *window,
                 GSSessionLockManager *manager) {
    struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
    struct wl_surface *wl_surface = gdk_wayland_window_get_wl_surface (gtk_widget_get_window (window));
    struct wl_output *wl_output = gdk_wayland_monitor_get_wl_output (gs_window_get_monitor (GS_WINDOW (window)));
    struct ext_session_lock_surface_v1 *wl_lock_surface;

    gs_debug ("Creating lock surface for monitor %s", gdk_monitor_get_model (gs_window_get_monitor (GS_WINDOW (window))));
    gdk_wayland_window_set_use_custom_surface (gtk_widget_get_window (window));
    wl_lock_surface = ext_session_lock_v1_get_lock_surface (manager->wl_lock, wl_surface, wl_output);
    ext_session_lock_surface_v1_add_listener (wl_lock_surface, &surface_listener, window);
    g_hash_table_insert (manager->lock_surfaces, window, wl_lock_surface);
    wl_display_roundtrip (wl_display);
}

void
gs_session_lock_manager_add_window (GSSessionLockManager *manager,
                                    GSWindow *window) {
    const gchar *model;

    g_return_if_fail (GS_IS_SESSION_LOCK_MANAGER (manager));
    g_return_if_fail (manager->wl_lock != NULL);
    g_return_if_fail (GS_IS_WINDOW (window));
    g_return_if_fail (!gtk_widget_get_realized (GTK_WIDGET (window)));

    model = gdk_monitor_get_model (gs_window_get_monitor (window));
    gs_debug ("Adding window for monitor %s", model);
    g_signal_connect (window, "realize", G_CALLBACK (window_realized), manager);
    g_object_set_data_full (G_OBJECT (window), "monitor-model", g_strdup (model), g_free);
}

void
gs_session_lock_manager_remove_window (GSSessionLockManager *manager,
                                       GSWindow *window) {
    g_return_if_fail (GS_IS_SESSION_LOCK_MANAGER (manager));
    g_return_if_fail (manager->wl_lock != NULL);
    g_return_if_fail (GS_IS_WINDOW (window));

    gs_debug ("Removing window for monitor %s", g_object_get_data (G_OBJECT (window), "monitor-model"));
    /* we can't do this on dispose or destroy or whatever signal unfortunately, it's too
     * late and causes a protocol error (surface destroyed before its role) */
    g_hash_table_remove (manager->lock_surfaces, window);
}
