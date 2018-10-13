/*
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
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

#ifndef __XFCEKBD_CONFIG_PRIVATE_H__
#define __XFCEKBD_CONFIG_PRIVATE_H__

#include "xfcekbd-desktop-config.h"
#include "xfcekbd-keyboard-config.h"

#define SETTINGS_XFCONF_CHANNEL "xfce4-screensaver"

/**
 * KBD/General: Default group, assigned on window creation
 */
#define KEY_KBD_DEFAULT_GROUP "/kbd/general/default-group"
#define DEFAULT_KEY_KBD_DEFAULT_GROUP -1

/**
 * KBD/General: Keep and manage separate group per window
 */
#define KEY_KBD_GROUP_PER_WINDOW "/kbd/general/group-per-window"
#define DEFAULT_KEY_KBD_GROUP_PER_WINDOW TRUE

/**
 * KBD/General: Save/restore indicators together with layout groups
 */
#define KEY_KBD_HANDLE_INDICATORS "/kbd/general/handle-indicators"
#define DEFAULT_KEY_KBD_HANDLE_INDICATORS FALSE

/**
 * KBD/General: Show layout names instead of group names
 * Only for versions of XFree supporting multiple layouts
 */
#define KEY_KBD_LAYOUT_NAMES_AS_GROUP_NAMES "/kbd/general/layout-names-as-group-names"
#define DEFAULT_KEY_KBD_LAYOUT_NAMES_AS_GROUP_NAMES TRUE

/**
 * KBD/General: Load extra configuration items
 * Load exotic, rarely used layouts and options
 */
#define KEY_KBD_LOAD_EXTRA_ITEMS "/kbd/general/load-extra-items"
#define DEFAULT_KEY_KBD_LOAD_EXTRA_ITEMS FALSE

/**
 * KBD/Indicator: Show flags instead of language code
 */
#define KEY_KBD_INDICATOR_SHOW_FLAGS "/kbd/indicator/show-flags"
#define DEFAULT_KEY_KBD_INDICATOR_SHOW_FLAGS FALSE

/**
 * KBD/Indicator: Secondary groups
 */
#define KEY_KBD_INDICATOR_SECONDARIES "/kbd/indicator/secondary"
#define DEFAULT_KEY_KBD_INDICATOR_SECONDARIES 0

/**
 * KBD/Indicator: The font
 * The font for the layout indicator. This should be in 
 * "[FAMILY-LIST] [STYLE-OPTIONS] [SIZE]" format.
 */
#define KEY_KBD_INDICATOR_FONT_FAMILY "/kbd/indicator/font-family"
#define DEFAULT_KEY_KBD_INDICATOR_FONT_FAMILY ""

/**
 * KBD/Indicator: The foreground color
 * The foreground color for the layout indicator. 
 * This should be in "R G B" format, for example "255 0 0".
 */
#define KEY_KBD_INDICATOR_FOREGROUND_COLOR "/kbd/indicator/foreground-color"
#define DEFAULT_KEY_KBD_INDICATOR_FOREGROUND_COLOR ""

/**
 * KBD/Indicator: The background color
 * The background color for the layout indicator. 
 * This should be in "R G B" format, for example "255 0 0".
 */
#define KEY_KBD_INDICATOR_BACKGROUND_COLOR "/kbd/indicator/background-color"
#define DEFAULT_KEY_KBD_INDICATOR_BACKGROUND_COLOR ""

/**
 * General config functions (private)
 */
extern void xfcekbd_keyboard_config_model_set (XfcekbdKeyboardConfig *
					    kbd_config,
					    const gchar * model_name);

extern void xfcekbd_keyboard_config_options_set (XfcekbdKeyboardConfig *
					      kbd_config,
					      gint idx,
					      const gchar * group_name,
					      const gchar * option_name);

#endif
