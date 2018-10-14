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

#ifndef __XFCEKBD_KEYBOARD_CONFIG_H__
#define __XFCEKBD_KEYBOARD_CONFIG_H__

#include <X11/Xlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <libxklavier/xklavier.h>

extern const gchar XFCEKBD_KEYBOARD_CONFIG_KEY_MODEL[];
extern const gchar XFCEKBD_KEYBOARD_CONFIG_KEY_LAYOUTS[];
extern const gchar XFCEKBD_KEYBOARD_CONFIG_KEY_OPTIONS[];

/*
 * Keyboard Configuration
 */
typedef struct _XfcekbdKeyboardConfig XfcekbdKeyboardConfig;
struct _XfcekbdKeyboardConfig {
    gchar      *model;
    gchar     **layouts_variants;
    gchar     **options;

    /* private, transient */
    int         config_listener_id;
    XklEngine  *engine;
};

/*
 * XfcekbdKeyboardConfig functions
 */
extern void             xfcekbd_keyboard_config_init                    (XfcekbdKeyboardConfig  *kbd_config,
                                                                         XklEngine              *engine);
extern void             xfcekbd_keyboard_config_term                    (XfcekbdKeyboardConfig  *kbd_config);

extern void             xfcekbd_keyboard_config_load_from_x_current     (XfcekbdKeyboardConfig  *kbd_config,
                                                                         XklConfigRec           *buf);

extern gboolean         xfcekbd_keyboard_config_split_items             (const gchar            *merged,
                                                                         gchar                 **parent,
                                                                         gchar                 **child);

extern const gchar *    xfcekbd_keyboard_config_format_full_layout      (const gchar            *layout_descr,
                                                                         const gchar            *variant_descr);

#endif
