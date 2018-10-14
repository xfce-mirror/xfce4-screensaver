#ifndef XFCE_RR_PRIVATE_H
#define XFCE_RR_PRIVATE_H

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

#endif
