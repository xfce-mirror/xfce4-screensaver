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

#ifndef __XFCEKBD_DESKTOP_CONFIG_H__
#define __XFCEKBD_DESKTOP_CONFIG_H__

#include <X11/Xlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <libxklavier/xklavier.h>

#include <xfconf/xfconf.h>

/*
 * General configuration
 */
typedef struct _XfcekbdDesktopConfig XfcekbdDesktopConfig;
struct _XfcekbdDesktopConfig {
	gint default_group;
	gboolean group_per_app;
	gboolean handle_indicators;
	gboolean layout_names_as_group_names;
	gboolean load_extra_items;

	/* private, transient */
	XfconfChannel *channel;
	int config_listener_id;
	XklEngine *engine;
};

/*
 * XfcekbdDesktopConfig functions
 */
extern void xfcekbd_desktop_config_init (XfcekbdDesktopConfig * config,
				      XklEngine * engine);
extern void xfcekbd_desktop_config_term (XfcekbdDesktopConfig * config);

extern void xfcekbd_desktop_config_load_from_xfconf (XfcekbdDesktopConfig *
						 config);

extern gboolean xfcekbd_desktop_config_activate (XfcekbdDesktopConfig * config);

extern gboolean
xfcekbd_desktop_config_load_group_descriptions (XfcekbdDesktopConfig
					     * config,
					     XklConfigRegistry *
					     registry,
					     const gchar **
					     layout_ids,
					     const gchar **
					     variant_ids,
					     gchar ***
					     short_group_names,
					     gchar *** full_group_names);

extern void xfcekbd_desktop_config_lock_next_group (XfcekbdDesktopConfig *
						 config);

extern void xfcekbd_desktop_config_start_listen (XfcekbdDesktopConfig * config,
					      GCallback func,
					      gpointer user_data);

extern void xfcekbd_desktop_config_stop_listen (XfcekbdDesktopConfig * config);

#endif
