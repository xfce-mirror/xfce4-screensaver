/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2018      Sean Davis <bluesabre@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <xfconf/xfconf.h>

#include "gs-prefs.h"

static void gs_prefs_class_init (GSPrefsClass *klass);
static void gs_prefs_init       (GSPrefs      *prefs);
static void gs_prefs_finalize   (GObject      *object);

struct GSPrefsPrivate
{
    XfconfChannel *channel;
};

enum
{
    CHANGED,
    LAST_SIGNAL
};

enum
{
    PROP_0
};

static guint         signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GSPrefs, gs_prefs, G_TYPE_OBJECT)

static void
gs_prefs_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gs_prefs_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gs_prefs_class_init (GSPrefsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize     = gs_prefs_finalize;
    object_class->get_property = gs_prefs_get_property;
    object_class->set_property = gs_prefs_set_property;


    signals [CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSPrefsClass, changed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
}

static void
_gs_prefs_set_timeout (GSPrefs *prefs,
                       int      value)
{
    if (value < 1)
        value = 10;

    /* pick a reasonable large number for the
       upper bound */
    if (value > 480)
        value = 480;

    prefs->timeout = value * 60000;
}

static void
_gs_prefs_set_power_timeout (GSPrefs *prefs,
                             int      value)
{
    if (value < 1)
        value = 60;

    /* pick a reasonable large number for the
       upper bound */
    if (value > 480)
        value = 480;

    /* this value is in seconds - others are in minutes */
    prefs->power_timeout = value * 1000;
}

static void
_gs_prefs_set_lock_timeout (GSPrefs *prefs,
                            int      value)
{
    if (value < 0)
        value = 0;

    /* pick a reasonable large number for the
       upper bound */
    if (value > 480)
        value = 480;

    prefs->lock_timeout = value * 60000;
}

static void
_gs_prefs_set_cycle_timeout (GSPrefs *prefs,
                             int      value)
{
    if (value < 1)
        value = 1;

    /* pick a reasonable large number for the
       upper bound */
    if (value > 480)
        value = 480;

    prefs->cycle = value * 60000;
}

static void
_gs_prefs_set_mode (GSPrefs    *prefs,
                    gint        mode)
{
    prefs->mode = mode;
}

static void
_gs_prefs_set_themes (GSPrefs  *prefs,
                      gchar   **values)
{
    guint i;
    if (prefs->themes)
    {
        g_slist_foreach (prefs->themes, (GFunc)g_free, NULL);
        g_slist_free (prefs->themes);
    }

    if (values == NULL)
        return;

    /* take ownership of the list */
    prefs->themes = NULL;
        for (i=0; values[i] != NULL; i++)
        prefs->themes = g_slist_append (prefs->themes, g_strdup (values[i]));
}

static void
_gs_prefs_set_idle_activation_enabled (GSPrefs  *prefs,
                                       gboolean  value)
{
    prefs->idle_activation_enabled = value;
}

static void
_gs_prefs_set_lock_enabled (GSPrefs  *prefs,
                            gboolean  value)
{
    prefs->lock_enabled = value;
}

static void
_gs_prefs_set_keyboard_enabled (GSPrefs  *prefs,
                                gboolean  value)
{
    prefs->keyboard_enabled = value;
}

static void
_gs_prefs_set_keyboard_command (GSPrefs    *prefs,
                                const char *value)
{
    g_free (prefs->keyboard_command);
    prefs->keyboard_command = NULL;

    if (value)
    {
        /* FIXME: check command */

        prefs->keyboard_command = g_strdup (value);
    }
}

static void
_gs_prefs_set_status_message_enabled (GSPrefs  *prefs,
                                      gboolean  enabled)
{
    prefs->status_message_enabled = enabled;
}

static void
_gs_prefs_set_logout_enabled (GSPrefs  *prefs,
                              gboolean  value)
{
    prefs->logout_enabled = value;
}

static void
_gs_prefs_set_logout_command (GSPrefs    *prefs,
                              const char *value)
{
    g_free (prefs->logout_command);
    prefs->logout_command = NULL;

    if (value)
    {
        /* FIXME: check command */

        prefs->logout_command = g_strdup (value);
    }
}

static void
_gs_prefs_set_logout_timeout (GSPrefs *prefs,
                              int      value)
{
    if (value < 0)
        value = 0;

    /* pick a reasonable large number for the
       upper bound */
    if (value > 480)
        value = 480;

    prefs->logout_timeout = value * 60000;
}

static void
_gs_prefs_set_user_switch_enabled (GSPrefs  *prefs,
                                   gboolean  value)
{
    prefs->user_switch_enabled = value;
}

static void
gs_prefs_load_from_settings (GSPrefs *prefs)
{
    glong      value;
    gboolean   bvalue;
    char      *string;
    gchar    **strv;
    gint       mode;

    bvalue = xfconf_channel_get_bool (prefs->priv->channel, KEY_IDLE_ACTIVATION_ENABLED, DEFAULT_KEY_IDLE_ACTIVATION_ENABLED);
    _gs_prefs_set_idle_activation_enabled (prefs, bvalue);

    bvalue = xfconf_channel_get_bool (prefs->priv->channel, KEY_LOCK_ENABLED, DEFAULT_KEY_LOCK_ENABLED);
    _gs_prefs_set_lock_enabled (prefs, bvalue);

    value = xfconf_channel_get_int (prefs->priv->channel, KEY_IDLE_DELAY, DEFAULT_KEY_IDLE_DELAY);
    _gs_prefs_set_timeout (prefs, value);

    value = xfconf_channel_get_int (prefs->priv->channel, KEY_POWER_DELAY, DEFAULT_KEY_POWER_DELAY);
    _gs_prefs_set_power_timeout (prefs, value);

    value = xfconf_channel_get_int (prefs->priv->channel, KEY_LOCK_DELAY, DEFAULT_KEY_LOCK_DELAY);
    _gs_prefs_set_lock_timeout (prefs, value);

    value = xfconf_channel_get_int (prefs->priv->channel, KEY_CYCLE_DELAY, DEFAULT_KEY_CYCLE_DELAY);
    _gs_prefs_set_cycle_timeout (prefs, value);

    mode = xfconf_channel_get_int (prefs->priv->channel, KEY_MODE, DEFAULT_KEY_MODE);
    _gs_prefs_set_mode (prefs, mode);

    strv = xfconf_channel_get_string_list (prefs->priv->channel, KEY_THEMES);
    _gs_prefs_set_themes (prefs, strv);
    g_strfreev (strv);

    /* Embedded keyboard options */

    bvalue = xfconf_channel_get_bool (prefs->priv->channel, KEY_KEYBOARD_ENABLED, DEFAULT_KEY_KEYBOARD_ENABLED);
    _gs_prefs_set_keyboard_enabled (prefs, bvalue);

    string = xfconf_channel_get_string (prefs->priv->channel, KEY_KEYBOARD_COMMAND, DEFAULT_KEY_KEYBOARD_COMMAND);
    _gs_prefs_set_keyboard_command (prefs, string);
    g_free (string);

    bvalue = xfconf_channel_get_bool (prefs->priv->channel, KEY_STATUS_MESSAGE_ENABLED, DEFAULT_KEY_STATUS_MESSAGE_ENABLED);
    _gs_prefs_set_status_message_enabled (prefs, bvalue);

    /* Logout options */

    bvalue = xfconf_channel_get_bool (prefs->priv->channel, KEY_LOGOUT_ENABLED, DEFAULT_KEY_LOGOUT_ENABLED);
    _gs_prefs_set_logout_enabled (prefs, bvalue);

    string = xfconf_channel_get_string (prefs->priv->channel, KEY_LOGOUT_COMMAND, DEFAULT_KEY_LOGOUT_COMMAND);
    _gs_prefs_set_logout_command (prefs, string);
    g_free (string);

    value = xfconf_channel_get_int (prefs->priv->channel, KEY_LOGOUT_DELAY, DEFAULT_KEY_LOGOUT_DELAY);
    _gs_prefs_set_logout_timeout (prefs, value);

    /* User switching options */

    bvalue = xfconf_channel_get_bool (prefs->priv->channel, KEY_USER_SWITCH_ENABLED, DEFAULT_KEY_USER_SWITCH_ENABLED);
    _gs_prefs_set_user_switch_enabled (prefs, bvalue);
}

static void
key_changed_cb (XfconfChannel *channel,
                gchar         *property,
                GValue        *value,
                GSPrefs       *prefs)
{
    g_warning("key changed!");
    if (strcmp (property, KEY_MODE) == 0)
    {
        gint mode;

        mode = xfconf_channel_get_int (channel, property, DEFAULT_KEY_MODE);
        _gs_prefs_set_mode (prefs, mode);
    }
    else if (strcmp (property, KEY_THEMES) == 0)
    {
        gchar **strv = NULL;

        strv = xfconf_channel_get_string_list (channel, property);
        _gs_prefs_set_themes (prefs, strv);
        g_strfreev (strv);
    }
    else if (strcmp (property, KEY_IDLE_DELAY) == 0)
    {
        int delay;

        delay = xfconf_channel_get_int (channel, property, DEFAULT_KEY_IDLE_DELAY);
        _gs_prefs_set_timeout (prefs, delay);
    }
    else if (strcmp (property, KEY_POWER_DELAY) == 0)
    {
        int delay;

        delay = xfconf_channel_get_int (channel, property, DEFAULT_KEY_POWER_DELAY);
        _gs_prefs_set_power_timeout (prefs, delay);
    }
    else if (strcmp (property, KEY_LOCK_DELAY) == 0)
    {
        int delay;

        delay = xfconf_channel_get_int (channel, property, DEFAULT_KEY_LOCK_DELAY);
        _gs_prefs_set_lock_timeout (prefs, delay);
    }
    else if (strcmp (property, KEY_IDLE_ACTIVATION_ENABLED) == 0)
    {
        gboolean enabled;

        enabled = xfconf_channel_get_bool (channel, property, DEFAULT_KEY_IDLE_ACTIVATION_ENABLED);
        _gs_prefs_set_idle_activation_enabled (prefs, enabled);
    }
    else if (strcmp (property, KEY_LOCK_ENABLED) == 0)
    {
        gboolean enabled;

        enabled = xfconf_channel_get_bool (channel, property, DEFAULT_KEY_LOCK_ENABLED);
        _gs_prefs_set_lock_enabled (prefs, enabled);
    }
    else if (strcmp (property, KEY_CYCLE_DELAY) == 0)
    {
        int delay;

        delay = xfconf_channel_get_int (channel, property, DEFAULT_KEY_CYCLE_DELAY);
        _gs_prefs_set_cycle_timeout (prefs, delay);
    }
    else if (strcmp (property, KEY_KEYBOARD_ENABLED) == 0)
    {
        gboolean enabled;

        enabled = xfconf_channel_get_bool (channel, property, DEFAULT_KEY_KEYBOARD_ENABLED);
        _gs_prefs_set_keyboard_enabled (prefs, enabled);
    }
    else if (strcmp (property, KEY_KEYBOARD_COMMAND) == 0)
    {
        char *command;

        command = xfconf_channel_get_string (channel, property, DEFAULT_KEY_KEYBOARD_COMMAND);
        _gs_prefs_set_keyboard_command (prefs, command);
        g_free (command);
    }
    else if (strcmp (property, KEY_STATUS_MESSAGE_ENABLED) == 0)
    {
        gboolean enabled;

        enabled = xfconf_channel_get_bool (channel, property, DEFAULT_KEY_STATUS_MESSAGE_ENABLED);
        _gs_prefs_set_status_message_enabled (prefs, enabled);
    }
    else if (strcmp (property, KEY_LOGOUT_ENABLED) == 0)
    {
        gboolean enabled;

        enabled = xfconf_channel_get_bool (channel, property, DEFAULT_KEY_LOGOUT_ENABLED);
        _gs_prefs_set_logout_enabled (prefs, enabled);
    }
    else if (strcmp (property, KEY_LOGOUT_DELAY) == 0)
    {
        int delay;

        delay = xfconf_channel_get_int (channel, property, DEFAULT_KEY_LOGOUT_DELAY);
        _gs_prefs_set_logout_timeout (prefs, delay);
    }
    else if (strcmp (property, KEY_LOGOUT_COMMAND) == 0)
    {
        char *command;
        command = xfconf_channel_get_string (channel, property, DEFAULT_KEY_LOGOUT_COMMAND);
        _gs_prefs_set_logout_command (prefs, command);
        g_free (command);
    }
    else if (strcmp (property, KEY_USER_SWITCH_ENABLED) == 0)
    {
        gboolean enabled;

        enabled = xfconf_channel_get_bool (channel, property, DEFAULT_KEY_USER_SWITCH_ENABLED);
        _gs_prefs_set_user_switch_enabled (prefs, enabled);

    }
    else
    {
        g_warning ("Config key not handled: %s", property);
    }

    g_signal_emit (prefs, signals [CHANGED], 0);
}

static void
gs_prefs_init (GSPrefs *prefs)
{
    prefs->priv = gs_prefs_get_instance_private (prefs);

    prefs->priv->channel = xfconf_channel_get (SETTINGS_XFCONF_CHANNEL);
    g_signal_connect (prefs->priv->channel,
                      "property-changed",
                      G_CALLBACK (key_changed_cb),
                      prefs);

    prefs->idle_activation_enabled = TRUE;
    prefs->lock_enabled            = TRUE;
    prefs->logout_enabled          = FALSE;
    prefs->user_switch_enabled     = FALSE;

    prefs->timeout                 = 600000;
    prefs->power_timeout           = 60000;
    prefs->lock_timeout            = 0;
    prefs->logout_timeout          = 14400000;
    prefs->cycle                   = 600000;

    prefs->mode                    = GS_MODE_SINGLE;

    gs_prefs_load_from_settings (prefs);
}

static void
gs_prefs_finalize (GObject *object)
{
    GSPrefs *prefs;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_PREFS (object));

    prefs = GS_PREFS (object);

    g_return_if_fail (prefs->priv != NULL);

    if (prefs->priv->channel)
    {
        g_object_unref (prefs->priv->channel);
        prefs->priv->channel = NULL;
    }

    if (prefs->themes)
    {
        g_slist_foreach (prefs->themes, (GFunc)g_free, NULL);
        g_slist_free (prefs->themes);
    }

    g_free (prefs->logout_command);
    g_free (prefs->keyboard_command);

    G_OBJECT_CLASS (gs_prefs_parent_class)->finalize (object);
}

GSPrefs *
gs_prefs_new (void)
{
    GObject *prefs;

    prefs = g_object_new (GS_TYPE_PREFS, NULL);

    return GS_PREFS (prefs);
}
