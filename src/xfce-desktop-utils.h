/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/* xfce-ditem.h - Utilities for the Xfce Desktop

   Copyright (C) 1998 Tom Tromey
   Copyright (C) 2018 Sean Davis <bluesabre@xfce.org>
   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA  02110-1301, USA.  */
/*
  @NOTATION@
 */

#ifndef SRC_XFCE_DESKTOP_UTILS_H_
#define SRC_XFCE_DESKTOP_UTILS_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* replace gdk_spawn_command_line_on_screen, not available in GTK3 */
gboolean xfce_gdk_spawn_command_line_on_screen (GdkScreen    *screen,
                                                const gchar  *command,
                                                GError      **error);

G_END_DECLS

#endif /* SRC_XFCE_DESKTOP_UTILS_H_ */
