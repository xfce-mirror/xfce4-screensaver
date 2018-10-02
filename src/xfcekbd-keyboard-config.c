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

#include <glib/gi18n-lib.h>

#include "xfcekbd-keyboard-config.h"
#include "xfcekbd-config-private.h"
#include "xfcekbd-util.h"

/*
 * XfcekbdKeyboardConfig
 */
#define XFCEKBD_KEYBOARD_CONFIG_SCHEMA XFCEKBD_CONFIG_SCHEMA ".kbd"

#define GROUP_SWITCHERS_GROUP "grp"
#define DEFAULT_GROUP_SWITCH "grp:shift_caps_toggle"

const gchar XFCEKBD_KEYBOARD_CONFIG_KEY_MODEL[] = "model";
const gchar XFCEKBD_KEYBOARD_CONFIG_KEY_LAYOUTS[] = "layouts";
const gchar XFCEKBD_KEYBOARD_CONFIG_KEY_OPTIONS[] = "options";

const gchar *XFCEKBD_KEYBOARD_CONFIG_ACTIVE[] = {
	XFCEKBD_KEYBOARD_CONFIG_KEY_MODEL,
	XFCEKBD_KEYBOARD_CONFIG_KEY_LAYOUTS,
	XFCEKBD_KEYBOARD_CONFIG_KEY_OPTIONS
};

/*
 * static common functions
 */

gboolean
xfcekbd_keyboard_config_get_lv_descriptions (XklConfigRegistry *
					  config_registry,
					  const gchar * layout_name,
					  const gchar * variant_name,
					  gchar ** layout_short_descr,
					  gchar ** layout_descr,
					  gchar ** variant_short_descr,
					  gchar ** variant_descr)
{
	/* TODO make it not static */
	static XklConfigItem *litem = NULL;
	static XklConfigItem *vitem = NULL;

	if (litem == NULL)
		litem = xkl_config_item_new ();
	if (vitem == NULL)
		vitem = xkl_config_item_new ();

	layout_name = g_strdup (layout_name);

	g_snprintf (litem->name, sizeof litem->name, "%s", layout_name);
	if (xkl_config_registry_find_layout (config_registry, litem)) {
		*layout_short_descr = litem->short_description;
		*layout_descr = litem->description;
	} else
		*layout_short_descr = *layout_descr = NULL;

	if (variant_name != NULL) {
		variant_name = g_strdup (variant_name);
		g_snprintf (vitem->name, sizeof vitem->name, "%s",
			    variant_name);
		if (xkl_config_registry_find_variant
		    (config_registry, layout_name, vitem)) {
			*variant_short_descr = vitem->short_description;
			*variant_descr = vitem->description;
		} else
			*variant_short_descr = *variant_descr = NULL;

		g_free ((char *) variant_name);
	} else
		*variant_descr = NULL;

	g_free ((char *) layout_name);
	return *layout_descr != NULL;
}

/*
 * extern common functions
 */
static const gchar *
xfcekbd_keyboard_config_merge_items (const gchar * parent,
				  const gchar * child)
{
	static gchar buffer[XKL_MAX_CI_NAME_LENGTH * 2 - 1];
	*buffer = '\0';
	if (parent != NULL) {
		if (strlen (parent) >= XKL_MAX_CI_NAME_LENGTH)
			return NULL;
		strcat (buffer, parent);
	}
	if (child != NULL && *child != 0) {
		if (strlen (child) >= XKL_MAX_CI_NAME_LENGTH)
			return NULL;
		strcat (buffer, "\t");
		strcat (buffer, child);
	}
	return buffer;
}

gboolean
xfcekbd_keyboard_config_split_items (const gchar * merged, gchar ** parent,
				  gchar ** child)
{
	static gchar pbuffer[XKL_MAX_CI_NAME_LENGTH];
	static gchar cbuffer[XKL_MAX_CI_NAME_LENGTH];
	int plen, clen;
	const gchar *pos;
	*parent = *child = NULL;

	if (merged == NULL)
		return FALSE;

	pos = strchr (merged, '\t');
	if (pos == NULL) {
		plen = strlen (merged);
		clen = 0;
	} else {
		plen = pos - merged;
		clen = strlen (pos + 1);
		if (clen >= XKL_MAX_CI_NAME_LENGTH)
			return FALSE;
		strcpy (*child = cbuffer, pos + 1);
	}
	if (plen >= XKL_MAX_CI_NAME_LENGTH)
		return FALSE;
	memcpy (*parent = pbuffer, merged, plen);
	pbuffer[plen] = '\0';
	return TRUE;
}

/*
 * static XfcekbdKeyboardConfig functions
 */
static void
xfcekbd_keyboard_config_copy_from_xkl_config (XfcekbdKeyboardConfig * kbd_config,
					   XklConfigRec * pdata)
{
	char **p, **p1;
	int i;
	xfcekbd_keyboard_config_model_set (kbd_config, pdata->model);
	xkl_debug (150, "Loaded Kbd model: [%s]\n", pdata->model);

	/* Layouts */
	g_strfreev (kbd_config->layouts_variants);
	kbd_config->layouts_variants = NULL;
	if (pdata->layouts != NULL) {
		p = pdata->layouts;
		p1 = pdata->variants;
		kbd_config->layouts_variants =
		    g_new0 (gchar *, g_strv_length (pdata->layouts) + 1);
		i = 0;
		while (*p != NULL) {
			const gchar *full_layout =
			    xfcekbd_keyboard_config_merge_items (*p, *p1);
			xkl_debug (150,
				   "Loaded Kbd layout (with variant): [%s]\n",
				   full_layout);
			kbd_config->layouts_variants[i++] =
			    g_strdup (full_layout);
			p++;
			p1++;
		}
	}

	/* Options */
	g_strfreev (kbd_config->options);
	kbd_config->options = NULL;

	if (pdata->options != NULL) {
		p = pdata->options;
		kbd_config->options =
		    g_new0 (gchar *, g_strv_length (pdata->options) + 1);
		i = 0;
		while (*p != NULL) {
			char group[XKL_MAX_CI_NAME_LENGTH];
			char *option = *p;
			char *delim =
			    (option != NULL) ? strchr (option, ':') : NULL;
			int len;
			if ((delim != NULL) &&
			    ((len =
			      (delim - option)) <
			     XKL_MAX_CI_NAME_LENGTH)) {
				strncpy (group, option, len);
				group[len] = 0;
				xkl_debug (150,
					   "Loaded Kbd option: [%s][%s]\n",
					   group, option);
				xfcekbd_keyboard_config_options_set
				    (kbd_config, i++, group, option);
			}
			p++;
		}
	}
}

/*
 * extern XfcekbdKeyboardConfig config functions
 */
void
xfcekbd_keyboard_config_init (XfcekbdKeyboardConfig * kbd_config,
			      XklEngine * engine)
{
	memset (kbd_config, 0, sizeof (*kbd_config));
	kbd_config->settings = g_settings_new (XFCEKBD_KEYBOARD_CONFIG_SCHEMA);
	kbd_config->engine = engine;
}

void
xfcekbd_keyboard_config_term (XfcekbdKeyboardConfig * kbd_config)
{
	xfcekbd_keyboard_config_model_set (kbd_config, NULL);

	g_strfreev (kbd_config->layouts_variants);
	kbd_config->layouts_variants = NULL;
	g_strfreev (kbd_config->options);
	kbd_config->options = NULL;

	g_object_unref (kbd_config->settings);
	kbd_config->settings = NULL;
}

void
xfcekbd_keyboard_config_load_from_x_current (XfcekbdKeyboardConfig * kbd_config,
					  XklConfigRec * data)
{
	gboolean own_data = data == NULL;
	xkl_debug (150, "Copying config from X(current)\n");
	if (own_data)
		data = xkl_config_rec_new ();
	if (xkl_config_rec_get_from_server (data, kbd_config->engine))
		xfcekbd_keyboard_config_copy_from_xkl_config (kbd_config,
							   data);
	else
		xkl_debug (150,
			   "Could not load keyboard config from server: [%s]\n",
			   xkl_get_last_error ());
	if (own_data)
		g_object_unref (G_OBJECT (data));
}

void
xfcekbd_keyboard_config_model_set (XfcekbdKeyboardConfig * kbd_config,
				const gchar * model_name)
{
	if (kbd_config->model != NULL)
		g_free (kbd_config->model);
	kbd_config->model =
	    (model_name == NULL
	     || model_name[0] == '\0') ? NULL : g_strdup (model_name);
}

void
xfcekbd_keyboard_config_options_set (XfcekbdKeyboardConfig * kbd_config,
				  gint idx,
				  const gchar * group_name,
				  const gchar * option_name)
{
	const gchar *merged;
	if (group_name == NULL || option_name == NULL)
		return;
	merged =
	    xfcekbd_keyboard_config_merge_items (group_name, option_name);
	if (merged == NULL)
		return;
	kbd_config->options[idx] = g_strdup (merged);
}

gboolean
xfcekbd_keyboard_config_options_is_set (XfcekbdKeyboardConfig * kbd_config,
				     const gchar * group_name,
				     const gchar * option_name)
{
	gchar **p = kbd_config->options;
	const gchar *merged =
	    xfcekbd_keyboard_config_merge_items (group_name, option_name);
	if (merged == NULL)
		return FALSE;

	while (p && *p) {
		if (!g_ascii_strcasecmp (merged, *p++))
			return TRUE;
	}
	return FALSE;
}

const gchar *
xfcekbd_keyboard_config_format_full_layout (const gchar * layout_descr,
					 const gchar * variant_descr)
{
	static gchar full_descr[XKL_MAX_CI_DESC_LENGTH * 2];
	if (variant_descr == NULL || variant_descr[0] == 0)
		g_snprintf (full_descr, sizeof (full_descr), "%s",
			    layout_descr);
	else
		g_snprintf (full_descr, sizeof (full_descr), "%s %s",
			    layout_descr, variant_descr);
	return full_descr;
}
