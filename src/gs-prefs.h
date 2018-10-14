/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GS_PREFS_H
#define __GS_PREFS_H

#include <glib.h>

G_BEGIN_DECLS

#define GS_TYPE_PREFS         (gs_prefs_get_type ())
#define GS_PREFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_PREFS, GSPrefs))
#define GS_PREFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_PREFS, GSPrefsClass))
#define GS_IS_PREFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_PREFS))
#define GS_IS_PREFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_PREFS))
#define GS_PREFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_PREFS, GSPrefsClass))

#define SETTINGS_XFCONF_CHANNEL "xfce4-screensaver"

/**
 * Screensaver theme selection mode
 * The selection mode used by screensaver. May be "blank-only" to enable the screensaver
 * without using any theme on activation, "single" to enable screensaver using only one
 * theme on activation (specified in "themes" key), and "random" to enable the screensaver
 * using a random theme on activation.
 */
#define KEY_MODE "/mode"
#define DEFAULT_KEY_MODE 0

/**
 * Time before session is considered idle
 * The number of minutes of inactivity before the session is considered idle.
 */
#define KEY_IDLE_DELAY "/idle-delay"
#define DEFAULT_KEY_IDLE_DELAY 5

/**
 * Time before power management baseline
 * The number of seconds of inactivity before signalling to power management.
 * This key is set and maintained by the session power management agent.
 */
#define KEY_POWER_DELAY "/power-management-delay"
#define DEFAULT_KEY_POWER_DELAY 30

/**
 * Time before locking
 * The number of minutes after screensaver activation before locking the screen.
 */
#define KEY_LOCK_DELAY "/lock-delay"
#define DEFAULT_KEY_LOCK_DELAY 0

/**
 * Activate when idle
 * Set this to TRUE to activate the screensaver when the session is idle.
 */
#define KEY_IDLE_ACTIVATION_ENABLED "/idle-activation-enabled"
#define DEFAULT_KEY_IDLE_ACTIVATION_ENABLED TRUE

/**
 * Lock on activation
 * Set this to TRUE to lock the screen when the screensaver goes active.
 */
#define KEY_LOCK_ENABLED "/lock-enabled"
#define DEFAULT_KEY_LOCK_ENABLED TRUE

/**
 * Time before theme change
 * The number of minutes to run before changing the screensaver theme.
 */
#define KEY_CYCLE_DELAY "/cycle-delay"
#define DEFAULT_KEY_CYCLE_DELAY 10

/**
 * Allow embedding a keyboard into the window
 * Set this to TRUE to allow embedding a keyboard into the window when trying to unlock.
 * The "keyboard_command" key must be set with the appropriate command.
 */
#define KEY_KEYBOARD_ENABLED "/embedded-keyboard-enabled"
#define DEFAULT_KEY_KEYBOARD_ENABLED FALSE

/**
 * Embedded keyboard command
 * The command that will be run, if the "embedded_keyboard_enabled" key is set to TRUE,
 * to embed a keyboard widget into the window. This command should implement an XEMBED
 * plug interface and output a window XID on the standard output.
 */
#define KEY_KEYBOARD_COMMAND "/embedded-keyboard-command"
#define DEFAULT_KEY_KEYBOARD_COMMAND ""

/**
 * Allow the session status message to be displayed
 * Allow the session status message to be displayed when the screen is locked.
 */
#define KEY_STATUS_MESSAGE_ENABLED "/status-message-enabled"
#define DEFAULT_KEY_STATUS_MESSAGE_ENABLED TRUE

/**
 * Allow logout
 * Set this to TRUE to offer an option in the unlock dialog to allow logging out after a
 * delay. The delay is specified in the "logout_delay" key.
 */
#define KEY_LOGOUT_ENABLED "/logout-enabled"
#define DEFAULT_KEY_LOGOUT_ENABLED FALSE

/**
 * Time before logout option
 * The number of minutes after the screensaver activation before a logout option will
 * appear in the unlock dialog. This key has effect only if the "logout_enable" key is
 * set to TRUE.
 */
#define KEY_LOGOUT_DELAY "/logout-delay"
#define DEFAULT_KEY_LOGOUT_DELAY 120

/**
 * Logout command
 * The command to invoke when the logout button is clicked. This command should simply
 * log the user out without any interaction. This key has effect only if the
 * "logout_enable" key is set to TRUE.
 */
#define KEY_LOGOUT_COMMAND "/logout-command"
#define DEFAULT_KEY_LOGOUT_COMMAND ""

/**
 * Allow user switching
 * Set this to TRUE to offer an option in the unlock dialog to switch to a different
 * user account.
 */
#define KEY_USER_SWITCH_ENABLED "/user-switch-enabled"
#define DEFAULT_KEY_USER_SWITCH_ENABLED TRUE

/**
 * Screensaver themes
 * This key specifies the list of themes to be used by the screensaver. It's ignored
 * when "mode" key is "blank-only", should provide the theme name when "mode" is "single",
 * and should provide a list of themes when "mode" is "random".
 */
#define KEY_THEMES "/themes"

typedef enum
{
    GS_MODE_BLANK_ONLY,
    GS_MODE_RANDOM,
    GS_MODE_SINGLE
} GSSaverMode;

typedef struct GSPrefsPrivate GSPrefsPrivate;

typedef struct
{
    GObject          parent;

    GSPrefsPrivate  *priv;

    guint            idle_activation_enabled : 1; /* whether to activate when idle */
    guint            lock_enabled : 1;           /* whether to lock when active */
    guint            logout_enabled : 1;         /* Whether to offer the logout option */
    guint            user_switch_enabled : 1;  /* Whether to offer the user switch option */
    guint            keyboard_enabled : 1;       /* Whether to try to embed a keyboard */
    guint            status_message_enabled : 1; /* show the status message in the lock */
    guint            power_timeout;  /* how much idle time before power management */
    guint            timeout;        /* how much idle time before activation */
    guint            lock_timeout;   /* how long after activation locking starts */
    guint            logout_timeout; /* how long until the logout option appears */
    guint            cycle;          /* how long each theme should run */

    char            *logout_command;   /* command to use to logout */
    char            *keyboard_command; /* command to use to embed a keyboard */

    GSList          *themes;   /* the screensaver themes to run */
    GSSaverMode      mode; /* theme selection mode */
} GSPrefs;

typedef struct
{
    GObjectClass     parent_class;

    void            (* changed)        (GSPrefs *prefs);
} GSPrefsClass;

GType       gs_prefs_get_type        (void);
GSPrefs   * gs_prefs_new             (void);
void        gs_prefs_load            (GSPrefs *prefs);

G_END_DECLS

#endif /* __GS_PREFS_H */
