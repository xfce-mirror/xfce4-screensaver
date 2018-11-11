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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>

#include <gdk/gdkx.h>
#include <pango/pango.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "src/xfcekbd-keyboard-config.h"
#include "src/xfcekbd-indicator-config.h"
#include "src/xfcekbd-config-private.h"

/*
 * static applet config functions
 */

static gchar *
xfcekbd_indicator_config_get_images_file (XfcekbdIndicatorConfig *ind_config,
                                          XfcekbdKeyboardConfig  *kbd_config,
                                          int                     group) {
    char        *image_file = NULL;
    GtkIconInfo *icon_info = NULL;

    if (!ind_config->show_flags)
        return NULL;

    if ((kbd_config->layouts_variants != NULL) &&
        (g_strv_length (kbd_config->layouts_variants) > group)) {
        char *full_layout_name =
            kbd_config->layouts_variants[group];

        if (full_layout_name != NULL) {
            char *l, *v;
            xfcekbd_keyboard_config_split_items (full_layout_name,
                              &l, &v);
            if (l != NULL) {
                /* probably there is something in theme? */
                icon_info = gtk_icon_theme_lookup_icon
                    (ind_config->icon_theme, l, 48, 0);

                /* Unbelievable but happens */
                if (icon_info != NULL &&
                    gtk_icon_info_get_filename (icon_info) == NULL) {
                    g_object_unref (icon_info);
                    icon_info = NULL;
                }
            }
        }
    }
    /* fallback to the default value */
    if (icon_info == NULL) {
        icon_info = gtk_icon_theme_lookup_icon
            (ind_config->icon_theme, "stock_dialog-error", 48, 0);
    }
    if (icon_info != NULL) {
        image_file =
            g_strdup (gtk_icon_info_get_filename (icon_info));
        g_object_unref (icon_info);
    }

    return image_file;
}

void
xfcekbd_indicator_config_load_image_filenames (XfcekbdIndicatorConfig *
                                               ind_config,
                                               XfcekbdKeyboardConfig *
                                               kbd_config) {
    int i;
    ind_config->image_filenames = NULL;

    if (!ind_config->show_flags)
        return;

    for (i = xkl_engine_get_max_num_groups (ind_config->engine);
         --i >= 0;) {
        gchar *image_file =
            xfcekbd_indicator_config_get_images_file (ind_config,
                               kbd_config,
                               i);
        ind_config->image_filenames =
            g_slist_prepend (ind_config->image_filenames,
                     image_file);
    }
}

void
xfcekbd_indicator_config_free_image_filenames (XfcekbdIndicatorConfig *ind_config) {
    while (ind_config->image_filenames) {
        if (ind_config->image_filenames->data)
            g_free (ind_config->image_filenames->data);
        ind_config->image_filenames =
            g_slist_delete_link (ind_config->image_filenames,
                     ind_config->image_filenames);
    }
}

void
xfcekbd_indicator_config_init (XfcekbdIndicatorConfig *ind_config,
                               XklEngine              *engine) {
    gchar *sp;

    memset (ind_config, 0, sizeof (*ind_config));
    ind_config->channel = xfconf_channel_get (SETTINGS_XFCONF_CHANNEL);
    ind_config->engine = engine;

    ind_config->icon_theme = gtk_icon_theme_get_default ();

    gtk_icon_theme_append_search_path (ind_config->icon_theme, sp =
                       g_build_filename (g_get_home_dir
                                 (),
                                 ".icons/flags",
                                 NULL));
    g_free (sp);

    gtk_icon_theme_append_search_path (ind_config->icon_theme,
                       sp =
                       g_build_filename (DATADIR,
                                 "pixmaps/flags",
                                 NULL));
    g_free (sp);

    gtk_icon_theme_append_search_path (ind_config->icon_theme,
                       sp =
                       g_build_filename (DATADIR,
                                 "icons/flags",
                                 NULL));
    g_free (sp);
}

void
xfcekbd_indicator_config_term (XfcekbdIndicatorConfig *ind_config) {
    g_free (ind_config->font_family);
    ind_config->font_family = NULL;

    ind_config->icon_theme = NULL;

    xfcekbd_indicator_config_free_image_filenames (ind_config);

    g_object_unref (ind_config->channel);
    ind_config->channel = NULL;
}

void
xfcekbd_indicator_config_load_from_xfconf (XfcekbdIndicatorConfig * ind_config) {
    ind_config->secondary_groups_mask =
        xfconf_channel_get_int (ind_config->channel,
                KEY_KBD_INDICATOR_SECONDARIES,
                DEFAULT_KEY_KBD_INDICATOR_SECONDARIES);

    ind_config->show_flags =
        xfconf_channel_get_bool (ind_config->channel,
                 KEY_KBD_INDICATOR_SHOW_FLAGS,
                 DEFAULT_KEY_KBD_INDICATOR_SHOW_FLAGS);
}

void
xfcekbd_indicator_config_activate (XfcekbdIndicatorConfig * ind_config) {
    xkl_engine_set_secondary_groups_mask (ind_config->engine,
                          ind_config->secondary_groups_mask);
}

/**
 * xfcekbd_indicator_config_start_listen:
 * @func: (scope notified): a function to call when settings are changed
 */
void
xfcekbd_indicator_config_start_listen (XfcekbdIndicatorConfig *ind_config,
                                       GCallback               func,
                                       gpointer                user_data) {
    ind_config->config_listener_id =
        g_signal_connect (ind_config->channel,
                          "property-changed",
                          func,
                          user_data);
}

void
xfcekbd_indicator_config_stop_listen (XfcekbdIndicatorConfig *ind_config) {
    g_signal_handler_disconnect (ind_config->channel,
                                 ind_config->config_listener_id);
    ind_config->config_listener_id = 0;
}
