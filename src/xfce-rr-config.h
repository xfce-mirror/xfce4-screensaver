/* xfce-rr-config.h
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
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
#ifndef XFCE_RR_CONFIG_H
#define XFCE_RR_CONFIG_H

#include "xfce-rr.h"
#include <glib.h>
#include <glib-object.h>

typedef struct XfceRROutputInfoPrivate XfceRROutputInfoPrivate;
typedef struct XfceRRConfigPrivate XfceRRConfigPrivate;

typedef struct
{
    GObject parent;

    /*< private >*/
    XfceRROutputInfoPrivate *priv;
} XfceRROutputInfo;

typedef struct
{
    GObjectClass parent_class;
} XfceRROutputInfoClass;

#define XFCE_TYPE_RR_OUTPUT_INFO                  (xfce_rr_output_info_get_type())
#define XFCE_RR_OUTPUT_INFO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_RR_OUTPUT_INFO, XfceRROutputInfo))
#define XFCE_IS_RR_OUTPUT_INFO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_RR_OUTPUT_INFO))
#define XFCE_RR_OUTPUT_INFO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_RR_OUTPUT_INFO, XfceRROutputInfoClass))
#define XFCE_IS_RR_OUTPUT_INFO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_RR_OUTPUT_INFO))
#define XFCE_RR_OUTPUT_INFO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_RR_OUTPUT_INFO, XfceRROutputInfoClass))

GType xfce_rr_output_info_get_type (void);

char *xfce_rr_output_info_get_name (XfceRROutputInfo *self);

gboolean xfce_rr_output_info_is_active  (XfceRROutputInfo *self);
void     xfce_rr_output_info_set_active (XfceRROutputInfo *self, gboolean active);


void xfce_rr_output_info_get_geometry (XfceRROutputInfo *self, int *x, int *y, int *width, int *height);
void xfce_rr_output_info_set_geometry (XfceRROutputInfo *self, int  x, int  y, int  width, int  height);

int  xfce_rr_output_info_get_refresh_rate (XfceRROutputInfo *self);
void xfce_rr_output_info_set_refresh_rate (XfceRROutputInfo *self, int rate);

XfceRRRotation xfce_rr_output_info_get_rotation (XfceRROutputInfo *self);
void            xfce_rr_output_info_set_rotation (XfceRROutputInfo *self, XfceRRRotation rotation);

gboolean xfce_rr_output_info_is_connected     (XfceRROutputInfo *self);
void     xfce_rr_output_info_get_vendor       (XfceRROutputInfo *self, gchar* vendor);
guint    xfce_rr_output_info_get_product      (XfceRROutputInfo *self);
guint    xfce_rr_output_info_get_serial       (XfceRROutputInfo *self);
double   xfce_rr_output_info_get_aspect_ratio (XfceRROutputInfo *self);
char    *xfce_rr_output_info_get_display_name (XfceRROutputInfo *self);

gboolean xfce_rr_output_info_get_primary (XfceRROutputInfo *self);
void     xfce_rr_output_info_set_primary (XfceRROutputInfo *self, gboolean primary);

int xfce_rr_output_info_get_preferred_width  (XfceRROutputInfo *self);
int xfce_rr_output_info_get_preferred_height (XfceRROutputInfo *self);

typedef struct
{
    GObject parent;

    /*< private >*/
    XfceRRConfigPrivate *priv;
} XfceRRConfig;

typedef struct
{
    GObjectClass parent_class;
} XfceRRConfigClass;

#define XFCE_TYPE_RR_CONFIG                  (xfce_rr_config_get_type())
#define XFCE_RR_CONFIG(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_RR_CONFIG, XfceRRConfig))
#define XFCE_IS_RR_CONFIG(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_RR_CONFIG))
#define XFCE_RR_CONFIG_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_RR_CONFIG, XfceRRConfigClass))
#define XFCE_IS_RR_CONFIG_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_RR_CONFIG))
#define XFCE_RR_CONFIG_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_RR_CONFIG, XfceRRConfigClass))

GType               xfce_rr_config_get_type     (void);

XfceRRConfig      *xfce_rr_config_new_current  (XfceRRScreen  *screen,
						  GError        **error);
XfceRRConfig      *xfce_rr_config_new_stored   (XfceRRScreen  *screen,
						  GError        **error);
gboolean                xfce_rr_config_load_current (XfceRRConfig  *self,
						      GError        **error);
gboolean                xfce_rr_config_load_filename (XfceRRConfig  *self,
						       const gchar    *filename,
						       GError        **error);
gboolean            xfce_rr_config_match        (XfceRRConfig  *config1,
						  XfceRRConfig  *config2);
gboolean            xfce_rr_config_equal	 (XfceRRConfig  *config1,
						  XfceRRConfig  *config2);
gboolean            xfce_rr_config_save         (XfceRRConfig  *configuration,
						  GError        **error);
void                xfce_rr_config_sanitize     (XfceRRConfig  *configuration);
gboolean            xfce_rr_config_ensure_primary (XfceRRConfig  *configuration);

gboolean	    xfce_rr_config_apply_with_time (XfceRRConfig  *configuration,
						     XfceRRScreen  *screen,
						     guint32         timestamp,
						     GError        **error);

gboolean            xfce_rr_config_apply_from_filename_with_time (XfceRRScreen  *screen,
								   const char     *filename,
								   guint32         timestamp,
								   GError        **error);

gboolean            xfce_rr_config_applicable   (XfceRRConfig  *configuration,
						  XfceRRScreen  *screen,
						  GError        **error);

gboolean            xfce_rr_config_get_clone    (XfceRRConfig  *configuration);
void                xfce_rr_config_set_clone    (XfceRRConfig  *configuration, gboolean clone);
XfceRROutputInfo **xfce_rr_config_get_outputs  (XfceRRConfig  *configuration);

char *xfce_rr_config_get_backup_filename (void);
char *xfce_rr_config_get_intended_filename (void);

#endif
