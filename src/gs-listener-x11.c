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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(3, 0, 0)
#include <gtk/gtkx.h>
#else
#include <gdk/gdkx.h>
#endif

#ifdef HAVE_MIT_SAVER_EXTENSION
#include <X11/extensions/scrnsaver.h>
#endif

#include "gs-listener-x11.h"
#include "gs-marshal.h"
#include "gs-debug.h"

static void         gs_listener_x11_class_init      (GSListenerX11Class *klass);
static void         gs_listener_x11_init            (GSListenerX11      *listener);
static void         gs_listener_x11_finalize        (GObject            *object);

#define GS_LISTENER_X11_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GS_TYPE_LISTENER_X11, GSListenerX11Private))

struct GSListenerX11Private
{
#ifdef HAVE_MIT_SAVER_EXTENSION
    int scrnsaver_event_base;
#endif

    gint lock_timeout;
    guint lock_timer_id;
};

enum {
    LOCK,
    LAST_SIGNAL
};

static guint         signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GSListenerX11, gs_listener_x11, G_TYPE_OBJECT)

static void
gs_listener_x11_class_init (GSListenerX11Class *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize       = gs_listener_x11_finalize;

    signals [LOCK] =
        g_signal_new ("lock",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerX11Class, lock),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    g_type_class_add_private (klass, sizeof (GSListenerX11Private));
}

static gboolean
lock_timer (GSListenerX11 *listener)
{
    Display *display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    static XScreenSaverInfo *mit_info = NULL;
    guint idle_time;

    mit_info = XScreenSaverAllocInfo();
    XScreenSaverQueryInfo(display, GDK_ROOT_WINDOW(), mit_info);
    idle_time = mit_info->idle / 1000; // seconds

    if (idle_time >= listener->priv->lock_timeout)
    {
        listener->priv->lock_timer_id = 0;

        gs_debug("Lock timer");

        g_signal_emit(listener, signals[LOCK], 0);
    }
    else
    {
        gs_debug("Idle time interrupted. Resetting lock timer for %i seconds", listener->priv->lock_timeout - idle_time);

        listener->priv->lock_timer_id = g_timeout_add_seconds(listener->priv->lock_timeout - idle_time,
                                      (GSourceFunc)lock_timer,
                                      listener);
    }

    return FALSE;
}

static void
remove_lock_timer (GSListenerX11 *listener)
{
    if (listener->priv->lock_timer_id != 0) {
        g_source_remove (listener->priv->lock_timer_id);
        listener->priv->lock_timer_id = 0;
    }
}

static void
reset_lock_timer (GSListenerX11 *listener)
{
    remove_lock_timer (listener);

    listener->priv->lock_timer_id = g_timeout_add_seconds(listener->priv->lock_timeout,
                                                          (GSourceFunc)lock_timer,
                                                          listener);
}

static GdkFilterReturn
xroot_filter (GdkXEvent *xevent,
              GdkEvent  *event,
              gpointer   data)
{
    GSListenerX11 *listener;
    XEvent *ev;

    g_return_val_if_fail (data != NULL, GDK_FILTER_CONTINUE);
    g_return_val_if_fail (GS_IS_LISTENER_X11 (data), GDK_FILTER_CONTINUE);

    listener = GS_LISTENER_X11 (data);

    ev = xevent;

    switch (ev->xany.type) {
    default:
        /* extension events */
#ifdef HAVE_MIT_SAVER_EXTENSION
        if (ev->xany.type == (listener->priv->scrnsaver_event_base + ScreenSaverNotify)) {
            XScreenSaverNotifyEvent *xssne = (XScreenSaverNotifyEvent *) ev;
            switch (xssne->state) {
                case ScreenSaverOff:
                    // Reset the lock timer
                    gs_debug("ScreenSaver timer reset");
                    reset_lock_timer (listener);
                    break;

                case ScreenSaverDisabled:
                    // Disable the lock timer
                    gs_debug ("ScreenSaver timer disabled");
                    remove_lock_timer (listener);
                    break;

                case ScreenSaverOn:
                    // lock now!
                    gs_debug("ScreenSaver on");
                    g_signal_emit (listener, signals [LOCK], 0);
                    break;

            }
        }
#endif
        break;
    }

    return GDK_FILTER_CONTINUE;
}


gboolean
gs_listener_x11_acquire (GSListenerX11 *listener)
{
    GdkDisplay *display;
    GdkScreen *screen;
    GdkWindow *window;
#ifdef HAVE_MIT_SAVER_EXTENSION
    int scrnsaver_error_base;
    unsigned long events;
#endif

    g_return_val_if_fail (listener != NULL, FALSE);

    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen (display);
    window = gdk_screen_get_root_window (screen);

#ifdef HAVE_MIT_SAVER_EXTENSION
    gdk_error_trap_push ();
    if (XScreenSaverQueryExtension (GDK_DISPLAY_XDISPLAY (display), &listener->priv->scrnsaver_event_base, &scrnsaver_error_base)) {
        events = ScreenSaverNotifyMask;
        XScreenSaverSelectInput (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window), events);
        gs_debug ("ScreenSaver Registered");
    } else {
        gs_debug ("ScreenSaverExtension not found");
    }

#if GTK_CHECK_VERSION(3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif
#endif

    gdk_window_add_filter (NULL, (GdkFilterFunc)xroot_filter, listener);

    return TRUE;
}

void
gs_listener_x11_set_lock_after (GSListenerX11 *listener,
                gint lock_after)
{
    gs_debug ("Lock timout updated to %i minutes", lock_after);
    listener->priv->lock_timeout = lock_after * 60;
    reset_lock_timer(listener);
}

static void
gs_listener_x11_init (GSListenerX11 *listener)
{
    listener->priv = GS_LISTENER_X11_GET_PRIVATE (listener);

    listener->priv->lock_timeout = 300;
}

static void
gs_listener_x11_finalize (GObject *object)
{
    GSListenerX11 *listener;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_LISTENER_X11 (object));

    listener = GS_LISTENER_X11 (object);

    g_return_if_fail (listener->priv != NULL);

    gdk_window_remove_filter (NULL, (GdkFilterFunc)xroot_filter, NULL);

    G_OBJECT_CLASS (gs_listener_x11_parent_class)->finalize (object);
}

GSListenerX11 *
gs_listener_x11_new (void)
{
    GSListenerX11 *listener;

    listener = g_object_new (GS_TYPE_LISTENER_X11, NULL);

    return GS_LISTENER_X11 (listener);
}
