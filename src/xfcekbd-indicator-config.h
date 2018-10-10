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

#ifndef __XFCEKBD_INDICATOR_CONFIG_H__
#define __XFCEKBD_INDICATOR_CONFIG_H__

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "xfcekbd-keyboard-config.h"

/*
 * Indicator configuration
 */
typedef struct _XfcekbdIndicatorConfig XfcekbdIndicatorConfig;
struct _XfcekbdIndicatorConfig {
	int secondary_groups_mask;
	gboolean show_flags;

	gchar *font_family;
	gchar *foreground_color;
	gchar *background_color;

	/* private, transient */
	XfconfChannel *channel;
	GSList *image_filenames;
	GtkIconTheme *icon_theme;
	int config_listener_id;
	XklEngine *engine;
};

/*
 * XfcekbdIndicatorConfig functions -
 * some of them require XfcekbdKeyboardConfig as well -
 * for loading approptiate images
 */
void xfcekbd_indicator_config_init (XfcekbdIndicatorConfig *
					applet_config,
					XklEngine * engine);
void xfcekbd_indicator_config_term (XfcekbdIndicatorConfig *
					applet_config);

void xfcekbd_indicator_config_load_from_xfconf (XfcekbdIndicatorConfig
						   * applet_config);

void xfcekbd_indicator_config_load_image_filenames (XfcekbdIndicatorConfig
							* applet_config,
							XfcekbdKeyboardConfig
							* kbd_config);
void xfcekbd_indicator_config_free_image_filenames (XfcekbdIndicatorConfig
							* applet_config);

/* Should be updated on Indicator/Xfconf configuration change */
void xfcekbd_indicator_config_activate (XfcekbdIndicatorConfig *
					    applet_config);

void xfcekbd_indicator_config_start_listen (XfcekbdIndicatorConfig *
						applet_config,
						GCallback
						func, gpointer user_data);

void xfcekbd_indicator_config_stop_listen (XfcekbdIndicatorConfig *
					       applet_config);

#endif
