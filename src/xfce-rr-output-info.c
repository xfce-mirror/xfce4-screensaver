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
