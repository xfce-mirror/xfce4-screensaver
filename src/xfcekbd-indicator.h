/*
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __XFCEKBD_INDICATOR_H__
#define __XFCEKBD_INDICATOR_H__

#include <gtk/gtk.h>

#include <libxklavier/xklavier.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct _XfcekbdIndicator XfcekbdIndicator;
	typedef struct _XfcekbdIndicatorPrivate XfcekbdIndicatorPrivate;
	typedef struct _XfcekbdIndicatorClass XfcekbdIndicatorClass;

#define XFCEKBD_TYPE_INDICATOR             (xfcekbd_indicator_get_type ())
#define XFCEKBD_INDICATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCEKBD_TYPE_INDICATOR, XfcekbdIndicator))
#define XFCEKBD_INDICATOR_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), XFCEKBD_TYPE_INDICATOR,  XfcekbdIndicatorClass))
#define XFCEKBD_IS_INDICATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCEKBD_TYPE_INDICATOR))
#define XFCEKBD_IS_INDICATOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), XFCEKBD_TYPE_INDICATOR))
#define XFCEKBD_INDICATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCEKBD_TYPE_INDICATOR, XfcekbdIndicatorClass))

	struct _XfcekbdIndicator {
		GtkNotebook parent;
		XfcekbdIndicatorPrivate *priv;
	};

	struct _XfcekbdIndicatorClass {
		GtkNotebookClass parent_class;

		void (*reinit_ui) (XfcekbdIndicator * gki);
	};

	extern GType xfcekbd_indicator_get_type (void);

	extern GtkWidget *xfcekbd_indicator_new (void);

	extern void xfcekbd_indicator_reinit_ui (XfcekbdIndicator * gki);

	extern void xfcekbd_indicator_set_angle (XfcekbdIndicator * gki,
					      gdouble angle);

	extern XklEngine *xfcekbd_indicator_get_xkl_engine (void);

	extern gchar **xfcekbd_indicator_get_group_names (void);

	extern gchar *xfcekbd_indicator_get_image_filename (guint group);

	extern gdouble xfcekbd_indicator_get_max_width_height_ratio (void);

	extern void
	 xfcekbd_indicator_set_parent_tooltips (XfcekbdIndicator *
					     gki, gboolean ifset);

	extern void
	 xfcekbd_indicator_set_tooltips_format (const gchar str[]);

#ifdef __cplusplus
}
#endif
#endif
