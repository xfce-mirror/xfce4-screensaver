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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef SRC_GS_LISTENER_DBUS_H_
#define SRC_GS_LISTENER_DBUS_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define GS_TYPE_LISTENER_DBUS         (gs_listener_dbus_get_type ())
#define GS_LISTENER_DBUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_LISTENER_DBUS, GSListenerDBus))
#define GS_LISTENER_DBUS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_LISTENER_DBUS, GSListenerDBusClass))
#define GS_IS_LISTENER_DBUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_LISTENER_DBUS))
#define GS_IS_LISTENER_DBUS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_LISTENER_DBUS))
#define GS_LISTENER_DBUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_LISTENER_DBUS, GSListenerDBusClass))

typedef struct GSListenerDBusPrivate GSListenerDBusPrivate;

typedef struct
{
    GObject                parent;
    GSListenerDBusPrivate *priv;
} GSListenerDBus;

typedef struct
{
    GObjectClass       parent_class;

    void            (* lock)                     (GSListenerDBus *listener);
    void            (* cycle)                    (GSListenerDBus *listener);
    void            (* quit)                     (GSListenerDBus *listener);
    void            (* simulate_user_activity)   (GSListenerDBus *listener);
    gboolean        (* active_changed)           (GSListenerDBus *listener,
                                                  gboolean        active);
    void            (* throttle_changed)         (GSListenerDBus *listener,
                                                  gboolean        throttled);
    void            (* show_message)             (GSListenerDBus *listener,
                                                  const char     *summary,
                                                  const char     *body,
                                                  const char     *icon);
} GSListenerDBusClass;

typedef enum
{
    GS_LISTENER_DBUS_ERROR_SERVICE_UNAVAILABLE,
    GS_LISTENER_DBUS_ERROR_ACQUISITION_FAILURE,
    GS_LISTENER_DBUS_ERROR_ACTIVATION_FAILURE
} GSListenerDBusError;

#define GS_LISTENER_DBUS_ERROR gs_listener_dbus_error_quark ()

GQuark          gs_listener_dbus_error_quark                     (void);

GType           gs_listener_dbus_get_type                        (void);

GSListenerDBus *gs_listener_dbus_new                             (void);
gboolean        gs_listener_dbus_acquire                         (GSListenerDBus *listener,
                                                                  GError        **error);
gboolean        gs_listener_dbus_activate_saver                  (GSListenerDBus *listener,
                                                                  gboolean        active);
gboolean        gs_listener_dbus_is_inhibited                    (GSListenerDBus *listener);

G_END_DECLS

#endif /* SRC_GS_LISTENER_DBUS_H_ */
