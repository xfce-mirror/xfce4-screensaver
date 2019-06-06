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

#include "xfcekbd-desktop-config.h"
#include "xfcekbd-indicator.h"
#include "xfcekbd-indicator-config.h"
#include "xfcekbd-indicator-marshal.h"

typedef struct _gki_globals {
    XklEngine               *engine;
    XklConfigRegistry       *registry;

    XfcekbdDesktopConfig     cfg;
    XfcekbdIndicatorConfig   ind_cfg;
    XfcekbdKeyboardConfig    kbd_cfg;

    const gchar             *tooltips_format;
    gchar                  **full_group_names;
    gchar                  **short_group_names;
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
    switch (event->keyval) {
        case GDK_KEY_KP_Enter:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_3270_Enter:
        case GDK_KEY_Return:
        case GDK_KEY_space:
        case GDK_KEY_KP_Space:
            xfcekbd_desktop_config_lock_next_group(&globals.cfg);
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
        xkl_debug (150, "Mouse button pressed on applet\n");
        xfcekbd_desktop_config_lock_next_group (&globals.cfg);
        globals.redraw_queued = TRUE;
        return TRUE;
    }
    return FALSE;
}

static gchar *
xfcekbd_indicator_extract_layout_name (int                     group,
                                       XklEngine              *engine,
                                       XfcekbdKeyboardConfig  *kbd_cfg,
                                       gchar                 **short_group_names,
                                       gchar                 **full_group_names) {
    char *layout_name = NULL;
    if (group < g_strv_length (short_group_names)) {
        if (xkl_engine_get_features (engine) & XKLF_MULTIPLE_LAYOUTS_SUPPORTED) {
            char *full_layout_name = kbd_cfg->layouts_variants[group];
            char *variant_name;
            if (!xfcekbd_keyboard_config_split_items (full_layout_name,
                                                      &layout_name,
                                                      &variant_name)) {
                /* just in case */
                layout_name = full_layout_name;
            }

            /* make it freeable */
            layout_name = g_strdup (layout_name);

            if (short_group_names != NULL) {
                char *short_group_name = short_group_names[group];
                if (short_group_name != NULL && *short_group_name != '\0') {
                    /* drop the long name */
                    g_free (layout_name);
                    layout_name =
                        g_strdup (short_group_name);
                }
            }
        } else {
            layout_name = g_strdup (full_group_names[group]);
        }
    }

    if (layout_name == NULL)
        layout_name = g_strdup ("");

    return layout_name;
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
                                   int               group) {
    GtkWidget *ebox;

    char *lbl_title = NULL;
    char *layout_name = NULL;
    GtkWidget *label;
    static GHashTable *ln2cnt_map = NULL;

    ebox = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);

    layout_name =
        xfcekbd_indicator_extract_layout_name (group,
                                                globals.engine,
                                                &globals.kbd_cfg,
                                                globals.short_group_names,
                                                globals.full_group_names);

    lbl_title =
        xfcekbd_indicator_create_label_title (group,
                                                &ln2cnt_map,
                                                layout_name);

    label = gtk_label_new (lbl_title);
    gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (label, 2);
    gtk_widget_set_margin_end (label, 2);
    gtk_widget_set_margin_top (label, 2);
    gtk_widget_set_margin_bottom (label, 2);
    g_free (lbl_title);
    gtk_label_set_angle (GTK_LABEL (label), gki->priv->angle);

    if (group + 1 == xkl_engine_get_num_groups (globals.engine)) {
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
    gchar    *buf;
    if (state == NULL ||
            state->group < 0 ||
            state->group >= g_strv_length (globals.full_group_names)) {
        return;
    }

    buf = g_strdup_printf (globals.tooltips_format, globals.full_group_names[state->group]);

    xfcekbd_indicator_set_tooltips (gki, buf);
    g_free (buf);
}

static void
xfcekbd_indicator_parent_set (GtkWidget *gki,
                              GtkWidget *previous_parent) {
    xfcekbd_indicator_update_tooltips (XFCEKBD_INDICATOR (gki));
}


static void
xfcekbd_indicator_reinit_ui (XfcekbdIndicator *gki) {
    xfcekbd_indicator_cleanup (gki);
    xfcekbd_indicator_fill (gki);

    xfcekbd_indicator_set_current_page (gki);

    g_signal_emit_by_name (gki, "reinit-ui");
}

/* Should be called once for all widgets */
static void
xfcekbd_indicator_cfg_changed (XfconfChannel *channel,
                               gchar         *key,
                               gpointer       user_data) {
    xkl_debug (100, "General configuration changed in Xfconf - reiniting...\n");
    xfcekbd_desktop_config_load_from_xfconf (&globals.cfg);
    xfcekbd_desktop_config_activate (&globals.cfg);
    ForAllIndicators () {
        xfcekbd_indicator_reinit_ui (gki);
    } NextIndicator ();
}

/* Should be called once for all widgets */
static void
xfcekbd_indicator_ind_cfg_changed (XfconfChannel *channel,
                                  gchar          *key,
                                  gpointer        user_data) {
    xkl_debug (100, "Applet configuration changed in Xfconf - reiniting...\n");
    xfcekbd_indicator_config_load_from_xfconf (&globals.ind_cfg);
    xfcekbd_indicator_config_activate (&globals.ind_cfg);

    ForAllIndicators () {
        xfcekbd_indicator_reinit_ui (gki);
    } NextIndicator ();
}

static void
xfcekbd_indicator_load_group_names (const gchar **layout_ids,
                                    const gchar **variant_ids) {
    if (!xfcekbd_desktop_config_load_group_descriptions (&globals.cfg,
                                                         globals.registry,
                                                         layout_ids,
                                                         variant_ids,
                                                         &globals.short_group_names,
                                                         &globals.full_group_names)) {
        /* We just populate no short names (remain NULL) -
         * full names are going to be used anyway */
        gint i, total_groups = xkl_engine_get_num_groups (globals.engine);
        globals.full_group_names = g_new0 (gchar *, total_groups + 1);

        if (xkl_engine_get_features (globals.engine) & XKLF_MULTIPLE_LAYOUTS_SUPPORTED) {
            gchar **lst = globals.kbd_cfg.layouts_variants;
            for (i = 0; *lst; lst++, i++) {
                globals.full_group_names[i] = g_strdup ((char *) *lst);
            }
        } else {
            for (i = total_groups; --i >= 0;) {
                globals.full_group_names[i] = g_strdup_printf ("Group %d", i);
            }
        }
    }
}

/* Should be called once for all widgets */
static void
xfcekbd_indicator_kbd_cfg_callback (XfcekbdIndicator *gki) {
    XklConfigRec *xklrec = xkl_config_rec_new ();
    xkl_debug (100, "XKB configuration changed on X Server - reiniting...\n");

    xfcekbd_keyboard_config_load_from_x_current (&globals.kbd_cfg, xklrec);

    g_strfreev (globals.full_group_names);
    globals.full_group_names = NULL;

    if (globals.short_group_names != NULL) {
        g_strfreev (globals.short_group_names);
        globals.short_group_names = NULL;
    }

    xfcekbd_indicator_load_group_names ((const gchar **) xklrec->layouts,
                                        (const gchar **)
                                        xklrec->variants);

    ForAllIndicators () {
        xfcekbd_indicator_reinit_ui (gki);
    } NextIndicator ();
    g_object_unref (G_OBJECT (xklrec));
}

/* Should be called once for all applets */
static void
xfcekbd_indicator_state_callback (XklEngine            *engine,
                                  XklEngineStateChange  changeType,
                                  gint                  group,
                                  gboolean              restore) {
    xkl_debug (150, "group is now %d, restore: %d\n", group, restore);

    if (changeType == GROUP_CHANGED) {
        ForAllIndicators () {
            xkl_debug (200, "do repaint\n");
            xfcekbd_indicator_set_current_page_for_group
                (gki, group);
        }
        NextIndicator ();
    }
}


void
xfcekbd_indicator_set_current_page (XfcekbdIndicator *gki) {
    XklState *cur_state;
    cur_state = xkl_engine_get_current_state (globals.engine);
    if (cur_state->group >= 0)
        xfcekbd_indicator_set_current_page_for_group (gki,
                                                      cur_state->group);
}

void
xfcekbd_indicator_set_current_page_for_group (XfcekbdIndicator *gki,
                                              int               group) {
    xkl_debug (200, "Revalidating for group %d\n", group);

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

    xkl_debug (100, "Initiating the widget startup process for %p\n", gki);

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
    xkl_debug (100,
               "Starting the xfce-kbd-indicator widget shutdown process for %p\n",
               gki);

    /* remove BEFORE all termination work is finished */
    globals.widget_instances = g_slist_remove (globals.widget_instances, gki);

    xfcekbd_indicator_cleanup (gki);

    xkl_debug (100,
               "The instance of xfce-kbd-indicator successfully finalized\n");

    g_free (gki->priv);

    G_OBJECT_CLASS (xfcekbd_indicator_parent_class)->finalize (obj);

    if (!g_slist_length (globals.widget_instances))
        xfcekbd_indicator_global_term ();
}

static void
xfcekbd_indicator_global_term (void) {
    xkl_debug (100, "*** Last  XfcekbdIndicator instance ***\n");
    xfcekbd_indicator_stop_listen ();

    xfcekbd_desktop_config_stop_listen (&globals.cfg);
    xfcekbd_indicator_config_stop_listen (&globals.ind_cfg);

    xfcekbd_indicator_config_term (&globals.ind_cfg);
    xfcekbd_keyboard_config_term (&globals.kbd_cfg);
    xfcekbd_desktop_config_term (&globals.cfg);

    g_object_unref (G_OBJECT (globals.registry));
    globals.registry = NULL;
    g_object_unref (G_OBJECT (globals.engine));
    globals.engine = NULL;
    xkl_debug (100, "*** Terminated globals *** \n");
}

static void
xfcekbd_indicator_class_init (XfcekbdIndicatorClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    xkl_debug (100, "*** First XfcekbdIndicator instance *** \n");

    memset (&globals, 0, sizeof (globals));

    /* Initing some global vars */
    globals.tooltips_format = "%s";

    globals.redraw_queued = TRUE;

    /* Initing vtable */
    object_class->finalize = xfcekbd_indicator_finalize;

    widget_class->scroll_event = xfcekbd_indicator_scroll;
    widget_class->parent_set = xfcekbd_indicator_parent_set;

    /* Signals */
    g_signal_new ("reinit-ui",
                  XFCEKBD_TYPE_INDICATOR,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (XfcekbdIndicatorClass, reinit_ui),
                  NULL, NULL, xfcekbd_indicator_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
xfcekbd_indicator_global_init (void) {
    XklConfigRec *xklrec = xkl_config_rec_new ();

    globals.engine = xkl_engine_get_instance(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));

    if (globals.engine == NULL) {
        xkl_debug (0, "Libxklavier initialization error");
        return;
    }

    g_signal_connect (globals.engine,
                      "X-state-changed",
                      G_CALLBACK (xfcekbd_indicator_state_callback),
                      NULL);
    g_signal_connect (globals.engine,
                      "X-config-changed",
                      G_CALLBACK (xfcekbd_indicator_kbd_cfg_callback),
                      NULL);

    xfcekbd_desktop_config_init (&globals.cfg, globals.engine);
    xfcekbd_keyboard_config_init (&globals.kbd_cfg, globals.engine);
    xfcekbd_indicator_config_init (&globals.ind_cfg, globals.engine);

    xfcekbd_desktop_config_start_listen (&globals.cfg,
                                         (GCallback)
                                         xfcekbd_indicator_cfg_changed,
                                         NULL);
    xfcekbd_indicator_config_start_listen (&globals.ind_cfg,
                                         (GCallback)
                                         xfcekbd_indicator_ind_cfg_changed,
                                         NULL);

    xfcekbd_desktop_config_load_from_xfconf (&globals.cfg);
    xfcekbd_desktop_config_activate (&globals.cfg);

    globals.registry = xkl_config_registry_get_instance (globals.engine);
    xkl_config_registry_load (globals.registry, globals.cfg.load_extra_items);

    xfcekbd_keyboard_config_load_from_x_current (&globals.kbd_cfg, xklrec);

    xfcekbd_indicator_config_load_from_xfconf (&globals.ind_cfg);
    xfcekbd_indicator_config_activate (&globals.ind_cfg);

    xfcekbd_indicator_load_group_names ((const gchar **) xklrec->layouts,
                                        (const gchar **)
                                        xklrec->variants);
    g_object_unref (G_OBJECT (xklrec));

    xfcekbd_indicator_start_listen ();

    xkl_debug (100, "*** Inited globals *** \n");
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
