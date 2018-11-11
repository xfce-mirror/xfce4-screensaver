/* xfce-rr-private.h
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2018 Sean Davis <bluesabre@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#ifndef SRC_XFCE_RR_PRIVATE_H_
#define SRC_XFCE_RR_PRIVATE_H_

#include <X11/Xlib.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
#ifdef HAVE_RANDR
    XRRScreenResources  *resources;
    RROutput             primary;
#endif

    XfceRROutput       **outputs;
    XfceRRCrtc         **crtcs;
    XfceRRMode         **modes;
    XfceRRMode         **clone_modes;
    XfceRRScreen        *screen;
    int                  min_width;
    int                  max_width;
    int                  min_height;
    int                  max_height;
};

struct XfceRRScreenPrivate
{
    GdkScreen           *gdk_screen;
    GdkWindow           *gdk_root;
    Display             *xdisplay;
    Screen              *xscreen;
    ScreenInfo          *info;
    Window               xroot;

    int                  randr_event_base;
    int                  rr_major_version;
    int                  rr_minor_version;

    Atom                 connector_type_atom;
};

#endif /* SRC_XFCE_RR_PRIVATE_H_ */
