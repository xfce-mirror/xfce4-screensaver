/* xfce-bg-crossfade.h - fade window background between two pixmaps

   Copyright 2008, Red Hat, Inc.

   This file is part of the Mate Library.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
   Floor, Boston, MA 02110-1301 US.

   Author: Ray Strode <rstrode@redhat.com>
*/

#ifndef __XFCE_BG_CROSSFADE_H__
#define __XFCE_BG_CROSSFADE_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFCE_TYPE_BG_CROSSFADE            (xfce_bg_crossfade_get_type ())
#define XFCE_BG_CROSSFADE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_BG_CROSSFADE, XfceBGCrossfade))
#define XFCE_BG_CROSSFADE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  XFCE_TYPE_BG_CROSSFADE, XfceBGCrossfadeClass))
#define XFCE_IS_BG_CROSSFADE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_BG_CROSSFADE))
#define XFCE_IS_BG_CROSSFADE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  XFCE_TYPE_BG_CROSSFADE))
#define XFCE_BG_CROSSFADE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  XFCE_TYPE_BG_CROSSFADE, XfceBGCrossfadeClass))

typedef struct _XfceBGCrossfadePrivate XfceBGCrossfadePrivate;
typedef struct _XfceBGCrossfade XfceBGCrossfade;
typedef struct _XfceBGCrossfadeClass XfceBGCrossfadeClass;

struct _XfceBGCrossfade
{
	GObject parent_object;

	XfceBGCrossfadePrivate *priv;
};

struct _XfceBGCrossfadeClass
{
	GObjectClass parent_class;

	void (* finished) (XfceBGCrossfade *fade, GdkWindow *window);
};

GType             xfce_bg_crossfade_get_type              (void);
XfceBGCrossfade *xfce_bg_crossfade_new (int width, int height);

gboolean          xfce_bg_crossfade_set_start_surface (XfceBGCrossfade *fade,
						       cairo_surface_t *surface);
gboolean          xfce_bg_crossfade_set_end_surface (XfceBGCrossfade *fade,
						     cairo_surface_t *surface);

void              xfce_bg_crossfade_start (XfceBGCrossfade *fade,
                                           GdkWindow        *window);
void              xfce_bg_crossfade_start_widget (XfceBGCrossfade *fade,
                                                  GtkWidget       *widget);
gboolean          xfce_bg_crossfade_is_started (XfceBGCrossfade *fade);
void              xfce_bg_crossfade_stop (XfceBGCrossfade *fade);

G_END_DECLS

#endif
