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

#ifndef SRC_XFCEKBD_CONFIG_PRIVATE_H_
#define SRC_XFCEKBD_CONFIG_PRIVATE_H_

#include "src/xfcekbd-desktop-config.h"
#include "src/xfcekbd-keyboard-config.h"

#define SETTINGS_XFCONF_CHANNEL "xfce4-screensaver"

/**
 * KBD/General: Default group, assigned on window creation
 */
#define KEY_KBD_DEFAULT_GROUP "/xkb/default-group"
#define DEFAULT_KEY_KBD_DEFAULT_GROUP -1

/**
 * KBD/General: Keep and manage separate group per window
 */
#define KEY_KBD_GROUP_PER_WINDOW "/xkb/group-per-window"
#define DEFAULT_KEY_KBD_GROUP_PER_WINDOW TRUE

/**
 * KBD/General: Save/restore indicators together with layout groups
 */
#define KEY_KBD_HANDLE_INDICATORS "/xkb/handle-indicators"
#define DEFAULT_KEY_KBD_HANDLE_INDICATORS FALSE

/**
 * KBD/General: Load extra configuration items
 * Load exotic, rarely used layouts and options
 */
#define KEY_KBD_LOAD_EXTRA_ITEMS "/xkb/load-extra-items"
#define DEFAULT_KEY_KBD_LOAD_EXTRA_ITEMS FALSE

/**
 * KBD/Indicator: Secondary groups
 */
#define KEY_KBD_SECONDARY_GROUPS "/xkb/secondary-groups"
#define DEFAULT_KEY_KBD_SECONDARY_GROUPS 0

/**
 * General config functions (private)
 */
extern void xfcekbd_keyboard_config_model_set   (XfcekbdKeyboardConfig  *kbd_config,
                                                 const gchar            *model_name);

extern void xfcekbd_keyboard_config_options_set (XfcekbdKeyboardConfig  *kbd_config,
                                                 gint                    idx,
                                                 const gchar            *group_name,
                                                 const gchar            *option_name);

#endif /* SRC_XFCEKBD_CONFIG_PRIVATE_H_ */
