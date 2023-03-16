/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-
 *
 * gs-theme-window.h - special toplevel for screensavers
 *
 * Copyright (C) 2005 Ray Strode <rstrode@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; see the file
 * COPYING.LGPL.  If not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Originally written by: Ray Strode <rstrode@redhat.com>
 */

#ifndef SAVERS_GS_THEME_WINDOW_H_
#define SAVERS_GS_THEME_WINDOW_H_

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GS_TYPE_THEME_WINDOW            (gs_theme_window_get_type ())
#define GS_THEME_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GS_TYPE_THEME_WINDOW, GSThemeWindow))
#define GS_THEME_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GS_TYPE_THEME_WINDOW, GSThemeWindowClass))
#define GS_IS_WINDOW(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GS_TYPE_THEME_WINDOW))
#define GS_IS_WINDOW_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GS_TYPE_THEME_WINDOW))
#define GS_THEME_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GS_TYPE_THEME_WINDOW, GSThemeWindowClass))

typedef struct _GSThemeWindow GSThemeWindow;
typedef struct _GSThemeWindowClass GSThemeWindowClass;

struct _GSThemeWindow
{
    GtkWindow parent;

    GdkWindow *foreign_window;

    /*< private >*/
    /* reserved for priv pointer */
    gpointer  reserved;
};

struct _GSThemeWindowClass
{
    GtkWindowClass parent_class;

    /* for signals later if needed */
    gpointer       reserved_1;
    gpointer       reserved_2;
    gpointer       reserved_3;
    gpointer       reserved_4;
};

#ifndef GS_HIDE_FUNCTION_DECLARATIONS
GType         gs_theme_window_get_type (void);
GtkWidget    *gs_theme_window_new      (void);
#endif

G_END_DECLS
#endif /* SAVERS_GS_THEME_WINDOW_H_ */
