/* xfce-bg.h -

   Copyright (C) 2007 Red Hat, Inc.
   Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>
   Copyright (C) 2018 Sean Davis <bluesabre@xfce.org>

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
   Boston, MA  02110-1301, USA.

   Authors: Soren Sandmann <sandmann@redhat.com>
            Jasmine Hassan <jasmine.aura@gmail.com>
*/

#ifndef SRC_XFCE_BG_H_
#define SRC_XFCE_BG_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define XFCE_TYPE_BG            (xfce_bg_get_type ())
#define XFCE_BG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_BG, XfceBG))
#define XFCE_BG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  XFCE_TYPE_BG, XfceBGClass))
#define XFCE_IS_BG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_BG))
#define XFCE_IS_BG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  XFCE_TYPE_BG))
#define XFCE_BG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  XFCE_TYPE_BG, XfceBGClass))

/* whether to draw the desktop bg */
#define XFCE_BG_KEY_DRAW_BACKGROUND     "draw-background"

/* whether Thunar or xfce-settings-daemon draw the desktop */
#define XFCE_BG_KEY_SHOW_DESKTOP        "show-desktop-icons"

/* whether to fade when changing background (By Thunar/m-s-d) */
#define XFCE_BG_KEY_BACKGROUND_FADE     "background-fade"

#define XFCE_BG_KEY_PRIMARY_COLOR       "primary-color"
#define XFCE_BG_KEY_SECONDARY_COLOR     "secondary-color"
#define XFCE_BG_KEY_COLOR_TYPE          "color-shading-type"
#define XFCE_BG_KEY_PICTURE_PLACEMENT   "picture-options"
#define XFCE_BG_KEY_PICTURE_OPACITY     "picture-opacity"
#define XFCE_BG_KEY_PICTURE_FILENAME    "picture-filename"

typedef struct _XfceBG XfceBG;
typedef struct _XfceBGClass XfceBGClass;

typedef enum {
    XFCE_BG_COLOR_SOLID,
    XFCE_BG_COLOR_H_GRADIENT,
    XFCE_BG_COLOR_V_GRADIENT,
    XFCE_BG_COLOR_TRANSPARENT
} XfceBGColorType;

typedef enum {
    XFCE_BG_PLACEMENT_NONE,
    XFCE_BG_PLACEMENT_CENTERED,
    XFCE_BG_PLACEMENT_TILED,
    XFCE_BG_PLACEMENT_FILL_SCREEN,
    XFCE_BG_PLACEMENT_SCALED,
    XFCE_BG_PLACEMENT_ZOOMED,
    XFCE_BG_PLACEMENT_SPANNED
} XfceBGPlacement;

GType             xfce_bg_get_type              (void);
XfceBG *          xfce_bg_new                   (void);
void              xfce_bg_load_from_preferences (XfceBG     *bg,
                                                 GdkMonitor *monitor);

/* Drawing and thumbnailing */
GdkPixbuf *       xfce_bg_get_pixbuf            (XfceBG     *bg,
                                                 int         screen_width,
                                                 int         screen_height,
                                                 int         monitor_width,
                                                 int         monitor_height,
                                                 int         scale);

G_END_DECLS

#endif /* SRC_XFCE_BG_H_ */
