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

#ifndef SRC_GS_GRAB_H_
#define SRC_GS_GRAB_H_

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GS_TYPE_GRAB (gs_grab_get_type ())
#define GS_GRAB(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_GRAB, GSGrab))
#define GS_GRAB_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GS_TYPE_GRAB, GSGrabClass))
#define GS_IS_GRAB(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_GRAB))
#define GS_IS_GRAB_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_GRAB))
#define GS_GRAB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_GRAB, GSGrabClass))

typedef struct GSGrabPrivate GSGrabPrivate;

typedef struct
{
    GObject parent;
    GSGrabPrivate *priv;
} GSGrab;

typedef struct
{
    GObjectClass parent_class;
} GSGrabClass;

GType
gs_grab_get_type (void);

GSGrab *
gs_grab_new (void);

void
gs_grab_release (GSGrab *grab,
                 gboolean flush);

gboolean
gs_grab_grab_window (GSGrab *grab,
                     GdkWindow *window,
                     GdkDisplay *display,
                     gboolean no_pointer_grab,
                     gboolean hide_cursor);

gboolean
gs_grab_grab_root (GSGrab *grab,
                   gboolean no_pointer_grab,
                   gboolean hide_cursor);

void
gs_grab_move_to_window (GSGrab *grab,
                        GdkWindow *window,
                        GdkDisplay *display,
                        gboolean no_pointer_grab,
                        gboolean hide_cursor);

void
gs_grab_reset (GSGrab *grab);

G_END_DECLS

#endif /* SRC_GS_GRAB_H_ */
