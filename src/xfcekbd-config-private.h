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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __XFCEKBD_CONFIG_PRIVATE_H__
#define __XFCEKBD_CONFIG_PRIVATE_H__

#include "xfcekbd-desktop-config.h"
#include "xfcekbd-keyboard-config.h"

#define XFCEKBD_CONFIG_SCHEMA "org.mate.peripherals-keyboard-xkb"

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
