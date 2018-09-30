/* xfce-rr-output-info.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2010 Giovanni Campagna
 *
 * This file is part of the Mate Desktop Library.
 *
 * The Mate Desktop Library is free software; you can redistribute it and/or
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
 * License along with the Mate Desktop Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */



#include <config.h>

#include "xfce-rr-config.h"

#include "edid.h"
#include "xfce-rr-private.h"

G_DEFINE_TYPE (XfceRROutputInfo, xfce_rr_output_info, G_TYPE_OBJECT)

static void
xfce_rr_output_info_init (XfceRROutputInfo *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, XFCE_TYPE_RR_OUTPUT_INFO, XfceRROutputInfoPrivate);

    self->priv->name = NULL;
    self->priv->on = FALSE;
    self->priv->display_name = NULL;
}

static void
xfce_rr_output_info_finalize (GObject *gobject)
{
    XfceRROutputInfo *self = XFCE_RR_OUTPUT_INFO (gobject);

    g_free (self->priv->name);
    g_free (self->priv->display_name);

    G_OBJECT_CLASS (xfce_rr_output_info_parent_class)->finalize (gobject);
}

static void
xfce_rr_output_info_class_init (XfceRROutputInfoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (XfceRROutputInfoPrivate));

    gobject_class->finalize = xfce_rr_output_info_finalize;
}

/**
 * xfce_rr_output_info_get_name:
 *
 * Returns: (transfer none): the output name
 */
char *xfce_rr_output_info_get_name (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), NULL);

    return self->priv->name;
}

/**
 * xfce_rr_output_info_is_active:
 *
 * Returns: whether there is a CRTC assigned to this output (i.e. a signal is being sent to it)
 */
gboolean xfce_rr_output_info_is_active (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->on;
}

void xfce_rr_output_info_set_active (XfceRROutputInfo *self, gboolean active)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));

    self->priv->on = active;
}

/**
 * xfce_rr_output_info_get_geometry:
 * @self: a #XfceRROutputInfo
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 * @width: (out) (allow-none):
 * @height: (out) (allow-none):
 */
void xfce_rr_output_info_get_geometry (XfceRROutputInfo *self, int *x, int *y, int *width, int *height)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));

    if (x)
	*x = self->priv->x;
    if (y)
	*y = self->priv->y;
    if (width)
	*width = self->priv->width;
    if (height)
	*height = self->priv->height;
}

/**
 * xfce_rr_output_info_set_geometry:
 * @self: a #XfceRROutputInfo
 * @x: x offset for monitor
 * @y: y offset for monitor
 * @width: monitor width
 * @height: monitor height
 *
 * Set the geometry for the monitor connected to the specified output.
 */
void xfce_rr_output_info_set_geometry (XfceRROutputInfo *self, int  x, int  y, int  width, int  height)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));

    self->priv->x = x;
    self->priv->y = y;
    self->priv->width = width;
    self->priv->height = height;
}

int xfce_rr_output_info_get_refresh_rate (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->rate;
}

void xfce_rr_output_info_set_refresh_rate (XfceRROutputInfo *self, int rate)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));

    self->priv->rate = rate;
}

XfceRRRotation xfce_rr_output_info_get_rotation (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), XFCE_RR_ROTATION_0);

    return self->priv->rotation;
}

void xfce_rr_output_info_set_rotation (XfceRROutputInfo *self, XfceRRRotation rotation)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));

    self->priv->rotation = rotation;
}

/**
 * xfce_rr_output_info_is_connected:
 *
 * Returns: whether the output is physically connected to a monitor
 */
gboolean xfce_rr_output_info_is_connected (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->connected;
}

/**
 * xfce_rr_output_info_get_vendor:
 * @self: a #XfceRROutputInfo
 * @vendor: (out caller-allocates) (array fixed-size=4):
 */
void xfce_rr_output_info_get_vendor (XfceRROutputInfo *self, gchar* vendor)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));
    g_return_if_fail (vendor != NULL);

    vendor[0] = self->priv->vendor[0];
    vendor[1] = self->priv->vendor[1];
    vendor[2] = self->priv->vendor[2];
    vendor[3] = self->priv->vendor[3];
}

guint xfce_rr_output_info_get_product (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->product;
}

guint xfce_rr_output_info_get_serial (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->serial;
}

double xfce_rr_output_info_get_aspect_ratio (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->aspect;
}

/**
 * xfce_rr_output_info_get_display_name:
 *
 * Returns: (transfer none): the display name of this output
 */
char *xfce_rr_output_info_get_display_name (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), NULL);

    return self->priv->display_name;
}

gboolean xfce_rr_output_info_get_primary (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->primary;
}

void xfce_rr_output_info_set_primary (XfceRROutputInfo *self, gboolean primary)
{
    g_return_if_fail (XFCE_IS_RR_OUTPUT_INFO (self));

    self->priv->primary = primary;
}

int xfce_rr_output_info_get_preferred_width (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->pref_width;
}

int xfce_rr_output_info_get_preferred_height (XfceRROutputInfo *self)
{
    g_return_val_if_fail (XFCE_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->pref_height;
}
