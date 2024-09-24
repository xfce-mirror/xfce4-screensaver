/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef SAVERS_GSTE_SLIDESHOW_H_
#define SAVERS_GSTE_SLIDESHOW_H_

#include <glib.h>
#include <gdk/gdk.h>

#include "gs-theme-engine.h"

G_BEGIN_DECLS

#define GSTE_TYPE_SLIDESHOW         (gste_slideshow_get_type ())
#define GSTE_SLIDESHOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSTE_TYPE_SLIDESHOW, GSTESlideshow))
#define GSTE_SLIDESHOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSTE_TYPE_SLIDESHOW, GSTESlideshowClass))
#define GSTE_IS_SLIDESHOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSTE_TYPE_SLIDESHOW))
#define GSTE_IS_SLIDESHOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSTE_TYPE_SLIDESHOW))
#define GSTE_SLIDESHOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSTE_TYPE_SLIDESHOW, GSTESlideshowClass))

typedef struct GSTESlideshowPrivate GSTESlideshowPrivate;

typedef struct
{
    GSThemeEngine         parent;
    GSTESlideshowPrivate *priv;
} GSTESlideshow;

typedef struct
{
    GSThemeEngineClass     parent_class;
} GSTESlideshowClass;

GType           gste_slideshow_get_type         (void);
GSThemeEngine  *gste_slideshow_new              (void);

void            gste_slideshow_set_images_location  (GSTESlideshow *show,
                                                     const char    *location);

void            gste_slideshow_set_sort_images      (GSTESlideshow *show,
                                                     gboolean       sort_images);

void            gste_slideshow_set_background_color (GSTESlideshow *show,
                                                     const char    *background_color);

void            gste_slideshow_set_no_stretch_hint  (GSTESlideshow *show,
                                                     gboolean       no_stretch_hint);

void            gste_slideshow_set_no_crop_hint  (GSTESlideshow *show,
                                                     gboolean       no_crop_hint);

G_END_DECLS

#endif /* SAVERS_GSTE_SLIDESHOW_H_ */
