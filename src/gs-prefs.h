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

#ifndef SRC_GS_PREFS_H_
#define SRC_GS_PREFS_H_

#include <glib.h>

G_BEGIN_DECLS

#define GS_TYPE_PREFS         (gs_prefs_get_type ())
#define GS_PREFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_PREFS, GSPrefs))
#define GS_PREFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_PREFS, GSPrefsClass))
#define GS_IS_PREFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_PREFS))
#define GS_IS_PREFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_PREFS))
#define GS_PREFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_PREFS, GSPrefsClass))

#define SETTINGS_XFCONF_CHANNEL "xfce4-screensaver"
#define XFPM_XFCONF_CHANNEL "xfce4-power-manager"

/**
 * Enable screensaver
 * Set this to TRUE to globally enable screensavers.
 */
#define KEY_SAVER_ENABLED "/saver/enabled"
#define DEFAULT_KEY_SAVER_ENABLED TRUE

/**
 * Screensaver theme selection mode
 * The selection mode used by screensaver. May be "blank-only" to enable the screensaver
 * without using any theme on activation, "single" to enable screensaver using only one
 * theme on activation (specified in "themes" key), and "random" to enable the screensaver
 * using a random theme on activation.
 */
#define KEY_MODE "/saver/mode"
#define DEFAULT_KEY_MODE 0

/**
 * Activate when idle
 * Set this to TRUE to activate the screensaver when the session is idle.
 */
#define KEY_IDLE_ACTIVATION_ENABLED "/saver/idle-activation/enabled"
#define DEFAULT_KEY_IDLE_ACTIVATION_ENABLED TRUE

/**
 * Time before session is considered idle
 * The number of minutes of inactivity before the session is considered idle.
 */
#define KEY_IDLE_DELAY "/saver/idle-activation/delay"
#define DEFAULT_KEY_IDLE_DELAY 5

/**
 * Inhibit when an application is fullscreen
 * Set this to TRUE to inhibit the screensaver when the focused application is fullscreen.
 */
#define KEY_FULLSCREEN_INHIBIT "/saver/fullscreen-inhibit"
#define DEFAULT_KEY_FULLSCREEN_INHIBIT FALSE

/**
 * Screensaver themes
 * This key specifies the list of themes to be used by the screensaver. It's ignored
 * when "mode" key is "blank-only", should provide the theme name when "mode" is "single",
 * and should provide a list of themes when "mode" is "random".
 */
#define KEY_THEMES "/saver/themes/list"

/**
 * Time before theme change
 * The number of minutes to run before changing the screensaver theme.
 */
#define KEY_CYCLE_DELAY "/saver/themes/cycle-delay"
#define DEFAULT_KEY_CYCLE_DELAY 10

/**
 * Enable locking
 * Set this to TRUE to globally enable locking.
 */
#define KEY_LOCK_ENABLED "/lock/enabled"
#define DEFAULT_KEY_LOCK_ENABLED TRUE

/**
 * Lock on activation
 * Set this to TRUE to lock the screen when the screensaver goes active.
 */
#define KEY_LOCK_WITH_SAVER_ENABLED "/lock/saver-activation/enabled"
#define DEFAULT_KEY_LOCK_WITH_SAVER_ENABLED TRUE

/**
 * Time before locking
 * The number of minutes after screensaver activation before locking the screen.
 */
#define KEY_LOCK_WITH_SAVER_DELAY "/lock/saver-activation/delay"
#define DEFAULT_KEY_LOCK_WITH_SAVER_DELAY 0

/**
 * Lock on suspend/hibernate
 * Set this to TRUE to lock the screen when the system goes to sleep
 * Shared with Xfce Power Manager
 */
#define KEY_LOCK_ON_SLEEP "/xfce4-power-manager/lock-screen-suspend-hibernate"
#define DEFAULT_KEY_LOCK_ON_SLEEP TRUE

/**
 * Allow embedding a keyboard into the window
 * Set this to TRUE to allow embedding a keyboard into the window when trying to unlock.
 * The "keyboard_command" key must be set with the appropriate command.
 */
#define KEY_KEYBOARD_ENABLED "/lock/embedded-keyboard/enabled"
#define DEFAULT_KEY_KEYBOARD_ENABLED FALSE

/**
 * Embedded keyboard command
 * The command that will be run, if the "embedded_keyboard_enabled" key is set to TRUE,
 * to embed a keyboard widget into the window. This command should implement an XEMBED
 * plug interface and output a window XID on the standard output.
 */
#define KEY_KEYBOARD_COMMAND "/lock/embedded-keyboard/command"
#define DEFAULT_KEY_KEYBOARD_COMMAND ""

/**
 * Display embedded keyboard
 * Remembers the current user preference to display the on-screen keyboard.
 */
#define KEY_KEYBOARD_DISPLAYED "/lock/embedded-keyboard/displayed"
#define DEFAULT_KEY_KEYBOARD_DISPLAYED FALSE

/**
 * Allow the session status message to be displayed
 * Allow the session status message to be displayed when the screen is locked.
 */
#define KEY_STATUS_MESSAGE_ENABLED "/lock/status-messages/enabled"
#define DEFAULT_KEY_STATUS_MESSAGE_ENABLED TRUE

/**
 * Allow logout
 * Set this to TRUE to offer an option in the unlock dialog to allow logging out after a
 * delay. The delay is specified in the "logout_delay" key.
 */
#define KEY_LOGOUT_ENABLED "/lock/logout/enabled"
#define DEFAULT_KEY_LOGOUT_ENABLED FALSE

/**
 * Time before logout option
 * The number of minutes after the screensaver activation before a logout option will
 * appear in the unlock dialog. This key has effect only if the "logout_enable" key is
 * set to TRUE.
 */
#define KEY_LOGOUT_DELAY "/lock/logout/delay"
#define DEFAULT_KEY_LOGOUT_DELAY 120

/**
 * Logout command
 * The command to invoke when the logout button is clicked. This command should simply
 * log the user out without any interaction. This key has effect only if the
 * "logout_enable" key is set to TRUE.
 */
#define KEY_LOGOUT_COMMAND "/lock/logout/command"
#define DEFAULT_KEY_LOGOUT_COMMAND ""

/**
 * Allow user switching
 * Set this to TRUE to offer an option in the unlock dialog to switch to a different
 * user account.
 */
#define KEY_USER_SWITCH_ENABLED "/lock/user-switching/enabled"
#define DEFAULT_KEY_USER_SWITCH_ENABLED TRUE

/**
 * Blank screensaver DPMS sleep timeout (seconds)
 * This value controls the timeout after blanking the screen to suspend the display.
 * A value of 0 means that it is disabled.
 */
#define KEY_DPMS_SLEEP_AFTER "/screensavers/xfce-blank/dpms-sleep-after"
#define DEFAULT_KEY_DPMS_SLEEP_AFTER 5

/**
 * Blank screensaver DPMS power timeout (minutes)
 * This value controls the timeout after blanking the screen to power off the display.
 * A value of 0 means that it is disabled.
 */
#define KEY_DPMS_OFF_AFTER "/screensavers/xfce-blank/dpms-off-after"
#define DEFAULT_KEY_DPMS_OFF_AFTER 15

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

    guint            saver_enabled : 1; /* global saver switch */
    guint            lock_enabled : 1; /* global lock switch */

    guint            idle_activation_enabled : 1; /* whether to activate when idle */
    guint            sleep_activation_enabled : 1; /* whether to activate on suspend/hibernate */
    guint            lock_with_saver_enabled : 1;           /* whether to lock when active */
    guint            logout_enabled : 1;         /* Whether to offer the logout option */
    guint            user_switch_enabled : 1;  /* Whether to offer the user switch option */
    guint            keyboard_enabled : 1;       /* Whether to try to embed a keyboard */
    guint            keyboard_displayed : 1;       /* Whether the keyboard is displayed */
    guint            status_message_enabled : 1; /* show the status message in the lock */
    guint            timeout;        /* how much idle time before activation */
    guint            lock_timeout;   /* how many minutes after activation locking starts */
    guint            logout_timeout; /* how many seconds until the logout option appears */
    guint            cycle;          /* how many seconds each theme should run */
    guint            fullscreen_inhibit : 1; /* inhibit screensaver when an application is fullscreen */

    char            *logout_command;   /* command to use to logout */
    char            *keyboard_command; /* command to use to embed a keyboard */

    GSList          *themes;   /* the screensaver themes to run */
    GSSaverMode      mode; /* theme selection mode */

    guint            dpms_sleep_timeout; /* blank: # of minutes to wait before sleeping the display */
    guint            dpms_off_timeout; /* blank: # of minutes after sleep to power off the display */
} GSPrefs;

typedef struct
{
    GObjectClass     parent_class;

    void            (* changed)        (GSPrefs *prefs);
} GSPrefsClass;

GType       gs_prefs_get_type            (void);
GSPrefs   * gs_prefs_new                 (void);
void        gs_prefs_load                (GSPrefs     *prefs);
const char* gs_prefs_get_theme           (GSPrefs     *prefs);
gchar *     gs_prefs_get_theme_arguments (GSPrefs     *prefs,
                                          const gchar *theme);

G_END_DECLS

#endif /* SRC_GS_PREFS_H_ */
