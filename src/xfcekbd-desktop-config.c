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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>

#include <xfconf/xfconf.h>

#include <libxfce4util/libxfce4util.h>
#include <gio/gio.h>
#include "xfcekbd-desktop-config.h"
#include "xfcekbd-config-private.h"

/*
 * static common functions
 */

static gboolean
    xfcekbd_desktop_config_get_lv_descriptions
    (XfcekbdDesktopConfig * config,
     XklConfigRegistry * registry,
     const gchar ** layout_ids,
     const gchar ** variant_ids,
     gchar *** short_layout_descriptions,
     gchar *** long_layout_descriptions,
     gchar *** short_variant_descriptions,
     gchar *** long_variant_descriptions) {
	const gchar **pl, **pv;
	guint total_layouts;
	gchar **sld, **lld, **svd, **lvd;
	XklConfigItem *item = xkl_config_item_new ();

	if (!
	    (xkl_engine_get_features (config->engine) &
	     XKLF_MULTIPLE_LAYOUTS_SUPPORTED))
		return FALSE;

	pl = layout_ids;
	pv = variant_ids;
	total_layouts = g_strv_length ((char **) layout_ids);
	sld = *short_layout_descriptions =
	    g_new0 (gchar *, total_layouts + 1);
	lld = *long_layout_descriptions =
	    g_new0 (gchar *, total_layouts + 1);
	svd = *short_variant_descriptions =
	    g_new0 (gchar *, total_layouts + 1);
	lvd = *long_variant_descriptions =
	    g_new0 (gchar *, total_layouts + 1);

	while (pl != NULL && *pl != NULL) {

		xkl_debug (100, "ids: [%s][%s]\n", *pl,
			   pv == NULL ? NULL : *pv);

		g_snprintf (item->name, sizeof item->name, "%s", *pl);
		if (xkl_config_registry_find_layout (registry, item)) {
			*sld = g_strdup (item->short_description);
			*lld = g_strdup (item->description);
		} else {
			*sld = g_strdup ("");
			*lld = g_strdup ("");
		}

		if (pv != NULL && *pv != NULL) {
			g_snprintf (item->name, sizeof item->name, "%s",
				    *pv);
			if (xkl_config_registry_find_variant
			    (registry, *pl, item)) {
				*svd = g_strdup (item->short_description);
				*lvd = g_strdup (item->description);
			} else {
				*svd = g_strdup ("");
				*lvd = g_strdup ("");
			}
		} else {
			*svd = g_strdup ("");
			*lvd = g_strdup ("");
		}

		xkl_debug (100, "description: [%s][%s][%s][%s]\n",
			   *sld, *lld, *svd, *lvd);
		sld++;
		lld++;
		svd++;
		lvd++;

		pl++;

		if (pv != NULL && *pv != NULL)
			pv++;
	}

	g_object_unref (item);
	return TRUE;
}

/*
 * extern XfcekbdDesktopConfig config functions
 */
void
xfcekbd_desktop_config_init (XfcekbdDesktopConfig * config,
			  XklEngine * engine)
{
	memset (config, 0, sizeof (*config));
	config->channel = xfconf_channel_get (SETTINGS_XFCONF_CHANNEL);
	config->engine = engine;
}

void
xfcekbd_desktop_config_term (XfcekbdDesktopConfig * config)
{
	g_object_unref (config->channel);
	config->channel = NULL;
}

void
xfcekbd_desktop_config_load_from_xfconf (XfcekbdDesktopConfig * config)
{
	config->group_per_app =
		xfconf_channel_get_bool(config->channel,
								KEY_KBD_GROUP_PER_WINDOW,
								DEFAULT_KEY_KBD_GROUP_PER_WINDOW);
	xkl_debug (150, "group_per_app: %d\n", config->group_per_app);

	config->handle_indicators =
		xfconf_channel_get_bool(config->channel,
								KEY_KBD_HANDLE_INDICATORS,
								DEFAULT_KEY_KBD_HANDLE_INDICATORS);
	xkl_debug (150, "handle_indicators: %d\n",
		   config->handle_indicators);

	config->layout_names_as_group_names =
		xfconf_channel_get_bool(config->channel,
								KEY_KBD_LAYOUT_NAMES_AS_GROUP_NAMES,
								DEFAULT_KEY_KBD_LAYOUT_NAMES_AS_GROUP_NAMES);
	xkl_debug (150, "layout_names_as_group_names: %d\n",
		   config->layout_names_as_group_names);

	config->load_extra_items =
		xfconf_channel_get_bool(config->channel,
								KEY_KBD_LOAD_EXTRA_ITEMS,
								DEFAULT_KEY_KBD_LOAD_EXTRA_ITEMS);
	xkl_debug (150, "load_extra_items: %d\n",
		   config->load_extra_items);

	config->default_group =
		xfconf_channel_get_int(config->channel,
							   KEY_KBD_DEFAULT_GROUP,
							   DEFAULT_KEY_KBD_DEFAULT_GROUP);

	if (config->default_group < -1
	    || config->default_group >=
	    xkl_engine_get_max_num_groups (config->engine))
		config->default_group = -1;
	xkl_debug (150, "default_group: %d\n", config->default_group);
}

gboolean
xfcekbd_desktop_config_activate (XfcekbdDesktopConfig * config)
{
	gboolean rv = TRUE;

	xkl_engine_set_group_per_toplevel_window (config->engine,
						  config->group_per_app);
	xkl_engine_set_indicators_handling (config->engine,
					    config->handle_indicators);
	xkl_engine_set_default_group (config->engine,
				      config->default_group);

	return rv;
}

void
xfcekbd_desktop_config_lock_next_group (XfcekbdDesktopConfig * config)
{
	int group = xkl_engine_get_next_group (config->engine);
	xkl_engine_lock_group (config->engine, group);
}

/**
 * xfcekbd_desktop_config_start_listen:
 * @func: (scope notified): a function to call when settings are changed
 */
void
xfcekbd_desktop_config_start_listen (XfcekbdDesktopConfig * config,
				  GCallback func,
				  gpointer user_data)
{
	config->config_listener_id =
	    g_signal_connect (config->channel, "property-changed", func,
			      user_data);
}

void
xfcekbd_desktop_config_stop_listen (XfcekbdDesktopConfig * config)
{
	g_signal_handler_disconnect (config->channel,
				    config->config_listener_id);
	config->config_listener_id = 0;
}

gboolean
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
					     gchar *** full_group_names)
{
	gchar **sld, **lld, **svd, **lvd;
	gchar **psld, **plld, **plvd;
	gchar **psgn, **pfgn, **psvd;
	gint total_descriptions;

	if (!xfcekbd_desktop_config_get_lv_descriptions
	    (config, registry, layout_ids, variant_ids, &sld, &lld, &svd,
	     &lvd)) {
		return False;
	}

	total_descriptions = g_strv_length (sld);

	*short_group_names = psgn =
	    g_new0 (gchar *, total_descriptions + 1);
	*full_group_names = pfgn =
	    g_new0 (gchar *, total_descriptions + 1);

	plld = lld;
	psld = sld;
	plvd = lvd;
	psvd = svd;
	while (plld != NULL && *plld != NULL) {
		gchar *sd = (*psvd[0] == '\0') ? *psld : *psvd;
		psld++, psvd++;
		*psgn++ = g_strdup (sd);
		*pfgn++ = g_strdup (xfcekbd_keyboard_config_format_full_layout
				    (*plld++, *plvd++));
	}
	g_strfreev (sld);
	g_strfreev (lld);
	g_strfreev (svd);
	g_strfreev (lvd);

	return True;
}
