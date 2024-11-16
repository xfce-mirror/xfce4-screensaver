/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
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
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE = 1
#include <libwnck/libwnck.h>

#include <X11/extensions/scrnsaver.h>

#include "gs-listener-x11.h"
#include "gs-marshal.h"
#include "gs-debug.h"
#include "gs-prefs.h"

static void         gs_listener_x11_finalize        (GObject            *object);
static void         gs_listener_x11_set_timeouts    (GSListener         *listener,
                                                     gboolean            enabled,
                                                     guint               timeout,
                                                     guint               lock_timeout);

static void         reset_timer                     (GSListenerX11      *listener,
                                                     guint               timeout);

struct GSListenerX11Private {
    int scrnsaver_event_base;
    gboolean enabled;
    guint    timeout;
    guint    lock_timeout;
    guint    timer_id;
    GSPrefs *prefs;
    WnckScreen *wnck_screen;
};

G_DEFINE_TYPE_WITH_PRIVATE (GSListenerX11, gs_listener_x11, GS_TYPE_LISTENER)

static void
gs_listener_x11_class_init (GSListenerX11Class *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GSListenerClass *listener_class = GS_LISTENER_CLASS (klass);

    object_class->finalize = gs_listener_x11_finalize;

    listener_class->set_timeouts = gs_listener_x11_set_timeouts;
}

static gboolean
get_x11_idle_info (guint *idle_time,
                   gint  *state) {
    Display *display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    static XScreenSaverInfo *mit_info;

    if (mit_info == NULL)
        mit_info = XScreenSaverAllocInfo();
    if (mit_info == NULL)
        return FALSE;

    if (XScreenSaverQueryInfo(display, GDK_ROOT_WINDOW(), mit_info) == 0)
        return FALSE;

    gs_debug("XScreenSaverInfo: state %x kind %x tos %lu idle %lu m %lx",
             mit_info->state, mit_info->kind, mit_info->til_or_since,
             mit_info->idle, mit_info->eventMask);
    *idle_time = mit_info->idle / 1000;  // seconds
    *state = mit_info->state;

    return TRUE;
}

static gboolean
check_fullscreen_window (GSListenerX11 *listener) {
    WnckWindow *window = wnck_screen_get_active_window (listener->priv->wnck_screen);
    if (window == NULL) {
        return FALSE;
    }

    return wnck_window_is_fullscreen (window);
}

static gboolean
lock_timer (gpointer user_data) {
    GSListenerX11 *listener = user_data;
    guint    idle_time;
    guint    lock_time = 0;
    gint     state;
    gboolean lock_state;
    gboolean fullscreen_inhibition = FALSE;

    if (!listener->priv->enabled)
        return TRUE;

    if (get_x11_idle_info (&idle_time, &state) == FALSE)
        return TRUE;

    if (idle_time > listener->priv->timeout) {
        lock_time = idle_time - listener->priv->timeout;
    }
    lock_state = (state == ScreenSaverOn &&
                  listener->priv->prefs->lock_with_saver_enabled &&
                  lock_time >= listener->priv->lock_timeout);

    if (listener->priv->prefs->fullscreen_inhibit) {
        fullscreen_inhibition = check_fullscreen_window(listener);
    }

    gs_debug("Idle: %us, Saver Timeout: %is, Lock: %s, Lock Timeout: %is, Lock Timer: %us, Lock Status: %s, Fullscreen: %s",
             idle_time,
             listener->priv->timeout,
             listener->priv->prefs->lock_with_saver_enabled ? "Enabled" : "Disabled",
             listener->priv->lock_timeout,
             lock_time,
             lock_state ? "Locked" : "Unlocked",
             fullscreen_inhibition ? "Inhibited" : "Uninhibited");

    if (fullscreen_inhibition) {
        if (idle_time < listener->priv->timeout) {
            reset_timer(listener, listener->priv->timeout - idle_time);
        } else {
            reset_timer(listener, 30);
        }
        return FALSE;
    }

    if (idle_time >= listener->priv->timeout &&
            state != ScreenSaverDisabled) {
        if (lock_state)
            g_signal_emit_by_name(listener, "lock");
        else
            g_signal_emit_by_name(listener, "activate");
    } else {
        switch (state) {
            case ScreenSaverOff:
                // Reset the lock timer
                if (idle_time < listener->priv->timeout) {
                    reset_timer(listener, listener->priv->timeout - idle_time);
                } else {
                    reset_timer(listener, 30);
                }
                return FALSE;
                break;

            case ScreenSaverDisabled:
                // Disabled, do nothing
                break;

            case ScreenSaverOn:
                // lock now!
                if (lock_state)
                    g_signal_emit_by_name(listener, "lock");
                break;
        }
    }

    return TRUE;
}

static void
remove_lock_timer(GSListenerX11 *listener) {
    if (listener->priv->timer_id != 0) {
        g_source_remove(listener->priv->timer_id);
        listener->priv->timer_id = 0;
    }
}

static void
reset_timer(GSListenerX11 *listener,
            guint          timeout) {
    remove_lock_timer(listener);

    listener->priv->timer_id = g_timeout_add_seconds(timeout, lock_timer, listener);
}

static GdkFilterReturn
xroot_filter (GdkXEvent *xevent,
              GdkEvent  *event,
              gpointer   data) {
    GSListenerX11 *listener;
    XEvent *ev;

    g_return_val_if_fail (data != NULL, GDK_FILTER_CONTINUE);
    g_return_val_if_fail (GS_IS_LISTENER_X11 (data), GDK_FILTER_CONTINUE);

    listener = GS_LISTENER_X11 (data);

    if (!listener->priv->enabled)
        return GDK_FILTER_CONTINUE;

    ev = xevent;

    switch (ev->xany.type) {
    default:
        /* extension events */
        if (ev->xany.type == (listener->priv->scrnsaver_event_base + ScreenSaverNotify)) {
            XScreenSaverNotifyEvent *xssne = (XScreenSaverNotifyEvent *) ev;
            switch (xssne->state) {
                case ScreenSaverOff:
                case ScreenSaverDisabled:
                    // Reset the lock timer
                    gs_debug("ScreenSaver timer reset");
                    reset_timer(listener, listener->priv->timeout);
                    break;

                case ScreenSaverOn:
                    if (listener->priv->prefs->fullscreen_inhibit && check_fullscreen_window (listener)) {
                        gs_debug ("Fullscreen inhibit: ScreenSaver timer reset");
                        reset_timer (listener, listener->priv->timeout);
                    } else {
                        gs_debug ("Activating screensaver on ScreenSaverOn");
                        g_signal_emit_by_name (listener, "activate");
                    }
                    break;
            }
        }
        break;
    }

    return GDK_FILTER_CONTINUE;
}


static void
gs_listener_x11_acquire (GSListenerX11 *listener) {
    GdkDisplay *display;
    GdkScreen *screen;
    GdkWindow *window;
    int scrnsaver_error_base;

    g_return_if_fail (listener != NULL);

    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen (display);
    window = gdk_screen_get_root_window (screen);

    gdk_x11_display_error_trap_push (display);
    if (XScreenSaverQueryExtension (GDK_DISPLAY_XDISPLAY (display),
                                    &listener->priv->scrnsaver_event_base,
                                    &scrnsaver_error_base)) {
        unsigned long events;
        events = ScreenSaverNotifyMask;
        XScreenSaverSelectInput (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window), events);
        gs_debug ("ScreenSaver Registered");
    } else {
        gs_debug ("ScreenSaverExtension not found");
    }

    gdk_x11_display_error_trap_pop_ignored (display);

    gdk_window_add_filter (NULL, (GdkFilterFunc)xroot_filter, listener);
}

static void
gs_listener_x11_set_timeouts (GSListener *_listener,
                              gboolean enabled,
                              guint timeout,
                              guint lock_timeout) {
    GSListenerX11 *listener = GS_LISTENER_X11 (_listener);
    Display *display = gdk_x11_display_get_xdisplay(gdk_display_get_default());

    listener->priv->enabled = enabled;
    listener->priv->timeout = timeout;
    listener->priv->lock_timeout = lock_timeout;

    /* set X server timeouts and disable screen blanking */
    XSetScreenSaver(display, timeout, timeout, DontPreferBlanking, DontAllowExposures);

    reset_timer (listener, timeout);
}

static void
gs_listener_x11_init (GSListenerX11 *listener) {
    listener->priv = gs_listener_x11_get_instance_private (listener);
    listener->priv->prefs = gs_prefs_new ();
    listener->priv->wnck_screen = wnck_screen_get_default ();
    gs_listener_x11_acquire (listener);
}

static void
gs_listener_x11_finalize (GObject *object) {
    GSListenerX11 *listener;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_LISTENER_X11 (object));

    listener = GS_LISTENER_X11 (object);

    g_return_if_fail (listener->priv != NULL);

    gdk_window_remove_filter (NULL, (GdkFilterFunc)xroot_filter, NULL);
    g_object_unref (listener->priv->prefs);

    G_OBJECT_CLASS (gs_listener_x11_parent_class)->finalize (object);
}
