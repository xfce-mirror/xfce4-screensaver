/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2006 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
//
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "gs-auth.h"
#include "setuid.h"

/* Initializations that potentially take place as a priveleged user:
   If the executable is setuid root, then these initializations
   are run as root, before discarding privileges.
*/
static gboolean
privileged_initialization (void) {
    gboolean ret;
    char *nolock_reason;
    char *orig_uid;
    char *uid_message;

#ifndef NO_LOCKING
    /* before hack_uid () for proper permissions */
    gs_auth_priv_init ();
#endif /* NO_LOCKING */

    ret = hack_uid (&nolock_reason,
                    &orig_uid,
                    &uid_message);
    if (nolock_reason) {
        g_warning ("Locking disabled: %s", nolock_reason);
    }
#ifndef NO_LOCKING
    if (uid_message && gs_auth_get_verbose ()) {
        g_print ("Modified UID: %s", uid_message);
    }
#endif /* NO_LOCKING */

    g_free (nolock_reason);
    g_free (orig_uid);
    g_free (uid_message);

    return ret;
}

/* Figure out what locking mechanisms are supported.
 */
static gboolean
lock_initialization (char **nolock_reason) {
    if (nolock_reason) {
        *nolock_reason = NULL;
    }

#ifdef NO_LOCKING
    if (nolock_reason) {
        *nolock_reason = g_strdup ("not compiled with locking support");
    }
    return FALSE;
#else /* !NO_LOCKING */

    /* Finish initializing locking, now that we're out of privileged code. */
    if (!gs_auth_init ()) {
        if (nolock_reason) {
            *nolock_reason = g_strdup ("error getting password");
        }
        return FALSE;
    }

    /* If locking is currently enabled, but the environment indicates that
       we have been launched as MDM's "Background" program, then disable
       locking just in case.
    */
    if (getenv ("RUNNING_UNDER_MDM")) {
        if (nolock_reason) {
            *nolock_reason = g_strdup ("running under MDM");
        }
        return FALSE;
    }

    /* If the server is XDarwin (MacOS X) then disable locking.
       (X grabs only affect X programs, so you can use Command-Tab
       to bring any other Mac program to the front, e.g., Terminal.)
    */
    {
#ifdef __APPLE__
        /* Disable locking if *running* on Apple hardware, since we have no
           reliable way to determine whether the server is running on MacOS.
           Hopefully __APPLE__ means "MacOS" and not "Linux on Mac hardware"
           but I'm not really sure about that.
        */
        if (nolock_reason) {
            *nolock_reason = g_strdup ("Cannot lock securely on MacOS X");
        }
        return FALSE;
#endif
    }

#endif /* NO_LOCKING */

    return TRUE;
}

static char *
request_password (const char *prompt) {
    char buf[255];
    char *pass;
    char *password;
    struct termios ts0;
    struct termios ts1;

    tcgetattr (fileno (stdin), &ts0);
    ts1 = ts0;
    ts1.c_lflag &= ~ECHO;

    printf ("%s", prompt);

    if (tcsetattr (fileno (stdin), TCSAFLUSH, &ts1) != 0) {
        fprintf (stderr, "Could not set terminal attributes\n");
        exit (1);
    }

    pass = fgets (buf, sizeof (buf) - 1, stdin);

    tcsetattr (fileno (stdin), TCSANOW, &ts0);

    if (!pass || !*pass) {
        exit (0);
    }

    if (pass[strlen (pass) - 1] == '\n') {
        pass[strlen (pass) - 1] = 0;
    }

    password = g_strdup (pass);

    memset (pass, '\b', strlen (pass));

    return password;
}

static gboolean
auth_message_handler (GSAuthMessageStyle style,
                      const char *msg,
                      char **response,
                      gpointer data) {
    gboolean ret;

    g_message ("Got message style %d: '%s'", style, msg);

    ret = TRUE;

    switch (style) {
        case GS_AUTH_MESSAGE_PROMPT_ECHO_ON:
            break;
        case GS_AUTH_MESSAGE_PROMPT_ECHO_OFF: {
            char *password;
            password = request_password (msg);
            *response = password;
        } break;
        case GS_AUTH_MESSAGE_ERROR_MSG:
            break;
        case GS_AUTH_MESSAGE_TEXT_INFO:
            break;
        default:
            g_assert_not_reached ();
    }

    return ret;
}

int
main (int argc,
      char **argv) {
    GError *error = NULL;
    gboolean verbose = TRUE;
    char *nolock_reason = NULL;

    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

#ifndef NO_LOCKING
    gs_auth_set_verbose (verbose);
#endif /* NO_LOCKING */
    if (!privileged_initialization ()) {
        return EXIT_FAILURE;
    }

    if (!gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
        fprintf (stderr, "%s", error->message);
        g_error_free (error);
        return EXIT_FAILURE;
    }

    if (!lock_initialization (&nolock_reason)) {
        if (nolock_reason) {
            g_warning ("Screen locking disabled: %s", nolock_reason);
            g_free (nolock_reason);
        }

        return EXIT_FAILURE;
    }

again:
    error = NULL;

#ifndef NO_LOCKING
    if (gs_auth_verify_user (g_get_user_name (), g_getenv ("DISPLAY"), auth_message_handler, NULL, &error)) {
#else
    if (TRUE) {
#endif /* NO_LOCKING */
        printf ("Correct!\n");
    } else {
        if (error != NULL) {
            fprintf (stderr, "ERROR: %s\n", error->message);
            g_error_free (error);
        }
        printf ("Incorrect\n");
        goto again;
    }

    return EXIT_SUCCESS;
}
