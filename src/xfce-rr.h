/* xfce-rr.h
 *
 * Copyright 2007, 2008, Red Hat, Inc.
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
 * Author: Soren Sandmann <sandmann@redhat.com>
 */
#ifndef XFCE_RR_H
#define XFCE_RR_H

#include <glib.h>
#include <gdk/gdk.h>

typedef struct XfceRRScreenPrivate XfceRRScreenPrivate;
typedef struct XfceRROutput XfceRROutput;
typedef struct XfceRRCrtc XfceRRCrtc;
typedef struct XfceRRMode XfceRRMode;

typedef struct {
    GObject parent;

    XfceRRScreenPrivate* priv;
} XfceRRScreen;

typedef struct {
    GObjectClass parent_class;

        void (* changed) (void);
} XfceRRScreenClass;

typedef enum
{
    XFCE_RR_ROTATION_0 =	(1 << 0),
    XFCE_RR_ROTATION_90 =	(1 << 1),
    XFCE_RR_ROTATION_180 =	(1 << 2),
    XFCE_RR_ROTATION_270 =	(1 << 3),
    XFCE_RR_REFLECT_X =	(1 << 4),
    XFCE_RR_REFLECT_Y =	(1 << 5)
} XfceRRRotation;

/* Error codes */

#define XFCE_RR_ERROR (xfce_rr_error_quark ())

GQuark xfce_rr_error_quark (void);

typedef enum {
    XFCE_RR_ERROR_UNKNOWN,		/* generic "fail" */
    XFCE_RR_ERROR_NO_RANDR_EXTENSION,	/* RANDR extension is not present */
    XFCE_RR_ERROR_RANDR_ERROR,		/* generic/undescribed error from the underlying XRR API */
    XFCE_RR_ERROR_BOUNDS_ERROR,	/* requested bounds of a CRTC are outside the maximum size */
    XFCE_RR_ERROR_CRTC_ASSIGNMENT,	/* could not assign CRTCs to outputs */
    XFCE_RR_ERROR_NO_MATCHING_CONFIG,	/* none of the saved configurations matched the current configuration */
} XfceRRError;

#define XFCE_RR_CONNECTOR_TYPE_PANEL "Panel"  /* This is a laptop's built-in LCD */

#define XFCE_TYPE_RR_SCREEN                  (xfce_rr_screen_get_type())
#define XFCE_RR_SCREEN(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_RR_SCREEN, XfceRRScreen))
#define XFCE_IS_RR_SCREEN(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_RR_SCREEN))
#define XFCE_RR_SCREEN_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_RR_SCREEN, XfceRRScreenClass))
#define XFCE_IS_RR_SCREEN_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_RR_SCREEN))
#define XFCE_RR_SCREEN_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_RR_SCREEN, XfceRRScreenClass))

#define XFCE_TYPE_RR_OUTPUT (xfce_rr_output_get_type())
#define XFCE_TYPE_RR_CRTC   (xfce_rr_crtc_get_type())
#define XFCE_TYPE_RR_MODE   (xfce_rr_mode_get_type())

GType xfce_rr_screen_get_type (void);
GType xfce_rr_output_get_type (void);
GType xfce_rr_crtc_get_type (void);
GType xfce_rr_mode_get_type (void);

/* XfceRRScreen */
XfceRRScreen * xfce_rr_screen_new                (GdkScreen             *screen,
						    GError               **error);
XfceRRCrtc **  xfce_rr_screen_list_crtcs         (XfceRRScreen         *screen);
gboolean        xfce_rr_screen_refresh            (XfceRRScreen         *screen,
						    GError               **error);

/* XfceRRCrtc */
XfceRRMode *   xfce_rr_crtc_get_current_mode     (XfceRRCrtc           *crtc);

gboolean        xfce_rr_crtc_get_gamma            (XfceRRCrtc           *crtc,
						    int                   *size,
						    unsigned short       **red,
						    unsigned short       **green,
						    unsigned short       **blue);
void            xfce_rr_crtc_set_gamma            (XfceRRCrtc           *crtc,
						    int                    size,
						    unsigned short        *red,
						    unsigned short        *green,
						    unsigned short        *blue);
#endif /* XFCE_RR_H */
