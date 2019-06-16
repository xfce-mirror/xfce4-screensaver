/*
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
 * Copyright (C) 2018 Sean Davis <bluesabre@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING.LGPL.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <memory.h>

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include "gs-debug.h"
#include "xfcekbd-indicator.h"

typedef struct _gki_globals {
    XklEngine               *engine;
    const gchar             *tooltips_format;
    GSList                  *widget_instances;

    gboolean                 redraw_queued;
} gki_globals;

struct _XfcekbdIndicatorPrivate {
    gboolean set_parent_tooltips;
    gdouble  angle;
};

/* one instance for ALL widgets */
static gki_globals globals;

#define ForAllIndicators() \
    { \
        GSList* cur; \
        for (cur = globals.widget_instances; cur != NULL; cur = cur->next) { \
            XfcekbdIndicator * gki = (XfcekbdIndicator*)cur->data;
#define NextIndicator() \
        } \
    }

G_DEFINE_TYPE (XfcekbdIndicator, xfcekbd_indicator, GTK_TYPE_NOTEBOOK)

static void        xfcekbd_indicator_global_init                    (void);
static void        xfcekbd_indicator_global_term                    (void);
static GtkWidget * xfcekbd_indicator_prepare_drawing                (XfcekbdIndicator *gki,
                                                                     int               group);
static void        xfcekbd_indicator_set_current_page_for_group     (XfcekbdIndicator *gki,
                                                                     int               group);
static void        xfcekbd_indicator_set_current_page               (XfcekbdIndicator *gki);
static void        xfcekbd_indicator_cleanup                        (XfcekbdIndicator *gki);
static void        xfcekbd_indicator_fill                           (XfcekbdIndicator *gki);
static void        xfcekbd_indicator_set_tooltips                   (XfcekbdIndicator *gki,
                                                                     const char       *str);

void
xfcekbd_indicator_set_tooltips (XfcekbdIndicator *gki,
                                const char       *str) {
    g_assert (str == NULL || g_utf8_validate (str, -1, NULL));

    gtk_widget_set_tooltip_text (GTK_WIDGET (gki), str);

    if (gki->priv->set_parent_tooltips) {
        GtkWidget *parent =
            gtk_widget_get_parent (GTK_WIDGET (gki));
        if (parent) {
            gtk_widget_set_tooltip_text (parent, str);
        }
    }
}

void
xfcekbd_indicator_cleanup (XfcekbdIndicator * gki) {
    int          i;
    GtkNotebook *notebook = GTK_NOTEBOOK (gki);

    /* Do not remove the first page! It is the default page */
    for (i = gtk_notebook_get_n_pages (notebook); --i > 0;) {
        gtk_notebook_remove_page (notebook, i);
    }
}

void
xfcekbd_indicator_fill (XfcekbdIndicator * gki) {
    int          grp;
    int          total_groups = xkl_engine_get_num_groups (globals.engine);
    GtkNotebook *notebook = GTK_NOTEBOOK (gki);

    for (grp = 0; grp < total_groups; grp++) {
        GtkWidget *page = xfcekbd_indicator_prepare_drawing (gki, grp);

        if (page == NULL)
            page = gtk_label_new ("");

        gtk_notebook_append_page (notebook, page, NULL);
        gtk_widget_show_all (page);
    }
}

static gboolean xfcekbd_indicator_key_pressed (GtkWidget        *widget,
                                               GdkEventKey      *event,
                                               XfcekbdIndicator *gki) {
    int group;

    switch (event->keyval) {
        case GDK_KEY_KP_Enter:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_3270_Enter:
        case GDK_KEY_Return:
        case GDK_KEY_space:
        case GDK_KEY_KP_Space:
            gs_debug("Switching language");
            group = xkl_engine_get_next_group (globals.engine);
            xkl_engine_lock_group (globals.engine, group);
            globals.redraw_queued = TRUE;
            return TRUE;
        default:
            break;
    }

    return FALSE;
}

static gboolean
xfcekbd_indicator_button_pressed (GtkWidget        *widget,
                                  GdkEventButton   *event,
                                  XfcekbdIndicator *gki) {
    if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
        gs_debug("Switching language");
        int group = xkl_engine_get_next_group (globals.engine);
        xkl_engine_lock_group (globals.engine, group);
        globals.redraw_queued = TRUE;
        return TRUE;
    }
    return FALSE;
}

static gchar *
xfcekbd_indicator_extract_layout_name (int groupId) {
    const gchar** layouts;

    layouts = xkl_engine_get_groups_names (globals.engine);

    if (strlen (layouts[groupId]) < 2)
        return g_strdup ("");
    return g_strndup (layouts[groupId], 2);
}


static gchar *
xfcekbd_indicator_create_label_title (int          group,
                                      GHashTable **ln2cnt_map,
                                      gchar       *layout_name) {
    gpointer  pcounter = NULL;
    char     *prev_layout_name = NULL;
    char     *lbl_title = NULL;
    int       counter = 0;

    if (group == 0) {
        *ln2cnt_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    }

    /* Process layouts with repeating description */
    if (g_hash_table_lookup_extended (*ln2cnt_map,
                                      layout_name,
                                      (gpointer *) & prev_layout_name,
                                      &pcounter)) {
        /* "next" same description */
        gchar appendix[10] = "";
        gint utf8length;
        gunichar cidx;
        counter = GPOINTER_TO_INT (pcounter);
        /* Unicode subscript 2, 3, 4 */
        cidx = 0x2081 + counter;
        utf8length = g_unichar_to_utf8 (cidx, appendix);
        appendix[utf8length] = '\0';
        lbl_title = g_strconcat (layout_name, appendix, NULL);
    } else {
        /* "first" time this description */
        lbl_title = g_strdup (layout_name);
    }
    g_hash_table_insert (*ln2cnt_map, layout_name,
                 GINT_TO_POINTER (counter + 1));
    return lbl_title;
}

static GtkWidget *
xfcekbd_indicator_prepare_drawing (XfcekbdIndicator *gki,
                                   int               groupId) {
    GtkWidget *ebox;

    char *lbl_title = NULL;
    char *layout_name = NULL;
    GtkWidget *label;
    static GHashTable *ln2cnt_map = NULL;

    ebox = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);

    layout_name = xfcekbd_indicator_extract_layout_name (groupId);
    gs_debug ("setting lang to %s", layout_name);

    lbl_title = xfcekbd_indicator_create_label_title (groupId, &ln2cnt_map, layout_name);

    label = gtk_label_new (lbl_title);
    gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (label, 2);
    gtk_widget_set_margin_end (label, 2);
    gtk_widget_set_margin_top (label, 2);
    gtk_widget_set_margin_bottom (label, 2);
    g_free (lbl_title);
    gtk_label_set_angle (GTK_LABEL (label), gki->priv->angle);

    if (groupId + 1 == xkl_engine_get_num_groups (globals.engine)) {
        g_hash_table_destroy (ln2cnt_map);
        ln2cnt_map = NULL;
    }

    gtk_container_add (GTK_CONTAINER (ebox), label);

    g_signal_connect (G_OBJECT (ebox),
                      "button_press_event",
                      G_CALLBACK (xfcekbd_indicator_button_pressed), gki);

    g_signal_connect (G_OBJECT (gki),
                      "key_press_event",
                      G_CALLBACK (xfcekbd_indicator_key_pressed), gki);

    /* We have everything prepared for that size */

    return ebox;
}

static void
xfcekbd_indicator_update_tooltips (XfcekbdIndicator *gki) {
    XklState *state = xkl_engine_get_current_state (globals.engine);
    const gchar** layouts = xkl_engine_get_groups_names (globals.engine);
    gchar *buf;

    if (state == NULL ||
            state->group < 0 ||
            state->group >= g_strv_length ((gchar **)layouts)) {
        return;
    }

    buf = g_strdup_printf (globals.tooltips_format, layouts[state->group]);
    gs_debug ("setting lang to %s", layouts[state->group]);
    xfcekbd_indicator_set_tooltips (gki, buf);
    g_free (buf);
}

static void
xfcekbd_indicator_parent_set (GtkWidget *gki,
                              GtkWidget *previous_parent) {
    xfcekbd_indicator_update_tooltips (XFCEKBD_INDICATOR (gki));
}

/* Should be called once for all applets */
static void
xfcekbd_indicator_state_callback (XklEngine            *engine,
                                  XklEngineStateChange  changeType,
                                  gint                  group,
                                  gboolean              restore) {
    gs_debug("group is now %d, restore: %d\n", group, restore);

    if (changeType == GROUP_CHANGED) {
        ForAllIndicators () {
            xfcekbd_indicator_set_current_page_for_group (gki, group);
        }
        NextIndicator ();
    }
}

void
xfcekbd_indicator_set_current_page (XfcekbdIndicator *gki) {
    XklState *cur_state;
    cur_state = xkl_engine_get_current_state (globals.engine);
    if (cur_state->group >= 0)
        xfcekbd_indicator_set_current_page_for_group (gki, cur_state->group);
}

void
xfcekbd_indicator_set_current_page_for_group (XfcekbdIndicator *gki,
                                              int               group) {
    gs_debug("Revalidating for group %d\n", group);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (gki), group + 1);

    xfcekbd_indicator_update_tooltips (gki);
}

/* Should be called once for all widgets */
static GdkFilterReturn
xfcekbd_indicator_filter_x_evt (GdkXEvent *xev,
                                GdkEvent  *event) {
    XEvent *xevent = (XEvent *) xev;

    xkl_engine_filter_events (globals.engine, xevent);
    switch (xevent->type) {
        case ReparentNotify:
            {
                if (!globals.redraw_queued)
                    return GDK_FILTER_CONTINUE;

                gs_debug("do repaint\n");
                XReparentEvent *rne = (XReparentEvent *) xev;

                ForAllIndicators () {
                    GdkWindow *w =
                        gtk_widget_get_parent_window
                        (GTK_WIDGET (gki));

                    /* compare the indicator's parent window with the even window */
                    if (w != NULL
                        && GDK_WINDOW_XID (w) == rne->window) {
                        /* if so - make it transparent... */
                        xkl_engine_set_window_transparent
                            (globals.engine, rne->window,
                            TRUE);
                    }
                }
                NextIndicator ()

                globals.redraw_queued = FALSE;
            }
            break;
    }
    return GDK_FILTER_CONTINUE;
}


/* Should be called once for all widgets */
static void
xfcekbd_indicator_start_listen (void) {
    gdk_window_add_filter (NULL, (GdkFilterFunc)
                           xfcekbd_indicator_filter_x_evt, NULL);
    gdk_window_add_filter (gdk_get_default_root_window (),
                           (GdkFilterFunc) xfcekbd_indicator_filter_x_evt, NULL);

    xkl_engine_start_listen (globals.engine,
                             XKLL_TRACK_KEYBOARD_STATE);
}

/* Should be called once for all widgets */
static void
xfcekbd_indicator_stop_listen (void) {
    xkl_engine_stop_listen (globals.engine, XKLL_TRACK_KEYBOARD_STATE);

    gdk_window_remove_filter (NULL, (GdkFilterFunc)
                              xfcekbd_indicator_filter_x_evt, NULL);
    gdk_window_remove_filter (gdk_get_default_root_window (),
                              (GdkFilterFunc) xfcekbd_indicator_filter_x_evt,
                              NULL);
}

static gboolean
xfcekbd_indicator_scroll (GtkWidget      *gki,
                          GdkEventScroll *event) {
    /* mouse wheel events should be ignored, otherwise funny effects appear */
    return TRUE;
}

static void xfcekbd_indicator_init(XfcekbdIndicator *gki) {
    GtkWidget   *def_drawing;
    GtkNotebook *notebook;

    if (!g_slist_length(globals.widget_instances)) {
        xfcekbd_indicator_global_init();
    }

    gki->priv = g_new0 (XfcekbdIndicatorPrivate, 1);

    notebook = GTK_NOTEBOOK (gki);

    gs_debug( "Initiating the widget startup process for %p\n", gki);

    gtk_notebook_set_show_tabs (notebook, FALSE);
    gtk_notebook_set_show_border (notebook, FALSE);

    def_drawing = gtk_image_new_from_icon_name ("process-stop",
                                                GTK_ICON_SIZE_BUTTON);

    gtk_notebook_append_page (notebook, def_drawing, gtk_label_new (""));

    if (globals.engine == NULL) {
        xfcekbd_indicator_set_tooltips (gki, _("XKB initialization error"));
        return;
    }

    xfcekbd_indicator_set_tooltips (gki, NULL);

    xfcekbd_indicator_fill (gki);
    xfcekbd_indicator_set_current_page (gki);

    gtk_widget_add_events (GTK_WIDGET (gki), GDK_BUTTON_PRESS_MASK);

    /* append AFTER all initialization work is finished */
    globals.widget_instances =
        g_slist_append (globals.widget_instances, gki);
}

static void
xfcekbd_indicator_finalize (GObject *obj) {
    XfcekbdIndicator *gki = XFCEKBD_INDICATOR (obj);
    gs_debug("Starting the xfce-kbd-indicator widget shutdown process for %p", gki);

    /* remove BEFORE all termination work is finished */
    globals.widget_instances = g_slist_remove (globals.widget_instances, gki);

    xfcekbd_indicator_cleanup (gki);

    gs_debug("The instance of xfce-kbd-indicator successfully finalized");

    g_free (gki->priv);

    G_OBJECT_CLASS (xfcekbd_indicator_parent_class)->finalize (obj);

    if (!g_slist_length (globals.widget_instances))
        xfcekbd_indicator_global_term ();
}

static void
xfcekbd_indicator_global_term (void) {
    gs_debug( "*** Last  XfcekbdIndicator instance ***\n");
    xfcekbd_indicator_stop_listen ();

    g_object_unref (G_OBJECT (globals.engine));
    globals.engine = NULL;
    gs_debug( "*** Terminated globals *** \n");
}

static void
xfcekbd_indicator_class_init (XfcekbdIndicatorClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gs_debug( "*** First XfcekbdIndicator instance ***");

    memset (&globals, 0, sizeof (globals));

    /* Initing some global vars */
    globals.tooltips_format = "%s";
    globals.redraw_queued = FALSE;

    /* Initing vtable */
    object_class->finalize = xfcekbd_indicator_finalize;
    widget_class->scroll_event = xfcekbd_indicator_scroll;
    widget_class->parent_set = xfcekbd_indicator_parent_set;
}

static void
xfcekbd_indicator_global_init (void) {
    globals.engine = xkl_engine_get_instance(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
    if (globals.engine == NULL) {
        gs_debug("Libxklavier initialization error");
        return;
    }
    g_signal_connect (globals.engine, "X-state-changed",
                      G_CALLBACK (xfcekbd_indicator_state_callback), NULL);
    xfcekbd_indicator_start_listen ();
    gs_debug( "*** Inited globals ***");
}

GtkWidget *
xfcekbd_indicator_new (void) {
    return GTK_WIDGET (g_object_new (xfcekbd_indicator_get_type (), NULL));
}

void
xfcekbd_indicator_set_parent_tooltips (XfcekbdIndicator * gki, gboolean spt) {
    gki->priv->set_parent_tooltips = spt;
    xfcekbd_indicator_update_tooltips (gki);
}
