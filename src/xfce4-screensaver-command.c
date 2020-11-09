/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
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

#include <config.h>

#include <locale.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>

#define GS_SERVICE   "org.xfce.ScreenSaver"
#define GS_PATH      "/org/xfce/ScreenSaver"
#define GS_INTERFACE "org.xfce.ScreenSaver"

static gboolean do_quit             = FALSE;
static gboolean do_lock             = FALSE;
static gboolean do_cycle            = FALSE;
static gboolean do_activate         = FALSE;
static gboolean do_deactivate       = FALSE;
static gboolean do_version          = FALSE;
static gboolean do_poke             = FALSE;
static gboolean do_inhibit          = FALSE;

static gboolean do_query            = FALSE;
static gboolean do_time             = FALSE;

static char    *inhibit_reason      = NULL;
static char    *inhibit_application = NULL;

static GOptionEntry entries[] = {
    {
        "exit", 0, 0, G_OPTION_ARG_NONE, &do_quit,
        N_("Causes the screensaver to exit gracefully"), NULL
    },
    {
        "query", 'q', 0, G_OPTION_ARG_NONE, &do_query,
        N_("Query the state of the screensaver"), NULL
    },
    {
        "time", 't', 0, G_OPTION_ARG_NONE, &do_time,
        N_("Query the length of time the screensaver has been active"), NULL
    },
    {
        "lock", 'l', 0, G_OPTION_ARG_NONE, &do_lock,
        N_("Tells the running screensaver process to lock the screen immediately"), NULL
    },
    {
        "cycle", 'c', 0, G_OPTION_ARG_NONE, &do_cycle,
        N_("If the screensaver is active then switch to another graphics demo"), NULL
    },
    {
        "activate", 'a', 0, G_OPTION_ARG_NONE, &do_activate,
        N_("Turn the screensaver on (blank the screen)"), NULL
    },
    {
        "deactivate", 'd', 0, G_OPTION_ARG_NONE, &do_deactivate,
        N_("If the screensaver is active then deactivate it (un-blank the screen)"), NULL
    },
    {
        "poke", 'p', 0, G_OPTION_ARG_NONE, &do_poke,
        N_("Poke the running screensaver to simulate user activity"), NULL
    },
    {
        "inhibit", 'i', 0, G_OPTION_ARG_NONE, &do_inhibit,
        N_("Inhibit the screensaver from activating.  Command blocks while inhibit is active."), NULL
    },
    {
        "application-name", 'n', 0, G_OPTION_ARG_STRING, &inhibit_application,
        N_("The calling application that is inhibiting the screensaver"), NULL
    },
    {
        "reason", 'r', 0, G_OPTION_ARG_STRING, &inhibit_reason,
        N_("The reason for inhibiting the screensaver"), NULL
    },
    {
        "version", 'V', 0, G_OPTION_ARG_NONE, &do_version,
        N_("Version of this application"), NULL
    },
    { NULL }
};

static GMainLoop *loop = NULL;
int return_code = 0;

static GDBusMessage *
screensaver_send_message (GDBusConnection *conn,
                          const char      *name,
                          GVariant        *body,
                          gboolean         expect_reply) {
    GDBusMessage *message;
    GDBusMessage *reply = NULL;
    GError       *error = NULL;

    g_return_val_if_fail (conn != NULL, NULL);
    g_return_val_if_fail (name != NULL, NULL);

    message = g_dbus_message_new_method_call (GS_SERVICE, GS_PATH,  GS_INTERFACE, name);
    if (message == NULL) {
        g_warning ("Couldn't allocate the dbus message");
        return NULL;
    }

    if (body)
        g_dbus_message_set_body (message, body);

    if (!expect_reply) {
        g_dbus_connection_send_message (conn, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                        NULL, &error);
    } else {
        reply = g_dbus_connection_send_message_with_reply_sync (conn, message,
                                                                G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                                                -1, NULL, NULL, &error);
    }
    if (error != NULL) {
        g_warning ("unable to send message: %s", error->message);
        g_clear_error (&error);
    }

    g_dbus_connection_flush_sync (conn, NULL, &error);
    if (error != NULL) {
        g_warning ("unable to flush message queue: %s", error->message);
        g_clear_error (&error);
    }
    g_object_unref (message);
    return reply;
}

static gboolean
screensaver_is_running (GDBusConnection *connection)
{
        GVariant *reply;
        gboolean exists = FALSE;

        g_return_val_if_fail (connection != NULL, FALSE);

        reply = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.DBus",
                                             "/org/freedesktop/DBus",
                                             "org.freedesktop.DBus",
                                             "GetNameOwner",
                                             g_variant_new ("(s)", GS_SERVICE),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                             -1,
                                             NULL,
                                             NULL);
        if (reply != NULL) {
                exists = TRUE;
                g_variant_unref (reply);
        }

        return exists;
}

static gboolean
do_command (gpointer user_data) {
    GDBusConnection *conn = user_data;
    GDBusMessage *reply;

    if (!screensaver_is_running (conn)) {
        g_message ("Screensaver is not running! Start xfce4-screensaver first");
        return_code = 1;
        goto done;
    }

    if (do_quit) {
        screensaver_send_message (conn, "Quit", NULL, FALSE);
        goto done;
    }

    if (do_query) {
        GVariant     *body;
        GVariantIter *iter;
        gboolean      is_active;
        gchar        *str;

        reply = screensaver_send_message (conn, "GetActive", NULL, TRUE);
        if (!reply) {
            g_message ("Did not receive a reply from the screensaver.");
            goto done;
        }

        body = g_dbus_message_get_body (reply);
        g_variant_get (body, "(b)", &is_active);
        g_object_unref (reply);
        g_print (_("The screensaver is %s\n"), is_active ? _("active") : _("inactive"));

        reply = screensaver_send_message (conn, "GetInhibitors", NULL, TRUE);
        if (!reply) {
            g_message ("Did not receive a reply from screensaver.");
            goto done;
        }
        body = g_dbus_message_get_body (reply);
        g_variant_get (body, "(as)", &iter);

        if (g_variant_iter_n_children(iter) <= 0) {
            g_print (_("The screensaver is not inhibited\n"));
        } else {
            g_print (_("The screensaver is being inhibited by:\n"));
            while (g_variant_iter_loop (iter, "s", &str)) {
                g_print ("\t%s\n", str);
            }
        }
        g_variant_iter_free (iter);
        g_object_unref (reply);
    }

    if (do_time) {
        GVariant *body;
        guint32   t;

        reply = screensaver_send_message (conn, "GetActiveTime", NULL, TRUE);
        body = g_dbus_message_get_body (reply);
        g_variant_get (body, "(u)", &t);
        g_print (_("The screensaver has been active for %d seconds.\n"), t);
        g_object_unref (reply);
    }

    if (do_lock) {
        screensaver_send_message (conn, "Lock", NULL, FALSE);
    }

    if (do_cycle) {
        screensaver_send_message (conn, "Cycle", NULL, FALSE);
    }

    if (do_poke) {
        screensaver_send_message (conn, "SimulateUserActivity", NULL, FALSE);
    }

    if (do_activate) {
        screensaver_send_message (conn, "SetActive", g_variant_new ("(b)", TRUE), FALSE);
    }

    if (do_deactivate) {
        screensaver_send_message (conn, "SetActive", g_variant_new ("(b)", FALSE), FALSE);
    }

    if (do_inhibit) {
        GVariant *body;
        body = g_variant_new ("(ss)", inhibit_application ? inhibit_application : "Unknown",
                                    inhibit_reason ? inhibit_reason : "Unknown"); 
        reply = screensaver_send_message (conn, "Inhibit", body, TRUE);
        if (!reply) {
            g_message ("Did not receive a reply from the screensaver.");
            goto done;
        }
        g_object_unref (reply);

        return FALSE;
    }

done:
    g_main_loop_quit (loop);

    return FALSE;
}

int
main (int    argc,
      char **argv) {
    GDBusConnection *conn;
    GOptionContext *context;
    gboolean        retval;
    GError         *error = NULL;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, XFCELOCALEDIR);
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# endif
    textdomain (GETTEXT_PACKAGE);
#endif

    g_set_prgname (argv[0]);

    if (setlocale (LC_ALL, "") == NULL)
        g_warning ("Locale not understood by C library, internationalization will not work\n");

    context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);
    retval = g_option_context_parse (context, &argc, &argv, &error);

    g_option_context_free (context);

    if (!retval) {
        g_warning ("%s", error->message);
        g_error_free (error);
        exit (1);
    }

    if (do_version) {
        g_print ("%s %s\n", argv[0], VERSION);
        exit (1);
    }

    conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (conn == NULL) {
            g_message ("Failed to get session bus: %s", error->message);
            g_error_free (error);
            return EXIT_FAILURE;
    }
    g_idle_add (do_command, conn);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);
    g_object_unref (conn);
    exit (return_code);
}
