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

#ifndef SRC_GS_LISTENER_X11_H_
#define SRC_GS_LISTENER_X11_H_

#include "gs-listener.h"

G_BEGIN_DECLS

#define GS_TYPE_LISTENER_X11         (gs_listener_x11_get_type ())
#define GS_LISTENER_X11(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_LISTENER_X11, GSListenerX11))
#define GS_LISTENER_X11_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_LISTENER_X11, GSListenerX11Class))
#define GS_IS_LISTENER_X11(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_LISTENER_X11))
#define GS_IS_LISTENER_X11_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_LISTENER_X11))
#define GS_LISTENER_X11_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_LISTENER_X11, GSListenerX11Class))

typedef struct GSListenerX11Private GSListenerX11Private;

typedef struct
{
    GSListener            parent;
    GSListenerX11Private *priv;
} GSListenerX11;

typedef struct
{
    GSListenerClass parent_class;
} GSListenerX11Class;

GType                     gs_listener_x11_get_type                  (void);

G_END_DECLS

#endif /* SRC_GS_LISTENER_X11_H_ */
