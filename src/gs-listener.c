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

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include "gs-listener-x11.h"
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include "gs-listener-wayland.h"
#endif

#include "gs-listener.h"
#include "gs-debug.h"
#include "gs-prefs.h"

#define get_instance_private(instance) ((GSListenerPrivate *) \
    gs_listener_get_instance_private (GS_LISTENER (instance)))

static void         gs_listener_finalize        (GObject            *object);

typedef struct _GSListenerPrivate {
    GSPrefs *prefs;
    gboolean enabled;
    guint timeout;
    gboolean lock_enabled;
    guint lock_timeout;
} GSListenerPrivate;

enum {
    ACTIVATE,
    LOCK,
    POKE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GSListener, gs_listener, G_TYPE_OBJECT)



static void
gs_listener_class_init (GSListenerClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gs_listener_finalize;

    signals[ACTIVATE] =
        g_signal_new ("activate",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[LOCK] =
        g_signal_new ("lock",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[POKE] =
        g_signal_new ("poke",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static gboolean
set_timeouts (gpointer listener) {
    GSListenerPrivate *priv = get_instance_private (listener);
    guint timeout = priv->prefs->timeout * 60;
    guint lock_timeout = priv->prefs->lock_timeout * 60;
    gboolean enabled = priv->prefs->saver_enabled && priv->prefs->idle_activation_enabled;
    gboolean update_needed = FALSE;

    if (priv->enabled != enabled) {
        priv->enabled = enabled;
        update_needed = TRUE;
        gs_debug("Saver state changed to %s", enabled ? "enabled" : "disabled");
    }

    if (priv->timeout != timeout) {
        priv->timeout = timeout;
        update_needed = TRUE;
        gs_debug("Saver timeout updated to %d seconds", timeout);
    }

    if (priv->lock_enabled != priv->prefs->lock_with_saver_enabled) {
        priv->lock_enabled = priv->prefs->lock_with_saver_enabled;
        update_needed = TRUE;
        gs_debug("Lock with saver state changed to %s", priv->lock_enabled ? "enabled" : "disabled");
    }

    if (priv->lock_timeout != lock_timeout) {
        priv->lock_timeout = lock_timeout;
        update_needed = TRUE;
        gs_debug("Lock timeout updated to %d seconds", lock_timeout);
    }

    if (update_needed) {
        GS_LISTENER_GET_CLASS (listener)->set_timeouts (listener, enabled, timeout, lock_timeout);
    }

    return FALSE;
}

static void
gs_listener_init (GSListener *listener) {
    GSListenerPrivate *priv = get_instance_private (listener);

    priv->prefs = gs_prefs_new ();
    g_signal_connect_object (priv->prefs, "changed", G_CALLBACK (set_timeouts), listener, G_CONNECT_SWAPPED);
    g_idle_add (set_timeouts, listener);
}

static void
gs_listener_finalize (GObject *object) {
    GSListenerPrivate *priv = get_instance_private (object);

    g_object_unref (priv->prefs);

    G_OBJECT_CLASS (gs_listener_parent_class)->finalize (object);
}

GSListener *
gs_listener_new (void) {
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
        return g_object_new (GS_TYPE_LISTENER_X11, NULL);
    }
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
        return g_object_new (GS_TYPE_LISTENER_WAYLAND, NULL);
    }
#endif

    g_critical ("Idle status monitoring is not supported on this windowing environment");
    return NULL;
}
