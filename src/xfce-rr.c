/* xfce-rr.c
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * 
 * This file is part of the Xfce Library.
 * 
 * The Xfce Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Xfce Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with the Xfce Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 */



#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <X11/Xlib.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "xfce-rr.h"

#include "xfce-rr-private.h"

#define DISPLAY(o) ((o)->info->screen->priv->xdisplay)

#ifndef HAVE_RANDR
/* This is to avoid a ton of ifdefs wherever we use a type from libXrandr */
typedef int RROutput;
typedef int RRCrtc;
typedef int RRMode;
typedef int Rotation;
#define RR_Rotate_0		1
#define RR_Rotate_90		2
#define RR_Rotate_180		4
#define RR_Rotate_270		8
#define RR_Reflect_X		16
#define RR_Reflect_Y		32
#endif

enum {
    SCREEN_PROP_0,
    SCREEN_PROP_GDK_SCREEN,
    SCREEN_PROP_LAST,
};

enum {
    SCREEN_CHANGED,
    SCREEN_SIGNAL_LAST,
};

gint screen_signals[SCREEN_SIGNAL_LAST];

struct XfceRROutput
{
    ScreenInfo *	info;
    RROutput		id;
    
    char *		name;
    XfceRRCrtc *	current_crtc;
    gboolean		connected;
    gulong		width_mm;
    gulong		height_mm;
    XfceRRCrtc **	possible_crtcs;
    XfceRROutput **	clones;
    XfceRRMode **	modes;
    int			n_preferred;
    guint8 *		edid_data;
    int         edid_size;
    char *              connector_type;
};

struct XfceRROutputWrap
{
    RROutput		id;
};

struct XfceRRCrtc
{
    ScreenInfo *	info;
    RRCrtc		id;
    
    XfceRRMode *	current_mode;
    XfceRROutput **	current_outputs;
    XfceRROutput **	possible_outputs;
    int			x;
    int			y;
    
    XfceRRRotation	current_rotation;
    XfceRRRotation	rotations;
    int			gamma_size;
};

struct XfceRRMode
{
    ScreenInfo *	info;
    RRMode		id;
    char *		name;
    int			width;
    int			height;
    int			freq;		/* in mHz */
};

/* XfceRRCrtc */
static XfceRRCrtc *  crtc_new          (ScreenInfo         *info,
					 RRCrtc              id);
static XfceRRCrtc *  crtc_copy         (const XfceRRCrtc  *from);
static void           crtc_free         (XfceRRCrtc        *crtc);

#ifdef HAVE_RANDR
static gboolean       crtc_initialize   (XfceRRCrtc        *crtc,
					 XRRScreenResources *res,
					 GError            **error);
#endif

/* XfceRROutput */
static XfceRROutput *output_new        (ScreenInfo         *info,
					 RROutput            id);

#ifdef HAVE_RANDR
static gboolean       output_initialize (XfceRROutput      *output,
					 XRRScreenResources *res,
					 GError            **error);
#endif

static XfceRROutput *output_copy       (const XfceRROutput *from);
static void           output_free       (XfceRROutput      *output);

/* XfceRRMode */
static XfceRRMode *  mode_new          (ScreenInfo         *info,
					 RRMode              id);

#ifdef HAVE_RANDR
static void           mode_initialize   (XfceRRMode        *mode,
					 XRRModeInfo        *info);
#endif

static XfceRRMode *  mode_copy         (const XfceRRMode  *from);
static void           mode_free         (XfceRRMode        *mode);

static guint         xfce_rr_mode_get_width  (XfceRRMode *mode);
static guint         xfce_rr_mode_get_height (XfceRRMode *mode);

static XfceRRMode  **xfce_rr_output_list_modes (XfceRROutput *output);

static void          xfce_rr_screen_get_ranges (XfceRRScreen *screen,
                                                int *min_width,
                                                int *max_width,
                                                int *min_height,
                                                int *max_height);

static void xfce_rr_screen_finalize (GObject*);
static void xfce_rr_screen_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void xfce_rr_screen_get_property (GObject*, guint, GValue*, GParamSpec*);
static gboolean xfce_rr_screen_initable_init (GInitable*, GCancellable*, GError**);
static void xfce_rr_screen_initable_iface_init (GInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (XfceRRScreen, xfce_rr_screen, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, xfce_rr_screen_initable_iface_init))

G_DEFINE_BOXED_TYPE (XfceRRCrtc, xfce_rr_crtc, crtc_copy, crtc_free)
G_DEFINE_BOXED_TYPE (XfceRROutput, xfce_rr_output, output_copy, output_free)
G_DEFINE_BOXED_TYPE (XfceRRMode, xfce_rr_mode, mode_copy, mode_free)

/* Errors */

/**
 * xfce_rr_error_quark:
 *
 * Returns the #GQuark that will be used for #GError values returned by the
 * XfceRR API.
 *
 * Return value: a #GQuark used to identify errors coming from the XfceRR API.
 */
GQuark
xfce_rr_error_quark (void)
{
    return g_quark_from_static_string ("xfce-rr-error-quark");
}

/* Screen */
static XfceRROutput *
xfce_rr_output_by_id (ScreenInfo *info, RROutput id)
{
    XfceRROutput **output;
    
    g_assert (info != NULL);
    
    for (output = info->outputs; *output; ++output)
    {
	if ((*output)->id == id)
	    return *output;
    }
    
    return NULL;
}

static XfceRRCrtc *
crtc_by_id (ScreenInfo *info, RRCrtc id)
{
    XfceRRCrtc **crtc;
    
    if (!info)
        return NULL;
    
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if ((*crtc)->id == id)
	    return *crtc;
    }
    
    return NULL;
}

static XfceRRMode *
mode_by_id (ScreenInfo *info, RRMode id)
{
    XfceRRMode **mode;
    
    g_assert (info != NULL);
    
    for (mode = info->modes; *mode; ++mode)
    {
	if ((*mode)->id == id)
	    return *mode;
    }
    
    return NULL;
}

static void
screen_info_free (ScreenInfo *info)
{
    XfceRROutput **output;
    XfceRRCrtc **crtc;
    XfceRRMode **mode;
    
    g_assert (info != NULL);

#ifdef HAVE_RANDR
    if (info->resources)
    {
	XRRFreeScreenResources (info->resources);
	
	info->resources = NULL;
    }
#endif
    
    if (info->outputs)
    {
	for (output = info->outputs; *output; ++output)
	    output_free (*output);
	g_free (info->outputs);
    }
    
    if (info->crtcs)
    {
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_free (*crtc);
	g_free (info->crtcs);
    }
    
    if (info->modes)
    {
	for (mode = info->modes; *mode; ++mode)
	    mode_free (*mode);
	g_free (info->modes);
    }

    if (info->clone_modes)
    {
	/* The modes themselves were freed above */
	g_free (info->clone_modes);
    }
    
    g_free (info);
}

static gboolean
has_similar_mode (XfceRROutput *output, XfceRRMode *mode)
{
    int i;
    XfceRRMode **modes = xfce_rr_output_list_modes (output);
    int width = xfce_rr_mode_get_width (mode);
    int height = xfce_rr_mode_get_height (mode);

    for (i = 0; modes[i] != NULL; ++i)
    {
	XfceRRMode *m = modes[i];

	if (xfce_rr_mode_get_width (m) == width	&&
	    xfce_rr_mode_get_height (m) == height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

static void
gather_clone_modes (ScreenInfo *info)
{
    int i;
    GPtrArray *result = g_ptr_array_new ();

    for (i = 0; info->outputs[i] != NULL; ++i)
    {
	int j;
	XfceRROutput *output1, *output2;

	output1 = info->outputs[i];
	
	if (!output1->connected)
	    continue;
	
	for (j = 0; output1->modes[j] != NULL; ++j)
	{
	    XfceRRMode *mode = output1->modes[j];
	    gboolean valid;
	    int k;

	    valid = TRUE;
	    for (k = 0; info->outputs[k] != NULL; ++k)
	    {
		output2 = info->outputs[k];
		
		if (!output2->connected)
		    continue;
		
		if (!has_similar_mode (output2, mode))
		{
		    valid = FALSE;
		    break;
		}
	    }

	    if (valid)
		g_ptr_array_add (result, mode);
	}
    }

    g_ptr_array_add (result, NULL);
    
    info->clone_modes = (XfceRRMode **)g_ptr_array_free (result, FALSE);
}

#ifdef HAVE_RANDR
static gboolean
fill_screen_info_from_resources (ScreenInfo *info,
				 XRRScreenResources *resources,
				 GError **error)
{
    int i;
    GPtrArray *a;
    XfceRRCrtc **crtc;
    XfceRROutput **output;

    info->resources = resources;

    /* We create all the structures before initializing them, so
     * that they can refer to each other.
     */
    a = g_ptr_array_new ();
    for (i = 0; i < resources->ncrtc; ++i)
    {
	XfceRRCrtc *crtc = crtc_new (info, resources->crtcs[i]);

	g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    info->crtcs = (XfceRRCrtc **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0; i < resources->noutput; ++i)
    {
	XfceRROutput *output = output_new (info, resources->outputs[i]);

	g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    info->outputs = (XfceRROutput **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0;  i < resources->nmode; ++i)
    {
	XfceRRMode *mode = mode_new (info, resources->modes[i].id);

	g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    info->modes = (XfceRRMode **)g_ptr_array_free (a, FALSE);

    /* Initialize */
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if (!crtc_initialize (*crtc, resources, error))
	    return FALSE;
    }

    for (output = info->outputs; *output; ++output)
    {
	if (!output_initialize (*output, resources, error))
	    return FALSE;
    }

    for (i = 0; i < resources->nmode; ++i)
    {
	XfceRRMode *mode = mode_by_id (info, resources->modes[i].id);

	mode_initialize (mode, &(resources->modes[i]));
    }

    gather_clone_modes (info);

    return TRUE;
}
#endif /* HAVE_RANDR */

static gboolean
fill_out_screen_info (Display *xdisplay,
		      Window xroot,
		      ScreenInfo *info,
		      gboolean needs_reprobe,
		      GError **error)
{
#ifdef HAVE_RANDR
    XRRScreenResources *resources;
	GdkDisplay *display;
    
    g_assert (xdisplay != NULL);
    g_assert (info != NULL);

    /* First update the screen resources */

    if (needs_reprobe)
        resources = XRRGetScreenResources (xdisplay, xroot);
    else
        resources = XRRGetScreenResourcesCurrent (xdisplay, xroot);

    if (resources)
    {
	if (!fill_screen_info_from_resources (info, resources, error))
	    return FALSE;
    }
    else
    {
	g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_RANDR_ERROR,
		     /* Translators: a CRTC is a CRT Controller (this is X terminology). */
		     _("could not get the screen resources (CRTCs, outputs, modes)"));
	return FALSE;
    }

    /* Then update the screen size range.  We do this after XRRGetScreenResources() so that
     * the X server will already have an updated view of the outputs.
     */

    if (needs_reprobe) {
	gboolean success;

	display = gdk_display_get_default ();
    gdk_x11_display_error_trap_push (display);
	success = XRRGetScreenSizeRange (xdisplay, xroot,
					 &(info->min_width),
					 &(info->min_height),
					 &(info->max_width),
					 &(info->max_height));
	gdk_display_flush (display);
	if (gdk_x11_display_error_trap_pop (display)) {
	    g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_UNKNOWN,
			 _("unhandled X error while getting the range of screen sizes"));
	    return FALSE;
	}

	if (!success) {
	    g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_RANDR_ERROR,
			 _("could not get the range of screen sizes"));
            return FALSE;
        }
    }
    else
    {
        xfce_rr_screen_get_ranges (info->screen, 
					 &(info->min_width),
					 &(info->max_width),
					 &(info->min_height),
					 &(info->max_height));
    }

    info->primary = None;
	display = gdk_display_get_default ();
    gdk_x11_display_error_trap_push (display);
    info->primary = XRRGetOutputPrimary (xdisplay, xroot);
    gdk_x11_display_error_trap_pop_ignored (display);

    return TRUE;
#else
    return FALSE;
#endif /* HAVE_RANDR */
}

static ScreenInfo *
screen_info_new (XfceRRScreen *screen, gboolean needs_reprobe, GError **error)
{
    ScreenInfo *info = g_new0 (ScreenInfo, 1);
    XfceRRScreenPrivate *priv;

    g_assert (screen != NULL);

    priv = screen->priv;

    info->outputs = NULL;
    info->crtcs = NULL;
    info->modes = NULL;
    info->screen = screen;
    
    if (fill_out_screen_info (priv->xdisplay, priv->xroot, info, needs_reprobe, error))
    {
	return info;
    }
    else
    {
	screen_info_free (info);
	return NULL;
    }
}

static gboolean
screen_update (XfceRRScreen *screen, gboolean force_callback, gboolean needs_reprobe, GError **error)
{
    ScreenInfo *info;
    gboolean changed = FALSE;
    
    g_assert (screen != NULL);

    info = screen_info_new (screen, needs_reprobe, error);
    if (!info)
	    return FALSE;

#ifdef HAVE_RANDR
    if (info->resources->configTimestamp != screen->priv->info->resources->configTimestamp)
	    changed = TRUE;
#endif

    screen_info_free (screen->priv->info);
	
    screen->priv->info = info;

    if (changed || force_callback)
	g_signal_emit (G_OBJECT (screen), screen_signals[SCREEN_CHANGED], 0);
    
    return changed;
}

static GdkFilterReturn
screen_on_event (GdkXEvent *xevent,
		 GdkEvent *event,
		 gpointer data)
{
#ifdef HAVE_RANDR
    XfceRRScreen *screen = data;
    XfceRRScreenPrivate *priv = screen->priv;
    XEvent *e = xevent;
    int event_num;

    if (!e)
	return GDK_FILTER_CONTINUE;

    event_num = e->type - priv->randr_event_base;

    if (event_num == RRScreenChangeNotify) {
	/* We don't reprobe the hardware; we just fetch the X server's latest
	 * state.  The server already knows the new state of the outputs; that's
	 * why it sent us an event!
	 */
        screen_update (screen, TRUE, FALSE, NULL); /* NULL-GError */
#if 0
	/* Enable this code to get a dialog showing the RANDR timestamps, for debugging purposes */
	{
	    GtkWidget *dialog;
	    XRRScreenChangeNotifyEvent *rr_event;
	    static int dialog_num;

	    rr_event = (XRRScreenChangeNotifyEvent *) e;

	    dialog = gtk_message_dialog_new (NULL,
					     0,
					     GTK_MESSAGE_INFO,
					     GTK_BUTTONS_CLOSE,
					     "RRScreenChangeNotify timestamps (%d):\n"
					     "event change: %u\n"
					     "event config: %u\n"
					     "event serial: %lu\n"
					     "----------------------"
					     "screen change: %u\n"
					     "screen config: %u\n",
					     dialog_num++,
					     (guint32) rr_event->timestamp,
					     (guint32) rr_event->config_timestamp,
					     rr_event->serial,
					     (guint32) priv->info->resources->timestamp,
					     (guint32) priv->info->resources->configTimestamp);
	    g_signal_connect (dialog, "response",
			      G_CALLBACK (gtk_widget_destroy), NULL);
	    gtk_widget_show (dialog);
	}
#endif
    }
#if 0
    /* WHY THIS CODE IS DISABLED:
     *
     * Note that in xfce_rr_screen_new(), we only select for
     * RRScreenChangeNotifyMask.  We used to select for other values in
     * RR*NotifyMask, but we weren't really doing anything useful with those
     * events.  We only care about "the screens changed in some way or another"
     * for now.
     *
     * If we ever run into a situtation that could benefit from processing more
     * detailed events, we can enable this code again.
     *
     * Note that the X server sends RRScreenChangeNotify in conjunction with the
     * more detailed events from RANDR 1.2 - see xserver/randr/randr.c:TellChanged().
     */
    else if (event_num == RRNotify)
    {
	/* Other RandR events */

	XRRNotifyEvent *event = (XRRNotifyEvent *)e;

	/* Here we can distinguish between RRNotify events supported
	 * since RandR 1.2 such as RRNotify_OutputProperty.  For now, we
	 * don't have anything special to do for particular subevent types, so
	 * we leave this as an empty switch().
	 */
	switch (event->subtype)
	{
	default:
	    break;
	}

	/* No need to reprobe hardware here */
	screen_update (screen, TRUE, FALSE, NULL); /* NULL-GError */
    }
#endif

#endif /* HAVE_RANDR */

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}

static gboolean
xfce_rr_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)

{
    XfceRRScreen *self = XFCE_RR_SCREEN (initable);
    XfceRRScreenPrivate *priv = self->priv;
    Display *dpy = GDK_SCREEN_XDISPLAY (self->priv->gdk_screen);
    int event_base;
    int ignore;

    priv->connector_type_atom = XInternAtom (dpy, "ConnectorType", FALSE);

#ifdef HAVE_RANDR
    if (XRRQueryExtension (dpy, &event_base, &ignore))
    {
        priv->randr_event_base = event_base;

        XRRQueryVersion (dpy, &priv->rr_major_version, &priv->rr_minor_version);
        if (priv->rr_major_version < 1 || (priv->rr_major_version == 1 && priv->rr_minor_version < 3)) {
            g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_NO_RANDR_EXTENSION,
                "RANDR extension is too old (must be at least 1.3)");
            return FALSE;
        }

        priv->info = screen_info_new (self, TRUE, error);

        if (!priv->info) {
	    return FALSE;
	}

        XRRSelectInput (priv->xdisplay,
            priv->xroot,
            RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (gdk_screen_get_display (priv->gdk_screen),
                          event_base,
                          RRNotify + 1);
        gdk_window_add_filter (priv->gdk_root, screen_on_event, self);

        return TRUE;
    }
    else
    {
#endif /* HAVE_RANDR */
    g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_NO_RANDR_EXTENSION,
        _("RANDR extension is not present"));

    return FALSE;

#ifdef HAVE_RANDR
   }
#endif
}

void
xfce_rr_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = xfce_rr_screen_initable_init;
}

void
    xfce_rr_screen_finalize (GObject *gobject)
{
    XfceRRScreen *screen = XFCE_RR_SCREEN (gobject);

    gdk_window_remove_filter (screen->priv->gdk_root, screen_on_event, screen);

    if (screen->priv->info)
      screen_info_free (screen->priv->info);

    G_OBJECT_CLASS (xfce_rr_screen_parent_class)->finalize (gobject);
}

void
xfce_rr_screen_set_property (GObject *gobject, guint property_id, const GValue *value, GParamSpec *property)
{
    XfceRRScreen *self = XFCE_RR_SCREEN (gobject);
    XfceRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        priv->gdk_screen = g_value_get_object (value);
        priv->gdk_root = gdk_screen_get_root_window (priv->gdk_screen);
        priv->xroot = GDK_WINDOW_XID (priv->gdk_root);
        priv->xdisplay = GDK_SCREEN_XDISPLAY (priv->gdk_screen);
        priv->xscreen = gdk_x11_screen_get_xscreen (priv->gdk_screen);
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

void
xfce_rr_screen_get_property (GObject *gobject, guint property_id, GValue *value, GParamSpec *property)
{
    XfceRRScreen *self = XFCE_RR_SCREEN (gobject);
    XfceRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        g_value_set_object (value, priv->gdk_screen);
        return;
     default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

void
xfce_rr_screen_class_init (XfceRRScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    g_type_class_add_private (klass, sizeof (XfceRRScreenPrivate));

    gobject_class->set_property = xfce_rr_screen_set_property;
    gobject_class->get_property = xfce_rr_screen_get_property;
    gobject_class->finalize = xfce_rr_screen_finalize;

    g_object_class_install_property(
        gobject_class,
        SCREEN_PROP_GDK_SCREEN,
        g_param_spec_object (
            "gdk-screen",
            "GDK Screen",
            "The GDK Screen represented by this XfceRRScreen",
            GDK_TYPE_SCREEN,
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)
        );

    screen_signals[SCREEN_CHANGED] = g_signal_new("changed",
        G_TYPE_FROM_CLASS (gobject_class),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
        G_STRUCT_OFFSET (XfceRRScreenClass, changed),
        NULL,
        NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE,
        0);
}

void
xfce_rr_screen_init (XfceRRScreen *self)
{
    XfceRRScreenPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, XFCE_TYPE_RR_SCREEN, XfceRRScreenPrivate);
    self->priv = priv;

    priv->gdk_screen = NULL;
    priv->gdk_root = NULL;
    priv->xdisplay = NULL;
    priv->xroot = None;
    priv->xscreen = NULL;
    priv->info = NULL;
    priv->rr_major_version = 0;
    priv->rr_minor_version = 0;
}

/**
 * xfce_rr_screen_new:
 * @screen: the #GdkScreen on which to operate
 * @error: will be set if XRandR is not supported
 *
 * Creates a new #XfceRRScreen instance
 *
 * Returns: a new #XfceRRScreen instance or NULL if screen could not be created,
 * for instance if the driver does not support Xrandr 1.2
 */
XfceRRScreen *
xfce_rr_screen_new (GdkScreen *screen,
                    GError **error)
{
    //bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    return g_initable_new (XFCE_TYPE_RR_SCREEN, NULL, error, "gdk-screen", screen, NULL);
}

/**
 * xfce_rr_screen_get_ranges:
 * @screen: a #XfceRRScreen
 * @min_width: (out): the minimum width
 * @max_width: (out): the maximum width
 * @min_height: (out): the minimum height
 * @max_height: (out): the maximum height
 *
 * Get the ranges of the screen
 */
static void
xfce_rr_screen_get_ranges (XfceRRScreen *screen,
			    int	          *min_width,
			    int	          *max_width,
			    int           *min_height,
			    int	          *max_height)
{
    XfceRRScreenPrivate *priv;

    g_return_if_fail (XFCE_IS_RR_SCREEN (screen));

    priv = screen->priv;
    
    if (min_width)
	*min_width = priv->info->min_width;
    
    if (max_width)
	*max_width = priv->info->max_width;
    
    if (min_height)
	*min_height = priv->info->min_height;
    
    if (max_height)
	*max_height = priv->info->max_height;
}

static gboolean
force_timestamp_update (XfceRRScreen *screen)
{
#ifdef HAVE_RANDR
    XfceRRScreenPrivate *priv = screen->priv;
    XfceRRCrtc *crtc;
    XRRCrtcInfo *current_info;
	GdkDisplay *display;
    Status status;
    gboolean timestamp_updated;

    timestamp_updated = FALSE;

    crtc = priv->info->crtcs[0];

    if (crtc == NULL)
	goto out;

    current_info = XRRGetCrtcInfo (priv->xdisplay,
				   priv->info->resources,
				   crtc->id);

    if (current_info == NULL)
	  goto out;

	display = gdk_display_get_default ();
    gdk_x11_display_error_trap_push (display);
    status = XRRSetCrtcConfig (priv->xdisplay,
			       priv->info->resources,
			       crtc->id,
			       current_info->timestamp,
			       current_info->x,
			       current_info->y,
			       current_info->mode,
			       current_info->rotation,
			       current_info->outputs,
			       current_info->noutput);

    XRRFreeCrtcInfo (current_info);

    gdk_display_flush (display);
    if (gdk_x11_display_error_trap_pop (display))
	goto out;

    if (status == RRSetConfigSuccess)
	timestamp_updated = TRUE;
out:
    return timestamp_updated;
#else
    return FALSE;
#endif
}

/**
 * xfce_rr_screen_refresh:
 * @screen: a #XfceRRScreen
 * @error: location to store error, or %NULL
 *
 * Refreshes the screen configuration, and calls the screen's callback if it
 * exists and if the screen's configuration changed.
 *
 * Return value: TRUE if the screen's configuration changed; otherwise, the
 * function returns FALSE and a NULL error if the configuration didn't change,
 * or FALSE and a non-NULL error if there was an error while refreshing the
 * configuration.
 */
gboolean
xfce_rr_screen_refresh (XfceRRScreen *screen,
			 GError       **error)
{
    gboolean refreshed;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gdk_x11_display_grab (gdk_screen_get_display (screen->priv->gdk_screen));

    refreshed = screen_update (screen, FALSE, TRUE, error);
    force_timestamp_update (screen); /* this is to keep other clients from thinking that the X server re-detected things by itself - bgo#621046 */

    gdk_x11_display_ungrab (gdk_screen_get_display (screen->priv->gdk_screen));

    return refreshed;
}

/**
 * xfce_rr_screen_list_crtcs:
 *
 * List all CRTCs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
XfceRRCrtc **
xfce_rr_screen_list_crtcs (XfceRRScreen *screen)
{
    g_return_val_if_fail (XFCE_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->crtcs;
}

/* XfceRROutput */
static XfceRROutput *
output_new (ScreenInfo *info, RROutput id)
{
    XfceRROutput *output = g_slice_new0 (XfceRROutput);
    
    output->id = id;
    output->info = info;
    
    return output;
}

static guint8 *
get_property (Display *dpy,
	      RROutput output,
	      Atom atom,
	      int *len)
{
#ifdef HAVE_RANDR
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    guint8 *result;
    
    XRRGetOutputProperty (dpy, output, atom,
			  0, 100, False, False,
			  AnyPropertyType,
			  &actual_type, &actual_format,
			  &nitems, &bytes_after, &prop);
    
    if (actual_type == XA_INTEGER && actual_format == 8)
    {
	result = g_memdup (prop, nitems);
	if (len)
	    *len = nitems;
    }
    else
    {
	result = NULL;
    }
    
    XFree (prop);
    
    return result;
#else
    return NULL;
#endif /* HAVE_RANDR */
}

static guint8 *
read_edid_data (XfceRROutput *output, int *len)
{
    Atom edid_atom;
    guint8 *result;

    edid_atom = XInternAtom (DISPLAY (output), "EDID", FALSE);
    result = get_property (DISPLAY (output),
			   output->id, edid_atom, len);

    if (!result)
    {
	edid_atom = XInternAtom (DISPLAY (output), "EDID_DATA", FALSE);
	result = get_property (DISPLAY (output),
			       output->id, edid_atom, len);
    }

    if (result)
    {
	if (*len % 128 == 0)
	    return result;
	else
	    g_free (result);
    }
    
    return NULL;
}

static char *
get_connector_type_string (XfceRROutput *output)
{
#ifdef HAVE_RANDR
    char *result;
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    Atom connector_type;
    char *connector_type_str;

    result = NULL;

    if (XRRGetOutputProperty (DISPLAY (output), output->id, output->info->screen->priv->connector_type_atom,
			      0, 100, False, False,
			      AnyPropertyType,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, &prop) != Success)
	return NULL;

    if (!(actual_type == XA_ATOM && actual_format == 32 && nitems == 1))
	goto out;

    connector_type = *((Atom *) prop);

    connector_type_str = XGetAtomName (DISPLAY (output), connector_type);
    if (connector_type_str) {
	result = g_strdup (connector_type_str); /* so the caller can g_free() it */
	XFree (connector_type_str);
    }

out:

    XFree (prop);

    return result;
#else
    return NULL;
#endif
}

#ifdef HAVE_RANDR
static gboolean
output_initialize (XfceRROutput *output, XRRScreenResources *res, GError **error)
{
    XRROutputInfo *info = XRRGetOutputInfo (
	DISPLAY (output), res, output->id);
    GPtrArray *a;
    int i;
    
#if 0
    g_print ("Output %lx Timestamp: %u\n", output->id, (guint32)info->timestamp);
#endif
    
    if (!info || !output->info)
    {
	/* FIXME: see the comment in crtc_initialize() */
	/* Translators: here, an "output" is a video output */
	g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_RANDR_ERROR,
		     _("could not get information about output %d"),
		     (int) output->id);
	return FALSE;
    }
    
    output->name = g_strdup (info->name); /* FIXME: what is nameLen used for? */
    output->current_crtc = crtc_by_id (output->info, info->crtc);
    output->width_mm = info->mm_width;
    output->height_mm = info->mm_height;
    output->connected = (info->connection == RR_Connected);
    output->connector_type = get_connector_type_string (output);

    /* Possible crtcs */
    a = g_ptr_array_new ();
    
    for (i = 0; i < info->ncrtc; ++i)
    {
	XfceRRCrtc *crtc = crtc_by_id (output->info, info->crtcs[i]);
	
	if (crtc)
	    g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    output->possible_crtcs = (XfceRRCrtc **)g_ptr_array_free (a, FALSE);
    
    /* Clones */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nclone; ++i)
    {
	XfceRROutput *xfce_rr_output = xfce_rr_output_by_id (output->info, info->clones[i]);
	
	if (xfce_rr_output)
	    g_ptr_array_add (a, xfce_rr_output);
    }
    g_ptr_array_add (a, NULL);
    output->clones = (XfceRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Modes */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nmode; ++i)
    {
	XfceRRMode *mode = mode_by_id (output->info, info->modes[i]);
	
	if (mode)
	    g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    output->modes = (XfceRRMode **)g_ptr_array_free (a, FALSE);
    
    output->n_preferred = info->npreferred;
    
    /* Edid data */
    output->edid_data = read_edid_data (output, &output->edid_size);
    
    XRRFreeOutputInfo (info);

    return TRUE;
}
#endif /* HAVE_RANDR */

static XfceRROutput*
output_copy (const XfceRROutput *from)
{
    GPtrArray *array;
    XfceRRCrtc **p_crtc;
    XfceRROutput **p_output;
    XfceRRMode **p_mode;
    XfceRROutput *output = g_slice_new0 (XfceRROutput);

    output->id = from->id;
    output->info = from->info;
    output->name = g_strdup (from->name);
    output->current_crtc = from->current_crtc;
    output->width_mm = from->width_mm;
    output->height_mm = from->height_mm;
    output->connected = from->connected;
    output->n_preferred = from->n_preferred;
    output->connector_type = g_strdup (from->connector_type);

    array = g_ptr_array_new ();
    for (p_crtc = from->possible_crtcs; *p_crtc != NULL; p_crtc++)
    {
        g_ptr_array_add (array, *p_crtc);
    }
    output->possible_crtcs = (XfceRRCrtc**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->clones; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    output->clones = (XfceRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_mode = from->modes; *p_mode != NULL; p_mode++)
    {
        g_ptr_array_add (array, *p_mode);
    }
    output->modes = (XfceRRMode**) g_ptr_array_free (array, FALSE);

    output->edid_size = from->edid_size;
    output->edid_data = g_memdup (from->edid_data, from->edid_size);

    return output;
}

static void
output_free (XfceRROutput *output)
{
    g_free (output->clones);
    g_free (output->modes);
    g_free (output->possible_crtcs);
    g_free (output->edid_data);
    g_free (output->name);
    g_free (output->connector_type);
    g_slice_free (XfceRROutput, output);
}

/**
 * xfce_rr_output_list_modes:
 * @output: a #XfceRROutput
 * Returns: (array zero-terminated=1) (transfer none):
 */

static XfceRRMode **
xfce_rr_output_list_modes (XfceRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes;
}

/* XfceRRCrtc */
typedef struct
{
    Rotation xrot;
    XfceRRRotation rot;
} RotationMap;

static const RotationMap rotation_map[] =
{
    { RR_Rotate_0, XFCE_RR_ROTATION_0 },
    { RR_Rotate_90, XFCE_RR_ROTATION_90 },
    { RR_Rotate_180, XFCE_RR_ROTATION_180 },
    { RR_Rotate_270, XFCE_RR_ROTATION_270 },
    { RR_Reflect_X, XFCE_RR_REFLECT_X },
    { RR_Reflect_Y, XFCE_RR_REFLECT_Y },
};

static XfceRRRotation
xfce_rr_rotation_from_xrotation (Rotation r)
{
    int i;
    XfceRRRotation result = 0;
    
    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].xrot)
	    result |= rotation_map[i].rot;
    }
    
    return result;
}

/**
 * xfce_rr_crtc_get_current_mode:
 * @crtc: a #XfceRRCrtc
 * Returns: (transfer none): the current mode of this crtc
 */
XfceRRMode *
xfce_rr_crtc_get_current_mode (XfceRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, NULL);
    
    return crtc->current_mode;
}

static XfceRRCrtc *
crtc_new (ScreenInfo *info, RROutput id)
{
    XfceRRCrtc *crtc = g_slice_new0 (XfceRRCrtc);
    
    crtc->id = id;
    crtc->info = info;
    
    return crtc;
}

static XfceRRCrtc *
crtc_copy (const XfceRRCrtc *from)
{
    XfceRROutput **p_output;
    GPtrArray *array;
    XfceRRCrtc *to = g_slice_new0 (XfceRRCrtc);

    to->info = from->info;
    to->id = from->id;
    to->current_mode = from->current_mode;
    to->x = from->x;
    to->y = from->y;
    to->current_rotation = from->current_rotation;
    to->rotations = from->rotations;
    to->gamma_size = from->gamma_size;

    array = g_ptr_array_new ();
    for (p_output = from->current_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->current_outputs = (XfceRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->possible_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->possible_outputs = (XfceRROutput**) g_ptr_array_free (array, FALSE);

    return to;
}

#ifdef HAVE_RANDR
static gboolean
crtc_initialize (XfceRRCrtc        *crtc,
		 XRRScreenResources *res,
		 GError            **error)
{
    XRRCrtcInfo *info = XRRGetCrtcInfo (DISPLAY (crtc), res, crtc->id);
    GPtrArray *a;
    int i;
    
#if 0
    g_print ("CRTC %lx Timestamp: %u\n", crtc->id, (guint32)info->timestamp);
#endif
    
    if (!info)
    {
	/* FIXME: We need to reaquire the screen resources */
	/* FIXME: can we actually catch BadRRCrtc, and does it make sense to emit that? */

	/* Translators: CRTC is a CRT Controller (this is X terminology).
	 * It is *very* unlikely that you'll ever get this error, so it is
	 * only listed for completeness. */
	g_set_error (error, XFCE_RR_ERROR, XFCE_RR_ERROR_RANDR_ERROR,
		     _("could not get information about CRTC %d"),
		     (int) crtc->id);
	return FALSE;
    }
    
    /* XfceRRMode */
    crtc->current_mode = mode_by_id (crtc->info, info->mode);
    
    crtc->x = info->x;
    crtc->y = info->y;
    
    /* Current outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->noutput; ++i)
    {
	XfceRROutput *output = xfce_rr_output_by_id (crtc->info, info->outputs[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->current_outputs = (XfceRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Possible outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->npossible; ++i)
    {
	XfceRROutput *output = xfce_rr_output_by_id (crtc->info, info->possible[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->possible_outputs = (XfceRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Rotations */
    crtc->current_rotation = xfce_rr_rotation_from_xrotation (info->rotation);
    crtc->rotations = xfce_rr_rotation_from_xrotation (info->rotations);
    
    XRRFreeCrtcInfo (info);

    /* get an store gamma size */
    crtc->gamma_size = XRRGetCrtcGammaSize (DISPLAY (crtc), crtc->id);

    return TRUE;
}
#endif

static void
crtc_free (XfceRRCrtc *crtc)
{
    g_free (crtc->current_outputs);
    g_free (crtc->possible_outputs);
    g_slice_free (XfceRRCrtc, crtc);
}

/* XfceRRMode */
static XfceRRMode *
mode_new (ScreenInfo *info, RRMode id)
{
    XfceRRMode *mode = g_slice_new0 (XfceRRMode);
    
    mode->id = id;
    mode->info = info;
    
    return mode;
}

static guint
xfce_rr_mode_get_width (XfceRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->width;
}

static guint
xfce_rr_mode_get_height (XfceRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->height;
}

#ifdef HAVE_RANDR
static void
mode_initialize (XfceRRMode *mode, XRRModeInfo *info)
{
    g_assert (mode != NULL);
    g_assert (info != NULL);
    
    mode->name = g_strdup (info->name);
    mode->width = info->width;
    mode->height = info->height;
    mode->freq = ((info->dotClock / (double)info->hTotal) / info->vTotal + 0.5) * 1000;
}
#endif /* HAVE_RANDR */

static XfceRRMode *
mode_copy (const XfceRRMode *from)
{
    XfceRRMode *to = g_slice_new0 (XfceRRMode);

    to->id = from->id;
    to->info = from->info;
    to->name = g_strdup (from->name);
    to->width = from->width;
    to->height = from->height;
    to->freq = from->freq;

    return to;
}

static void
mode_free (XfceRRMode *mode)
{
    g_free (mode->name);
    g_slice_free (XfceRRMode, mode);
}

void
xfce_rr_crtc_set_gamma (XfceRRCrtc *crtc, int size,
			 unsigned short *red,
			 unsigned short *green,
			 unsigned short *blue)
{
#ifdef HAVE_RANDR
    int copy_size;
    XRRCrtcGamma *gamma;

    g_return_if_fail (crtc != NULL);
    g_return_if_fail (red != NULL);
    g_return_if_fail (green != NULL);
    g_return_if_fail (blue != NULL);

    if (size != crtc->gamma_size)
	return;

    gamma = XRRAllocGamma (crtc->gamma_size);

    copy_size = crtc->gamma_size * sizeof (unsigned short);
    memcpy (gamma->red, red, copy_size);
    memcpy (gamma->green, green, copy_size);
    memcpy (gamma->blue, blue, copy_size);

    XRRSetCrtcGamma (DISPLAY (crtc), crtc->id, gamma);
    XRRFreeGamma (gamma);
#endif /* HAVE_RANDR */
}

/**
 * xfce_rr_crtc_get_gamma:
 * @crtc: a #XfceRRCrtc
 * @size:
 * @red: (out): the minimum width
 * @green: (out): the maximum width
 * @blue: (out): the minimum height
 *
 * Returns: %TRUE for success
 */
gboolean
xfce_rr_crtc_get_gamma (XfceRRCrtc *crtc, int *size,
			 unsigned short **red, unsigned short **green,
			 unsigned short **blue)
{
#ifdef HAVE_RANDR
    int copy_size;
    unsigned short *r, *g, *b;
    XRRCrtcGamma *gamma;

    g_return_val_if_fail (crtc != NULL, FALSE);

    gamma = XRRGetCrtcGamma (DISPLAY (crtc), crtc->id);
    if (!gamma)
	return FALSE;

    copy_size = crtc->gamma_size * sizeof (unsigned short);

    if (red) {
	r = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (r, gamma->red, copy_size);
	*red = r;
    }

    if (green) {
	g = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (g, gamma->green, copy_size);
	*green = g;
    }

    if (blue) {
	b = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (b, gamma->blue, copy_size);
	*blue = b;
    }

    XRRFreeGamma (gamma);

    if (size)
	*size = crtc->gamma_size;

    return TRUE;
#else
    return FALSE;
#endif /* HAVE_RANDR */
}

