/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2018      Sean Davis <bluesabre@xfce.org>
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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "gs-auth.h"
#include "gs-debug.h"
#include "gs-lock-plug.h"
#include "setuid.h"
#include "xfce4-screensaver-dialog-css.h"

#define MAX_FAILURES 5

static gboolean verbose = FALSE;
static gboolean show_version = FALSE;
static gboolean enable_logout = FALSE;
static gboolean enable_switch = FALSE;
static gint dialog_height = 0;
static gint dialog_width = 0;
static char* logout_command = NULL;
static char* status_message = NULL;
static char* away_message = NULL;
static gint monitor_index = 0;

static GOptionEntry entries[] = {
    {"verbose", 0, 0, G_OPTION_ARG_NONE, &verbose,
        N_("Show debugging output"), NULL},
    {"version", 0, 0, G_OPTION_ARG_NONE, &show_version,
        N_("Version of this application"), NULL},
    {"enable-logout", 0, 0, G_OPTION_ARG_NONE, &enable_logout,
        N_("Show the logout button"), NULL},
    {"logout-command", 0, 0, G_OPTION_ARG_STRING, &logout_command,
        N_("Command to invoke from the logout button"), NULL},
    {"enable-switch", 0, 0, G_OPTION_ARG_NONE, &enable_switch,
        N_("Show the switch user button"), NULL},
    {"status-message", 0, 0, G_OPTION_ARG_STRING, &status_message,
        N_("Message to show in the dialog"), N_("MESSAGE")},
    {"away-message", 0, 0, G_OPTION_ARG_STRING, &away_message,
        N_("Not used"), N_("MESSAGE")},
    {"height", 0, 0, G_OPTION_ARG_INT, &dialog_height,
        N_("Monitor height"), NULL},
    {"width", 0, 0, G_OPTION_ARG_INT, &dialog_width,
        N_("Monitor width"), NULL},
    {"monitor", 0, 0, G_OPTION_ARG_INT, &monitor_index,
        N_("Monitor index"), NULL},
    {NULL}
};

static char* get_id_string(GtkWidget* widget) {
    char* id = NULL;

    g_return_val_if_fail(widget != NULL, NULL);
    g_return_val_if_fail(GTK_IS_WIDGET(widget), NULL);

    id = g_strdup_printf("%" G_GUINT32_FORMAT, (guint32) GDK_WINDOW_XID(gtk_widget_get_window(widget)));
    return id;
}

static gboolean print_id(GtkWidget* widget) {
    char* id;

    gs_profile_start(NULL);

    id = get_id_string(widget);
    printf("WINDOW ID=%s\n", id);
    fflush(stdout);

    gs_profile_end(NULL);

    g_free(id);

    return FALSE;
}

static void response_cancel(void) {
    printf("RESPONSE=CANCEL\n");
    fflush(stdout);
}

static void response_ok(void) {
    printf("RESPONSE=OK\n");
    fflush(stdout);
}

static gboolean quit_response_ok(gpointer user_data) {
    response_ok();
    gtk_main_quit();
    return FALSE;
}

static gboolean quit_response_cancel(void) {
    response_cancel();
    gtk_main_quit();
    return FALSE;
}

static void response_lock_init_failed(void) {
    /* if we fail to lock then we should drop the dialog */
    response_ok();
}

static char* request_response(GSLockPlug* plug,
                              const char* prompt,
                              gboolean visible) {
    int response;
    char* text;

    gs_lock_plug_set_sensitive(plug, TRUE);
    gs_lock_plug_enable_prompt(plug, prompt, visible);
    response = gs_lock_plug_run(plug);

    gs_debug ("Got response: %d", response);

    text = NULL;

    if (response == GS_LOCK_PLUG_RESPONSE_OK) {
        gs_lock_plug_get_text(plug, &text);
    }

    gs_lock_plug_disable_prompt(plug);

    return text;
}

static gboolean status_text_should_be_hidden(const char* msg) {
    return g_str_equal(msg, pam_dgettext("Password: ")) ||
           // "Password:" is the default on OpenPAM.
           g_str_equal(msg, "Password:");
}

static gboolean auth_message_handler(GSAuthMessageStyle   style,
                                     const char          *msg,
                                     char               **response,
                                     gpointer             data) {
    gboolean    ret;
    GSLockPlug *plug;
    const char *message;

    plug = GS_LOCK_PLUG(data);

    gs_profile_start(NULL);
    gs_debug("Got message style %d: '%s'", style, msg);

    gtk_widget_show(gs_lock_plug_get_widget(plug));
    gs_lock_plug_set_ready(plug);

    ret = TRUE;
    *response = NULL;
    message = status_text_should_be_hidden(msg) ? "" : msg;

    switch (style) {
        case GS_AUTH_MESSAGE_PROMPT_ECHO_ON:
            if (msg != NULL) {
                char *resp;
                resp = request_response(plug, message, TRUE);
                *response = resp;
            }
            break;
        case GS_AUTH_MESSAGE_PROMPT_ECHO_OFF:
            if (msg != NULL) {
                char *resp;
                resp = request_response(plug, message, FALSE);
                *response = resp;
            }
            break;
        case GS_AUTH_MESSAGE_ERROR_MSG:
            gs_lock_plug_show_message(plug, message);
            break;
        case GS_AUTH_MESSAGE_TEXT_INFO:
            gs_lock_plug_show_message(plug, message);
            break;
        default:
            g_assert_not_reached();
    }

    if (*response == NULL) {
        gs_debug("Got no response");
        ret = FALSE;
    } else {
        gs_lock_plug_show_message(plug, _("Checking..."));
        gs_lock_plug_set_sensitive(plug, FALSE);
    }

    /* we may have pending events that should be processed before continuing back into PAM */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    gs_lock_plug_set_busy(plug);
    gs_profile_end(NULL);

    return ret;
}

static gboolean reset_idle_cb(gpointer user_data) {
    GSLockPlug* plug = user_data;
    gs_lock_plug_set_sensitive(plug, TRUE);
    gs_lock_plug_show_message(plug, NULL);

    return FALSE;
}

static gboolean do_auth_check(GSLockPlug* plug) {
    GError   *error;
    gboolean  res;

    error = NULL;

    res = gs_auth_verify_user(g_get_user_name(), g_getenv("DISPLAY"), auth_message_handler, plug, &error);

    gs_debug("Verify user returned: %s", res ? "TRUE" : "FALSE");

    if (!res) {
        if (error != NULL) {
            gs_debug("Verify user returned error: %s", error->message);
            gs_lock_plug_show_message(plug, error->message);
        } else {
            gs_lock_plug_show_message(plug, _("Authentication failed."));
        }

        printf("NOTICE=AUTH FAILED\n");
        fflush(stdout);

        if (error != NULL) {
            g_error_free(error);
        }
    }

    return res;
}

static void response_cb(GSLockPlug *plug,
                        gint        response_id) {
    if ((response_id == GS_LOCK_PLUG_RESPONSE_CANCEL) || (response_id == GTK_RESPONSE_DELETE_EVENT)) {
        quit_response_cancel();
    }
}

static gboolean response_request_quit(gpointer user_data) {
    printf("REQUEST QUIT\n");
    fflush(stdout);
    return FALSE;
}

static gboolean quit_timeout_cb(gpointer user_data) {
    gtk_main_quit();
    return FALSE;
}

static gboolean auth_check_idle(gpointer user_data) {
    GSLockPlug* plug = user_data;
    gboolean     res;
    gboolean     again;
    static guint loop_counter = 0;

    again = TRUE;
    res = do_auth_check(plug);

    if (res) {
        again = FALSE;
        g_idle_add(quit_response_ok, NULL);
    } else {
        loop_counter++;

        if (loop_counter < MAX_FAILURES) {
            gs_debug ("Authentication failed, retrying (%u)", loop_counter);
            g_timeout_add_seconds (3, reset_idle_cb, plug);
        } else {
            gs_debug ("Authentication failed, quitting (max failures)");
            again = FALSE;
            /* Don't quit immediately, but rather request that xfce4-screensaver
             * terminates us after it has finished the dialog shake. Time out
             * after 5 seconds and quit anyway if this doesn't happen though */
            g_idle_add(response_request_quit, NULL);
            g_timeout_add_seconds(5, quit_timeout_cb, NULL);
        }
    }

    return again;
}

static void show_cb(GtkWidget *widget,
                    gpointer   data) {
    print_id(widget);
}

static gboolean popup_dialog_idle(gpointer user_data) {
    GSLockPlug     *plug;
    GtkWidget      *plug_widget;
    GtkCssProvider *css_provider;

    gs_profile_start(NULL);

    plug = gs_lock_plug_new(enable_logout, logout_command, enable_switch, status_message, monitor_index);
    plug_widget = gs_lock_plug_get_widget(plug);
    gtk_widget_set_size_request(plug_widget, dialog_width, dialog_height);
    g_signal_connect(GS_LOCK_PLUG(plug), "response", G_CALLBACK(response_cb), NULL);
    g_signal_connect(plug_widget, "show", G_CALLBACK(show_cb), NULL);

    css_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (css_provider,
                                     xfce4_screensaver_dialog_css,
                                     xfce4_screensaver_dialog_css_length,
                                     NULL);
    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_realize(plug_widget);
    gtk_widget_show(plug_widget);

    g_idle_add(auth_check_idle, plug);

    gs_profile_end(NULL);

    return FALSE;
}


/*
 * Copyright (c) 1991-2004 Jamie Zawinski <jwz@jwz.org>
 * Copyright (c) 2005 William Jon McCann <mccann@jhu.edu>
 *
 * Initializations that potentially take place as a priveleged user:
   If the executable is setuid root, then these initializations
   are run as root, before discarding privileges.
*/
static gboolean privileged_initialization(int       *argc,
                                          char     **argv,
                                          gboolean   local_verbose) {
    gboolean  ret;
    char     *nolock_reason;
    char     *orig_uid;
    char     *uid_message;

    gs_profile_start(NULL);

    #ifndef NO_LOCKING
        /* before hack_uid () for proper permissions */
        gs_auth_priv_init();
    #endif /* NO_LOCKING */

    ret = hack_uid(&nolock_reason, &orig_uid, &uid_message);

    if (nolock_reason) {
        gs_debug("Locking disabled: %s", nolock_reason);
    }

    if (uid_message && local_verbose) {
        g_print("Modified UID: %s", uid_message);
    }

    g_free(nolock_reason);
    g_free(orig_uid);
    g_free(uid_message);

    gs_profile_end(NULL);

    return ret;
}


/*
 * Copyright (c) 1991-2004 Jamie Zawinski <jwz@jwz.org>
 * Copyright (c) 2005 William Jon McCann <mccann@jhu.edu>
 *
 * Figure out what locking mechanisms are supported.
 */
static gboolean lock_initialization (int       *argc,
                                     char     **argv,
                                     char     **nolock_reason,
                                     gboolean   local_verbose) {
    if (nolock_reason != NULL) {
        *nolock_reason = NULL;
    }

    #ifdef NO_LOCKING

        if (nolock_reason != NULL) {
            *nolock_reason = g_strdup("not compiled with locking support");
        }

        return FALSE;
    #else /* !NO_LOCKING */

        /* Finish initializing locking, now that we're out of privileged code. */
        if (!gs_auth_init()) {
            if (nolock_reason != NULL) {
                *nolock_reason = g_strdup("error getting password");
            }

            return FALSE;
        }

        /* If locking is currently enabled, but the environment indicates that
         * we have been launched as MDM's "Background" program, then disable
         * locking just in case.
         */
        if (getenv("RUNNING_UNDER_MDM")) {
            if (nolock_reason != NULL) {
                *nolock_reason = g_strdup("running under MDM");
            }

            return FALSE;
        }

        /* If the server is XDarwin (MacOS X) then disable locking.
         * (X grabs only affect X programs, so you can use Command-Tab
         * to bring any other Mac program to the front, e.g., Terminal.)
         */
        {
            #ifdef __APPLE__
                /* Disable locking if *running* on Apple hardware, since we have no
                 * reliable way to determine whether the server is running on MacOS.
                 * Hopefully __APPLE__ means "MacOS" and not "Linux on Mac hardware"
                 * but I'm not really sure about that.
                 */
                if (nolock_reason != NULL) {
                    *nolock_reason = g_strdup("Cannot lock securely on MacOS X");
                }

                return FALSE;
            #endif /* __APPLE__ */
        }

    #endif /* NO_LOCKING */

    return TRUE;
}

int main(int    argc,
         char **argv) {
    GError *error = NULL;
    char   *nolock_reason = NULL;

    /* The order matters since xfce_textdomain sets default domain. */
#ifdef HAVE_LINUX_PAM
    xfce_textdomain ("Linux-PAM", LINUXPAMLOCALEDIR, "UTF-8");
#endif
    xfce_textdomain (GETTEXT_PACKAGE, XFCELOCALEDIR, "UTF-8");

    gs_profile_start(NULL);

    if (!privileged_initialization(&argc, argv, verbose)) {
        response_lock_init_failed();
        exit(1);
    }

    error = NULL;

    if (!gtk_init_with_args(&argc, &argv, NULL, entries, NULL, &error)) {
        if (error != NULL) {
            fprintf(stderr, "%s", error->message);
            g_error_free(error);
        }

        exit(1);
    }

    if (!xfconf_init(&error)) {
        g_error("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free(error);

        exit(1);
    }

    if (show_version) {
        g_print("%s %s\n", argv[0], VERSION);
        exit(1);
    }

    if (!lock_initialization(&argc, argv, &nolock_reason, verbose)) {
        if (nolock_reason != NULL) {
            gs_debug ("Screen locking disabled: %s", nolock_reason);
            g_free (nolock_reason);
        }

        response_lock_init_failed();
        exit (1);
    }

    gs_debug_init(verbose, FALSE);

    g_idle_add(popup_dialog_idle, NULL);

    gtk_main();

    gs_profile_end(NULL);
    gs_debug_shutdown();
    xfconf_shutdown ();

    return 0;
}
