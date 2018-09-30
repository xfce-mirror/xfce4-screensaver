/* xfce-rr-labeler.h - Utility to label monitors to identify them
 * while they are being configured.
 *
 * Copyright 2008, Novell, Inc.
 *
 * This file is part of the Mate Library.
 *
 * The Mate Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Mate Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Mate Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Federico Mena-Quintero <federico@novell.com>
 */

#ifndef XFCE_RR_LABELER_H
#define XFCE_RR_LABELER_H

#include "xfce-rr-config.h"

#define XFCE_TYPE_RR_LABELER            (xfce_rr_labeler_get_type ())
#define XFCE_RR_LABELER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_RR_LABELER, XfceRRLabeler))
#define XFCE_RR_LABELER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  XFCE_TYPE_RR_LABELER, XfceRRLabelerClass))
#define XFCE_IS_RR_LABELER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_RR_LABELER))
#define XFCE_IS_RR_LABELER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  XFCE_TYPE_RR_LABELER))
#define XFCE_RR_LABELER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  XFCE_TYPE_RR_LABELER, XfceRRLabelerClass))

typedef struct _XfceRRLabeler XfceRRLabeler;
typedef struct _XfceRRLabelerClass XfceRRLabelerClass;
typedef struct _XfceRRLabelerPrivate XfceRRLabelerPrivate;

struct _XfceRRLabeler {
	GObject parent;

	/*< private >*/
	XfceRRLabelerPrivate *priv;
};

struct _XfceRRLabelerClass {
	GObjectClass parent_class;
};

GType xfce_rr_labeler_get_type (void);

XfceRRLabeler *xfce_rr_labeler_new (XfceRRConfig *config);

void xfce_rr_labeler_hide (XfceRRLabeler *labeler);

void xfce_rr_labeler_get_rgba_for_output (XfceRRLabeler *labeler, XfceRROutputInfo *output, GdkRGBA *color_out);

#endif
