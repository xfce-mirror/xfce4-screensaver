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

#include "xfcekbd-keyboard-config.h"
#include "xfcekbd-indicator-config.h"
#include "xfcekbd-config-private.h"

/*
 * static applet config functions
 */

void
xfcekbd_indicator_config_init (XfcekbdIndicatorConfig *ind_config,
                               XklEngine              *engine) {
    memset (ind_config, 0, sizeof (*ind_config));
    ind_config->channel = xfconf_channel_get (SETTINGS_XFCONF_CHANNEL);
    ind_config->engine = engine;
}

void
xfcekbd_indicator_config_term (XfcekbdIndicatorConfig *ind_config) {
    g_object_unref (ind_config->channel);
    ind_config->channel = NULL;
}

void
xfcekbd_indicator_config_load_from_xfconf (XfcekbdIndicatorConfig * ind_config) {
    ind_config->secondary_groups_mask =
        xfconf_channel_get_int (ind_config->channel,
                KEY_KBD_SECONDARY_GROUPS,
                DEFAULT_KEY_KBD_SECONDARY_GROUPS);
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
