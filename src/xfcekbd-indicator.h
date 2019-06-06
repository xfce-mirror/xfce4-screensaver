/*
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
 * Copyright (C) 2018 Sean Davis <bluesabre@xfce.org>
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
 * License along with this library; see the file COPYING.LGPL.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef SRC_XFCEKBD_INDICATOR_H_
#define SRC_XFCEKBD_INDICATOR_H_

#include <gtk/gtk.h>

#include <libxklavier/xklavier.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _XfcekbdIndicator XfcekbdIndicator;
typedef struct _XfcekbdIndicatorPrivate XfcekbdIndicatorPrivate;
typedef struct _XfcekbdIndicatorClass XfcekbdIndicatorClass;

#define XFCEKBD_TYPE_INDICATOR             (xfcekbd_indicator_get_type ())
#define XFCEKBD_INDICATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCEKBD_TYPE_INDICATOR, \
                                            XfcekbdIndicator))
#define XFCEKBD_INDICATOR_CLASS(obj)       (G_TYPE_CHECK_CLASS_CAST ((obj), XFCEKBD_TYPE_INDICATOR, \
                                            XfcekbdIndicatorClass))
#define XFCEKBD_IS_INDICATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCEKBD_TYPE_INDICATOR))
#define XFCEKBD_IS_INDICATOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE ((obj), XFCEKBD_TYPE_INDICATOR))
#define XFCEKBD_INDICATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCEKBD_TYPE_INDICATOR, \
                                            XfcekbdIndicatorClass))

struct _XfcekbdIndicator {
    GtkNotebook              parent;
    XfcekbdIndicatorPrivate *priv;
};

struct _XfcekbdIndicatorClass {
    GtkNotebookClass   parent_class;

    void (*reinit_ui)  (XfcekbdIndicator * gki);
};

extern GType        xfcekbd_indicator_get_type              (void);

extern GtkWidget *  xfcekbd_indicator_new                   (void);

extern void         xfcekbd_indicator_set_parent_tooltips   (XfcekbdIndicator *gki,
                                                             gboolean          spt);

#ifdef __cplusplus
}
#endif
#endif /* SRC_XFCEKBD_INDICATOR_H_ */
