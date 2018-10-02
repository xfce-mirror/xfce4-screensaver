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

#endif
