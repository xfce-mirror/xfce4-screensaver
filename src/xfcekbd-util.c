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

#include <xfcekbd-util.h>

#include <time.h>

#include <glib/gi18n-lib.h>

#include <libxklavier/xklavier.h>

#include <gio/gio.h>

#include <gdk/gdkx.h>

#include <xfcekbd-config-private.h>

/**
 * xfcekbd_strv_append:
 *
 * Returns: (transfer full) (array zero-terminated=1): Append string to strv array
 */
gchar **
xfcekbd_strv_append (gchar ** arr, gchar * element)
{
	gint old_length = (arr == NULL) ? 0 : g_strv_length (arr);
	gchar **new_arr = g_new0 (gchar *, old_length + 2);
	if (arr != NULL) {
		memcpy (new_arr, arr, old_length * sizeof (gchar *));
		g_free (arr);
	}
	new_arr[old_length] = element;
	return new_arr;
}

