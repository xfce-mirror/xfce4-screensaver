/* xfce-bg.h -

   Copyright (C) 2007 Red Hat, Inc.
   Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>

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
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA  02110-1301, USA.

   Authors: Soren Sandmann <sandmann@redhat.com>
	    Jasmine Hassan <jasmine.aura@gmail.com>
*/

#ifndef __XFCE_BG_H__
#define __XFCE_BG_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include "xfce-desktop-thumbnail.h"
#include "xfce-bg-crossfade.h"

G_BEGIN_DECLS

#define XFCE_TYPE_BG            (xfce_bg_get_type ())
#define XFCE_BG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_BG, XfceBG))
#define XFCE_BG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  XFCE_TYPE_BG, XfceBGClass))
#define XFCE_IS_BG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_BG))
#define XFCE_IS_BG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  XFCE_TYPE_BG))
#define XFCE_BG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  XFCE_TYPE_BG, XfceBGClass))

#define XFCE_BG_SCHEMA "org.mate.background"

/* whether to draw the desktop bg */
#define XFCE_BG_KEY_DRAW_BACKGROUND	"draw-background"

/* whether Caja or mate-settings-daemon draw the desktop */
#define XFCE_BG_KEY_SHOW_DESKTOP	"show-desktop-icons"

/* whether to fade when changing background (By Caja/m-s-d) */
#define XFCE_BG_KEY_BACKGROUND_FADE	"background-fade"

#define XFCE_BG_KEY_PRIMARY_COLOR	"primary-color"
#define XFCE_BG_KEY_SECONDARY_COLOR	"secondary-color"
#define XFCE_BG_KEY_COLOR_TYPE		"color-shading-type"
#define XFCE_BG_KEY_PICTURE_PLACEMENT	"picture-options"
#define XFCE_BG_KEY_PICTURE_OPACITY	"picture-opacity"
#define XFCE_BG_KEY_PICTURE_FILENAME	"picture-filename"

typedef struct _XfceBG XfceBG;
typedef struct _XfceBGClass XfceBGClass;

typedef enum {
	XFCE_BG_COLOR_SOLID,
	XFCE_BG_COLOR_H_GRADIENT,
	XFCE_BG_COLOR_V_GRADIENT
} XfceBGColorType;

typedef enum {
	XFCE_BG_PLACEMENT_TILED,
	XFCE_BG_PLACEMENT_ZOOMED,
	XFCE_BG_PLACEMENT_CENTERED,
	XFCE_BG_PLACEMENT_SCALED,
	XFCE_BG_PLACEMENT_FILL_SCREEN,
	XFCE_BG_PLACEMENT_SPANNED
} XfceBGPlacement;

GType            xfce_bg_get_type              (void);
XfceBG *         xfce_bg_new                   (void);
void             xfce_bg_load_from_preferences (XfceBG               *bg);
void             xfce_bg_load_from_system_preferences  (XfceBG       *bg);
void             xfce_bg_load_from_system_gsettings    (XfceBG       *bg,
							GSettings    *settings,
							gboolean      reset_apply);
void             xfce_bg_load_from_gsettings   (XfceBG               *bg,
						GSettings            *settings);
void             xfce_bg_save_to_preferences   (XfceBG               *bg);
void             xfce_bg_save_to_gsettings     (XfceBG               *bg,
						GSettings            *settings);

/* Setters */
void             xfce_bg_set_filename          (XfceBG               *bg,
						 const char            *filename);
void             xfce_bg_set_placement         (XfceBG               *bg,
						 XfceBGPlacement       placement);
void             xfce_bg_set_color             (XfceBG               *bg,
						 XfceBGColorType       type,
						 GdkRGBA              *primary,
						 GdkRGBA              *secondary);
void		 xfce_bg_set_draw_background   (XfceBG		     *bg,
						gboolean	      draw_background);
/* Getters */
gboolean	 xfce_bg_get_draw_background   (XfceBG		     *bg);
XfceBGPlacement  xfce_bg_get_placement         (XfceBG               *bg);
void		 xfce_bg_get_color             (XfceBG               *bg,
						 XfceBGColorType      *type,
						 GdkRGBA              *primary,
						 GdkRGBA              *secondary);
const gchar *    xfce_bg_get_filename          (XfceBG               *bg);

/* Drawing and thumbnailing */
void             xfce_bg_draw                  (XfceBG               *bg,
						 GdkPixbuf             *dest,
						 GdkScreen	       *screen,
                                                 gboolean               is_root);

cairo_surface_t *xfce_bg_create_surface        (XfceBG               *bg,
						GdkWindow            *window,
						int                   width,
						int                   height,
						gboolean              root);

cairo_surface_t *xfce_bg_create_surface_scale  (XfceBG               *bg,
						GdkWindow            *window,
						int                   width,
						int                   height,
						int                   scale,
						gboolean              root);

gboolean         xfce_bg_get_image_size        (XfceBG               *bg,
						 XfceDesktopThumbnailFactory *factory,
                                                 int                    best_width,
                                                 int                    best_height,
						 int                   *width,
						 int                   *height);
GdkPixbuf *      xfce_bg_create_thumbnail      (XfceBG               *bg,
						 XfceDesktopThumbnailFactory *factory,
						 GdkScreen             *screen,
						 int                    dest_width,
						 int                    dest_height);
gboolean         xfce_bg_is_dark               (XfceBG               *bg,
                                                 int                    dest_width,
						 int                    dest_height);
gboolean         xfce_bg_has_multiple_sizes    (XfceBG               *bg);
gboolean         xfce_bg_changes_with_time     (XfceBG               *bg);
GdkPixbuf *      xfce_bg_create_frame_thumbnail (XfceBG              *bg,
						 XfceDesktopThumbnailFactory *factory,
						 GdkScreen             *screen,
						 int                    dest_width,
						 int                    dest_height,
						 int                    frame_num);

/* Set a surface as root - not a XfceBG method. At some point
 * if we decide to stabilize the API then we may want to make
 * these object methods, drop xfce_bg_create_surface, etc.
 */
void             xfce_bg_set_surface_as_root   (GdkScreen            *screen,
						cairo_surface_t    *surface);
XfceBGCrossfade *xfce_bg_set_surface_as_root_with_crossfade (GdkScreen       *screen,
							     cairo_surface_t *surface);
cairo_surface_t *xfce_bg_get_surface_from_root (GdkScreen *screen);

G_END_DECLS

#endif
