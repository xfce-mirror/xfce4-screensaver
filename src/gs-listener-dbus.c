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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef WITH_SYSTEMD
#include <systemd/sd-login.h>
#endif
#ifdef WITH_ELOGIND
#include <elogind/sd-login.h>
#endif

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <libxfce4util/libxfce4util.h>

#include "gs-debug.h"
#include "gs-listener-dbus.h"
#include "gs-marshal.h"
#include "gs-prefs.h"

static void
gs_listener_dbus_finalize (GObject *object);

static void
gs_listener_dbus_unregister_handler (DBusConnection *connection,
                                     void *data);

static DBusHandlerResult
gs_listener_dbus_message_handler (DBusConnection *connection,
                                  DBusMessage *message,
                                  void *user_data);

#define GS_LISTENER_DBUS_SERVICE "org.xfce.ScreenSaver"
#define GS_LISTENER_DBUS_PATH "/org/xfce/ScreenSaver"
#define GS_LISTENER_DBUS_INTERFACE "org.xfce.ScreenSaver"

#define HAL_DEVICE_INTERFACE "org.freedesktop.Hal.Device"

/* systemd logind */
#define LOGIND_SERVICE "org.freedesktop.login1"
#define LOGIND_PATH "/org/freedesktop/login1"
#define LOGIND_INTERFACE "org.freedesktop.login1.Manager"

#define LOGIND_SESSION_INTERFACE "org.freedesktop.login1.Session"
#define LOGIND_SESSION_PATH "/org/freedesktop/login1/session"

/* consolekit */
#define CK_NAME "org.freedesktop.ConsoleKit"
#define CK_MANAGER_PATH "/org/freedesktop/ConsoleKit/Manager"
#define CK_MANAGER_INTERFACE "org.freedesktop.ConsoleKit.Manager"
#define CK_SESSION_INTERFACE "org.freedesktop.ConsoleKit.Session"

#define TYPE_MISMATCH_ERROR GS_LISTENER_DBUS_INTERFACE ".TypeMismatch"

struct GSListenerDBusPrivate {
    DBusConnection *connection;
    DBusConnection *system_connection;

    GSPrefs *prefs;
    guint session_idle : 1;
    guint active : 1;
    guint throttled : 1;
    GHashTable *inhibitors;
    GHashTable *throttlers;
    time_t active_start;
    time_t session_idle_start;
    char *session_id;

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    gboolean have_logind;
#endif

    guint32 ck_throttle_cookie;
    int sleep_inhibitor;
};

typedef struct {
    int entry_type;
    char *application;
    char *reason;
    char *connection;
    guint32 cookie;
    gint32 fd;
    GDateTime *since;
} GSListenerDBusRefEntry;

enum {
    LOCK,
    CYCLE,
    QUIT,
    SIMULATE_USER_ACTIVITY,
    ACTIVE_CHANGED,
    THROTTLE_CHANGED,
    SHOW_MESSAGE,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_ACTIVE,
};

enum {
    REF_ENTRY_TYPE_INHIBIT,
    REF_ENTRY_TYPE_THROTTLE
};

static DBusObjectPathVTable
    gs_listener_dbus_vtable = { &gs_listener_dbus_unregister_handler,
                                &gs_listener_dbus_message_handler,
                                NULL,
                                NULL,
                                NULL,
                                NULL };

static guint signals[LAST_SIGNAL] = {
    0,
};

G_DEFINE_TYPE_WITH_PRIVATE (GSListenerDBus, gs_listener_dbus, G_TYPE_OBJECT)

GQuark
gs_listener_dbus_error_quark (void) {
    static GQuark quark = 0;
    if (!quark) {
        quark = g_quark_from_static_string ("gs_listener_dbus_error");
    }

    return quark;
}

static void
gs_listener_dbus_ref_entry_free (GSListenerDBusRefEntry *entry) {
    g_free (entry->connection);
    g_free (entry->application);
    g_free (entry->reason);
    g_date_time_unref (entry->since);
    g_free (entry);
}

static void
gs_listener_dbus_unregister_handler (DBusConnection *connection,
                                     void *data) {
}

static gboolean
send_dbus_message (DBusConnection *connection,
                   DBusMessage *message) {
    gboolean is_connected;
    gboolean sent;

    g_return_val_if_fail (message != NULL, FALSE);

    if (!connection) {
        gs_debug ("There is no valid connection to the message bus");
        return FALSE;
    }

    is_connected = dbus_connection_get_is_connected (connection);
    if (!is_connected) {
        gs_debug ("Not connected to the message bus");
        return FALSE;
    }

    sent = dbus_connection_send (connection, message, NULL);

    return sent;
}

static void
send_dbus_boolean_signal (GSListenerDBus *listener,
                          const char *name,
                          gboolean value) {
    DBusMessage *message;
    DBusMessageIter iter;

    g_return_if_fail (listener != NULL);

    message = dbus_message_new_signal (GS_LISTENER_DBUS_PATH,
                                       GS_LISTENER_DBUS_SERVICE,
                                       name);

    dbus_message_iter_init_append (message, &iter);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_BOOLEAN, &value);

    if (!send_dbus_message (listener->priv->connection, message)) {
        gs_debug ("Could not send %s signal", name);
    }

    dbus_message_unref (message);
}

static void
gs_listener_dbus_send_signal_active_changed (GSListenerDBus *listener) {
    g_return_if_fail (listener != NULL);

    gs_debug ("Sending the ActiveChanged(%s) signal on the session bus",
              listener->priv->active ? "TRUE" : "FALSE");

    send_dbus_boolean_signal (listener, "ActiveChanged", listener->priv->active);
}

static const char *
get_name_for_entry_type (int entry_type) {
    const char *name;

    switch (entry_type) {
        case REF_ENTRY_TYPE_INHIBIT:
            name = "inhibitor";
            break;
        case REF_ENTRY_TYPE_THROTTLE:
            name = "throttler";
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    return name;
}

static GHashTable *
get_hash_for_entry_type (GSListenerDBus *listener,
                         int entry_type) {
    GHashTable *hash;

    switch (entry_type) {
        case REF_ENTRY_TYPE_INHIBIT:
            hash = listener->priv->inhibitors;
            break;
        case REF_ENTRY_TYPE_THROTTLE:
            hash = listener->priv->throttlers;
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    return hash;
}

static void
list_ref_entry (gpointer key,
                gpointer value,
                gpointer user_data) {
    GSListenerDBusRefEntry *entry;

    entry = (GSListenerDBusRefEntry *) value;

    gs_debug ("%s: %s for reason: %s",
              get_name_for_entry_type (entry->entry_type),
              entry->application,
              entry->reason);
}

static gboolean
listener_ref_entry_is_present (GSListenerDBus *listener,
                               int entry_type) {
    guint n_entries;
    gboolean is_set;
    GHashTable *hash;

    hash = get_hash_for_entry_type (listener, entry_type);

    /* if we aren't inhibited then activate */
    n_entries = 0;
    if (hash != NULL) {
        n_entries = g_hash_table_size (hash);

        g_hash_table_foreach (hash, list_ref_entry, NULL);
    }

    is_set = (n_entries > 0);

    return is_set;
}

static gboolean
listener_check_activation (GSListenerDBus *listener) {
    gboolean inhibited;
    gboolean res;

    gs_debug ("Checking for activation");

    if (!listener->priv->prefs->saver_enabled || !listener->priv->prefs->idle_activation_enabled) {
        return TRUE;
    }

    if (!listener->priv->session_idle) {
        return TRUE;
    }

    /* if we aren't inhibited then activate */
    inhibited = listener_ref_entry_is_present (listener, REF_ENTRY_TYPE_INHIBIT);

    res = FALSE;
    if (!inhibited) {
        gs_debug ("Trying to activate");
        res = gs_listener_dbus_activate_saver (listener, TRUE);
    }

    return res;
}

static void
gs_listener_dbus_set_throttle (GSListenerDBus *listener,
                               gboolean throttled) {
    g_return_if_fail (GS_IS_LISTENER_DBUS (listener));

    if (listener->priv->throttled != throttled) {
        gs_debug ("Changing throttle status: %d", throttled);

        listener->priv->throttled = throttled;

        g_signal_emit (listener, signals[THROTTLE_CHANGED], 0, throttled);
    }
}

static gboolean
listener_check_throttle (GSListenerDBus *listener) {
    gboolean throttled;

    gs_debug ("Checking for throttle");

    throttled = listener_ref_entry_is_present (listener, REF_ENTRY_TYPE_THROTTLE);

    if (throttled != listener->priv->throttled) {
        gs_listener_dbus_set_throttle (listener, throttled);
    }

    return TRUE;
}

static gboolean
listener_set_active_internal (GSListenerDBus *listener,
                              gboolean active) {
    listener->priv->active = active;

    if (active) {
        listener->priv->active_start = time (NULL);
    } else {
        listener->priv->active_start = 0;
    }

    gs_listener_dbus_send_signal_active_changed (listener);

    return TRUE;
}

gboolean
gs_listener_dbus_activate_saver (GSListenerDBus *listener,
                                 gboolean active) {
    gboolean res;

    g_return_val_if_fail (GS_IS_LISTENER_DBUS (listener), FALSE);

    if (listener->priv->active == active) {
        return TRUE;
    }

    g_signal_emit (listener, signals[ACTIVE_CHANGED], 0, active, &res);
    if (res) {
        listener_set_active_internal (listener, active);
        return TRUE;
    }

    return FALSE;
}

gboolean
gs_listener_dbus_is_inhibited (GSListenerDBus *listener) {
    gboolean inhibited;

    g_return_val_if_fail (GS_IS_LISTENER_DBUS (listener), FALSE);

    inhibited = listener_ref_entry_is_present (listener, REF_ENTRY_TYPE_INHIBIT);

    return inhibited;
}

static dbus_bool_t
listener_property_set_bool (GSListenerDBus *listener,
                            guint prop_id,
                            dbus_bool_t value) {
    dbus_bool_t ret;

    ret = FALSE;

    switch (prop_id) {
        case PROP_ACTIVE:
            gs_listener_dbus_activate_saver (listener, value);
            ret = TRUE;
            break;
        default:
            break;
    }

    return ret;
}

static void
raise_error (DBusConnection *connection,
             DBusMessage *in_reply_to,
             const char *error_name,
             char *format, ...) {
    char buf[512];
    DBusMessage *reply;

    va_list args;
    va_start (args, format);
    vsnprintf (buf, sizeof (buf), format, args);
    va_end (args);

    gs_debug (buf);
    reply = dbus_message_new_error (in_reply_to, error_name, buf);
    if (reply == NULL) {
        g_error ("No memory");
    }
    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);
}

static void
raise_syntax (DBusConnection *connection,
              DBusMessage *in_reply_to,
              const char *method_name) {
    raise_error (connection, in_reply_to,
                 GS_LISTENER_DBUS_SERVICE ".SyntaxError",
                 "There is a syntax error in the invocation of the method %s",
                 method_name);
}

static guint32
generate_cookie (void) {
    guint32 cookie;

    cookie = (guint32) g_random_int_range (1, G_MAXINT32);

    return cookie;
}

static guint32
listener_generate_unique_key (GSListenerDBus *listener,
                              int entry_type) {
    guint32 cookie;
    GHashTable *hash;

    hash = get_hash_for_entry_type (listener, entry_type);

    do {
        cookie = generate_cookie ();
    } while (g_hash_table_lookup (hash, &cookie) != NULL);

    return cookie;
}

static void
listener_ref_entry_check (GSListenerDBus *listener,
                          int entry_type) {
    switch (entry_type) {
        case REF_ENTRY_TYPE_INHIBIT:
            listener_check_activation (listener);
            break;
        case REF_ENTRY_TYPE_THROTTLE:
            listener_check_throttle (listener);
            break;
        default:
            g_assert_not_reached ();
            break;
    }
}

static void
add_sleep_inhibit (GSListenerDBus *listener) {
    DBusMessage *message = NULL, *reply = NULL;
    DBusMessageIter iter, reply_iter;
    DBusError error;
    const gchar *what = "sleep";
    const gchar *who = "xfce4-screensaver";
    const gchar *why = "Locking screen before sleep";
    const gchar *mode = "delay";

    g_return_if_fail (listener != NULL);

    if (listener->priv->system_connection == NULL) {
        gs_debug ("No connection to the system bus");
        return;
    }

    dbus_error_init (&error);

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    message = dbus_message_new_method_call (LOGIND_SERVICE,
                                            LOGIND_PATH,
                                            LOGIND_INTERFACE,
                                            "Inhibit");
#elif defined(WITH_CONSOLE_KIT)
    message = dbus_message_new_method_call (CK_NAME,
                                            CK_MANAGER_PATH,
                                            CK_MANAGER_INTERFACE,
                                            "Inhibit");
#endif
    if (message == NULL) {
        gs_debug ("Couldn't allocate the dbus message");
        return;
    }

    dbus_message_iter_init_append (message, &iter);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &what);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &who);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &why);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &mode);

    reply = dbus_connection_send_with_reply_and_block (listener->priv->system_connection,
                                                       message,
                                                       -1,
                                                       &error);
    dbus_message_unref (message);

    if (dbus_error_is_set (&error)) {
        gs_debug ("%s raised:\n %s\n\n", error.name, error.message);
        dbus_error_free (&error);
        return;
    }

    dbus_message_iter_init (reply, &reply_iter);
    if (DBUS_TYPE_UNIX_FD == dbus_message_iter_get_arg_type (&reply_iter))
        dbus_message_iter_get_basic (&reply_iter, &listener->priv->sleep_inhibitor);

    dbus_message_unref (reply);
}

static void
remove_sleep_inhibit (GSListenerDBus *listener) {
    g_return_if_fail (listener != NULL);

    if (listener->priv->sleep_inhibitor < 0) {
        gs_debug ("Can't remove sleep inhibitor: Lock not acquired");
        return;
    }

    if (close (listener->priv->sleep_inhibitor) < 0) {
        gs_debug ("Can't close file descriptor");
        return;
    }
}

static void
add_session_inhibit (GSListenerDBus *listener,
                     GSListenerDBusRefEntry *entry) {
    DBusMessage *message = NULL, *reply = NULL;
    DBusMessageIter iter, reply_iter;
    DBusError error;
    const gchar *mode = "block";
    /* it is a colon-separated list of lock types */
    const gchar *what = "idle";

    g_return_if_fail (listener != NULL);

    dbus_error_init (&error);

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    message = dbus_message_new_method_call (LOGIND_SERVICE,
                                            LOGIND_PATH,
                                            LOGIND_INTERFACE,
                                            "Inhibit");
#elif defined(WITH_CONSOLE_KIT)
    message = dbus_message_new_method_call (CK_NAME,
                                            CK_MANAGER_PATH,
                                            CK_MANAGER_INTERFACE,
                                            "Inhibit");
#endif
    if (message == NULL) {
        gs_debug ("Couldn't allocate the dbus message");
        return;
    }

    dbus_message_iter_init_append (message, &iter);
    /* what parameter */
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &what);
    /* who parameter */
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING,
                                    &entry->application);
    /* why parameter */
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING,
                                    &entry->reason);
    /* mode parameter */
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &mode);

    /* FIXME: use async? */
    reply = dbus_connection_send_with_reply_and_block (listener->priv->system_connection,
                                                       message,
                                                       -1,
                                                       &error);
    dbus_message_unref (message);

    if (dbus_error_is_set (&error)) {
        gs_debug ("%s raised:\n %s\n\n", error.name, error.message);
        dbus_error_free (&error);
        return;
    }

    dbus_message_iter_init (reply, &reply_iter);
    if (DBUS_TYPE_UNIX_FD == dbus_message_iter_get_arg_type (&reply_iter))
        dbus_message_iter_get_basic (&reply_iter, &entry->fd);

    dbus_message_unref (reply);
}

static void
remove_session_inhibit (GSListenerDBusRefEntry *entry) {
    if (entry->fd < 0) {
        gs_debug ("Can't remove inhibitor from session: Session cookie not set");
        return;
    }

    if (close (entry->fd) < 0) {
        gs_debug ("Can't close file descriptor");
        return;
    }
}

static void
listener_add_ref_entry (GSListenerDBus *listener,
                        int entry_type,
                        GSListenerDBusRefEntry *entry) {
    GHashTable *hash;

    gs_debug ("Adding %s from %s for reason '%s' on connection %s",
              get_name_for_entry_type (entry_type),
              entry->application,
              entry->reason,
              entry->connection);

    hash = get_hash_for_entry_type (listener, entry_type);
    g_hash_table_insert (hash, &entry->cookie, entry);

    if (entry_type == REF_ENTRY_TYPE_INHIBIT) {
        /* proxy inhibit over to xfce session */
        add_session_inhibit (listener, entry);
    }

    listener_ref_entry_check (listener, entry_type);
}

static gboolean
listener_remove_ref_entry (GSListenerDBus *listener,
                           int entry_type,
                           guint32 cookie) {
    GHashTable *hash;
    gboolean removed = FALSE;
    GSListenerDBusRefEntry *entry;

    hash = get_hash_for_entry_type (listener, entry_type);

    entry = g_hash_table_lookup (hash, &cookie);
    if (entry == NULL) {
        goto out;
    }

    gs_debug ("Removing %s from %s for reason '%s' on connection %s",
              get_name_for_entry_type (entry_type),
              entry->application,
              entry->reason,
              entry->connection);

    if (entry_type == REF_ENTRY_TYPE_INHIBIT) {
        /* remove inhibit from xfce session */
        remove_session_inhibit (entry);
    }

    removed = g_hash_table_remove (hash, &cookie);
out:
    if (removed) {
        listener_ref_entry_check (listener, entry_type);
    } else {
        gs_debug ("Cookie %u was not in the list!", cookie);
    }

    return removed;
}

/**
 * Borrowed from https://gitlab.gnome.org/GNOME/glib/blob/master/glib/gdatetime.c
 * g_date_time_format_iso8601 is not available until 2.62, but we support 2.50+
 * LGPL 2.1+
 */
static gchar *
listener_g_date_time_format_iso8601 (GDateTime *datetime) {
    GString *outstr = NULL;
    gchar *main_date = NULL;
    gint64 offset;

    /* Main date and time. */
    main_date = g_date_time_format (datetime, "%Y-%m-%dT%H:%M:%S");
    outstr = g_string_new (main_date);
    g_free (main_date);

    /* Timezone. Format it as `%:::z` unless the offset is zero, in which case
     * we can simply use `Z`. */
    offset = g_date_time_get_utc_offset (datetime);

    if (offset == 0) {
        g_string_append_c (outstr, 'Z');
    } else {
        gchar *time_zone = g_date_time_format (datetime, "%:::z");
        g_string_append (outstr, time_zone);
        g_free (time_zone);
    }

    return g_string_free (outstr, FALSE);
}


static void
accumulate_ref_entry (gpointer key,
                      GSListenerDBusRefEntry *entry,
                      DBusMessageIter *iter) {
    char *description;
    char *time;

    time = listener_g_date_time_format_iso8601 (entry->since);

    description = g_strdup_printf ("Application=\"%s\"; Since=\"%s\"; Reason=\"%s\";",
                                   entry->application,
                                   time,
                                   entry->reason);

    dbus_message_iter_append_basic (iter, DBUS_TYPE_STRING, &description);

    g_free (description);
    g_free (time);
}

static DBusHandlerResult
listener_dbus_get_ref_entries (GSListenerDBus *listener,
                               int entry_type,
                               DBusConnection *connection,
                               DBusMessage *message) {
    DBusMessage *reply;
    GHashTable *hash;
    DBusMessageIter iter;
    DBusMessageIter iter_array;

    hash = get_hash_for_entry_type (listener, entry_type);

    reply = dbus_message_new_method_return (message);
    if (reply == NULL) {
        g_error ("No memory");
    }

    dbus_message_iter_init_append (reply, &iter);
    dbus_message_iter_open_container (&iter,
                                      DBUS_TYPE_ARRAY,
                                      DBUS_TYPE_STRING_AS_STRING,
                                      &iter_array);

    if (hash != NULL) {
        g_hash_table_foreach (hash,
                              (GHFunc) accumulate_ref_entry,
                              &iter_array);
    }

    dbus_message_iter_close_container (&iter, &iter_array);

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

#ifdef WITH_CONSOLE_KIT
static void
listener_add_ck_ref_entry (GSListenerDBus *listener,
                           int entry_type,
                           DBusConnection *connection,
                           DBusMessage *message,
                           guint32 *cookiep) {
    GSListenerDBusRefEntry *entry;

    entry = g_new0 (GSListenerDBusRefEntry, 1);
    entry->entry_type = entry_type;
    entry->connection = g_strdup (dbus_message_get_sender (message));
    entry->cookie = listener_generate_unique_key (listener, entry_type);
    entry->application = g_strdup ("ConsoleKit");
    entry->reason = g_strdup ("Session is not active");
    entry->since = g_date_time_new_now_local ();

    /* takes ownership of entry */
    listener_add_ref_entry (listener, entry_type, entry);

    if (cookiep != NULL) {
        *cookiep = entry->cookie;
    }
}

static void
listener_remove_ck_ref_entry (GSListenerDBus *listener,
                              int entry_type,
                              guint32 cookie) {
    listener_remove_ref_entry (listener, entry_type, cookie);
}
#endif

static DBusHandlerResult
listener_dbus_confirm (DBusConnection *connection,
                       DBusMessage *message) {
    DBusMessage *reply;

    reply = dbus_message_new_method_return (message);
    if (reply == NULL) {
        g_error ("No memory");
    }

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_dbus_add_ref_entry (GSListenerDBus *listener,
                             int entry_type,
                             DBusConnection *connection,
                             DBusMessage *message) {
    DBusMessage *reply;
    DBusError error;
    const char *application;
    const char *reason;
    GSListenerDBusRefEntry *entry;
    DBusMessageIter iter;

    dbus_error_init (&error);
    if (!dbus_message_get_args (message, &error,
                                DBUS_TYPE_STRING, &application,
                                DBUS_TYPE_STRING, &reason,
                                DBUS_TYPE_INVALID)) {
        if (entry_type == REF_ENTRY_TYPE_INHIBIT) {
            raise_syntax (connection, message, "Inhibit");
        } else if (entry_type == REF_ENTRY_TYPE_THROTTLE) {
            raise_syntax (connection, message, "Throttle");
        } else {
            g_assert_not_reached ();
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    reply = dbus_message_new_method_return (message);
    if (reply == NULL) {
        g_error ("No memory");
    }

    entry = g_new0 (GSListenerDBusRefEntry, 1);
    entry->entry_type = entry_type;
    entry->connection = g_strdup (dbus_message_get_sender (message));
    entry->cookie = listener_generate_unique_key (listener, entry_type);
    entry->application = g_strdup (application);
    entry->reason = g_strdup (reason);
    entry->since = g_date_time_new_now_local ();

    listener_add_ref_entry (listener, entry_type, entry);

    dbus_message_iter_init_append (reply, &iter);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &entry->cookie);

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_dbus_remove_ref_entry (GSListenerDBus *listener,
                                int entry_type,
                                DBusConnection *connection,
                                DBusMessage *message) {
    DBusMessage *reply;
    DBusError error;
    guint32 cookie;

    dbus_error_init (&error);
    if (!dbus_message_get_args (message, &error,
                                DBUS_TYPE_UINT32, &cookie,
                                DBUS_TYPE_INVALID)) {
        if (entry_type == REF_ENTRY_TYPE_INHIBIT) {
            raise_syntax (connection, message, "UnInhibit");
        } else if (entry_type == REF_ENTRY_TYPE_THROTTLE) {
            raise_syntax (connection, message, "UnThrottle");
        } else {
            g_assert_not_reached ();
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    reply = dbus_message_new_method_return (message);
    if (reply == NULL)
        g_error ("No memory");

    /* FIXME: check sender is from same connection as entry */
    dbus_message_get_sender (message);

    listener_remove_ref_entry (listener, entry_type, cookie);

    /* FIXME:  Pointless? */
    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static gboolean
listener_ref_entry_remove_for_connection (GSListenerDBus *listener,
                                          int entry_type,
                                          const char *connection) {
    gboolean removed;
    GHashTable *hash;
    GHashTableIter iter;
    GSListenerDBusRefEntry *entry;

    if (connection == NULL)
        return FALSE;

    hash = get_hash_for_entry_type (listener, entry_type);

    removed = FALSE;
    g_hash_table_iter_init (&iter, hash);
    while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &entry)) {
        if (entry->connection != NULL && strcmp (connection, entry->connection) == 0) {
            gs_debug ("Removing %s from %s for reason '%s' on connection %s",
                      get_name_for_entry_type (entry->entry_type),
                      entry->application,
                      entry->reason,
                      entry->connection);

            if (entry->entry_type == REF_ENTRY_TYPE_INHIBIT) {
                /* remove inhibit from xfce session */
                remove_session_inhibit (entry);
            }

            g_hash_table_iter_remove (&iter);
            removed = TRUE;
        }
    }

    return removed;
}

static void
listener_service_deleted (GSListenerDBus *listener,
                          DBusMessage *message) {
    const char *old_service_name;
    const char *new_service_name;
    gboolean removed;

    if (!dbus_message_get_args (message, NULL,
                                DBUS_TYPE_STRING, &old_service_name,
                                DBUS_TYPE_STRING, &new_service_name,
                                DBUS_TYPE_INVALID)) {
        g_error ("Invalid NameOwnerChanged signal from bus!");
        return;
    }

    removed = listener_ref_entry_remove_for_connection (listener, REF_ENTRY_TYPE_THROTTLE, new_service_name);
    if (removed) {
        listener_ref_entry_check (listener, REF_ENTRY_TYPE_THROTTLE);
    }

    removed = listener_ref_entry_remove_for_connection (listener, REF_ENTRY_TYPE_INHIBIT, new_service_name);
    if (removed) {
        listener_ref_entry_check (listener, REF_ENTRY_TYPE_INHIBIT);
    }
}

static void
raise_property_type_error (DBusConnection *connection,
                           DBusMessage *in_reply_to,
                           const char *device_id) {
    char buf[512];
    DBusMessage *reply;

    snprintf (buf, sizeof (buf),
              "Type mismatch setting property with id %s",
              device_id);
    gs_debug (buf);

    reply = dbus_message_new_error (in_reply_to,
                                    TYPE_MISMATCH_ERROR,
                                    buf);
    if (reply == NULL) {
        g_error ("No memory");
    }
    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);
}

static DBusHandlerResult
listener_set_property (GSListenerDBus *listener,
                       DBusConnection *connection,
                       DBusMessage *message,
                       guint prop_id) {
    const char *path;
    int type;
    gboolean rc;
    DBusMessageIter iter;
    DBusMessage *reply;

    path = dbus_message_get_path (message);

    dbus_message_iter_init (message, &iter);
    type = dbus_message_iter_get_arg_type (&iter);
    rc = FALSE;

    switch (type) {
        case DBUS_TYPE_BOOLEAN: {
            dbus_bool_t v;
            dbus_message_iter_get_basic (&iter, &v);
            rc = listener_property_set_bool (listener, prop_id, v);
            break;
        }
        default:
            gs_debug ("Unsupported property type %d", type);
            break;
    }

    if (!rc) {
        raise_property_type_error (connection, message, path);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    reply = dbus_message_new_method_return (message);

    if (reply == NULL) {
        g_error ("No memory");
    }

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_get_property (GSListenerDBus *listener,
                       DBusConnection *connection,
                       DBusMessage *message,
                       guint prop_id) {
    DBusMessageIter iter;
    DBusMessage *reply;

    reply = dbus_message_new_method_return (message);

    dbus_message_iter_init_append (reply, &iter);

    if (reply == NULL)
        g_error ("No memory");

    switch (prop_id) {
        case PROP_ACTIVE: {
            dbus_bool_t b;
            b = listener->priv->active;
            dbus_message_iter_append_basic (&iter, DBUS_TYPE_BOOLEAN, &b);
        } break;
        default:
            gs_debug ("Unsupported property id %u", prop_id);
            break;
    }

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_get_active_time (GSListenerDBus *listener,
                          DBusConnection *connection,
                          DBusMessage *message) {
    DBusMessageIter iter;
    DBusMessage *reply;
    dbus_uint32_t secs;

    reply = dbus_message_new_method_return (message);

    dbus_message_iter_init_append (reply, &iter);

    if (reply == NULL) {
        g_error ("No memory");
    }

    if (listener->priv->active) {
        time_t now = time (NULL);

        if (now < listener->priv->active_start) {
            /* shouldn't happen */
            gs_debug ("Active start time is in the future");
            secs = 0;
        } else if (listener->priv->active_start <= 0) {
            /* shouldn't happen */
            gs_debug ("Active start time was not set");
            secs = 0;
        } else {
            secs = now - listener->priv->active_start;
        }
    } else {
        secs = 0;
    }

    gs_debug ("Returning screensaver active for %u seconds", secs);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &secs);

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_show_message (GSListenerDBus *listener,
                       DBusConnection *connection,
                       DBusMessage *message) {
    DBusMessageIter iter;
    DBusMessage *reply;
    DBusError error;

    reply = dbus_message_new_method_return (message);

    dbus_message_iter_init_append (reply, &iter);

    if (reply == NULL) {
        g_error ("No memory");
    }

    if (listener->priv->active) {
        char *summary;
        char *body;
        char *icon;

        /* if we're not active we ignore the request */

        dbus_error_init (&error);
        if (!dbus_message_get_args (message, &error,
                                    DBUS_TYPE_STRING, &summary,
                                    DBUS_TYPE_STRING, &body,
                                    DBUS_TYPE_STRING, &icon,
                                    DBUS_TYPE_INVALID)) {
            raise_syntax (connection, message, "ShowMessage");
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        g_signal_emit (listener, signals[SHOW_MESSAGE], 0, summary, body, icon);
    }

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
do_introspect (DBusConnection *connection,
               DBusMessage *message,
               dbus_bool_t local_interface) {
    DBusMessage *reply;
    GString *xml;
    char *xml_string;

    /* standard header */
    xml = g_string_new ("<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
                        "<node>\n"
                        "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                        "    <method name=\"Introspect\">\n"
                        "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
                        "    </method>\n"
                        "  </interface>\n");

    /* ScreenSaver interface */
    xml = g_string_append (xml,
                           "  <interface name=\"org.xfce.ScreenSaver\">\n"
                           "    <method name=\"Lock\">\n"
                           "    </method>\n"
                           "    <method name=\"Cycle\">\n"
                           "    </method>\n"
                           "    <method name=\"SimulateUserActivity\">\n"
                           "    </method>\n"
                           "    <method name=\"Inhibit\">\n"
                           "      <arg name=\"application_name\" direction=\"in\" type=\"s\"/>\n"
                           "      <arg name=\"reason\" direction=\"in\" type=\"s\"/>\n"
                           "      <arg name=\"cookie\" direction=\"out\" type=\"u\"/>\n"
                           "    </method>\n"
                           "    <method name=\"UnInhibit\">\n"
                           "      <arg name=\"cookie\" direction=\"in\" type=\"u\"/>\n"
                           "    </method>\n"
                           "    <method name=\"GetInhibitors\">\n"
                           "      <arg name=\"list\" direction=\"out\" type=\"as\"/>\n"
                           "    </method>\n"
                           "    <method name=\"Throttle\">\n"
                           "      <arg name=\"application_name\" direction=\"in\" type=\"s\"/>\n"
                           "      <arg name=\"reason\" direction=\"in\" type=\"s\"/>\n"
                           "      <arg name=\"cookie\" direction=\"out\" type=\"u\"/>\n"
                           "    </method>\n"
                           "    <method name=\"UnThrottle\">\n"
                           "      <arg name=\"cookie\" direction=\"in\" type=\"u\"/>\n"
                           "    </method>\n"
                           "    <method name=\"GetActive\">\n"
                           "      <arg name=\"value\" direction=\"out\" type=\"b\"/>\n"
                           "    </method>\n"
                           "    <method name=\"GetActiveTime\">\n"
                           "      <arg name=\"seconds\" direction=\"out\" type=\"u\"/>\n"
                           "    </method>\n"
                           "    <method name=\"SetActive\">\n"
                           "      <arg name=\"value\" direction=\"in\" type=\"b\"/>\n"
                           "    </method>\n"
                           "    <method name=\"ShowMessage\">\n"
                           "      <arg name=\"summary\" direction=\"in\" type=\"s\"/>\n"
                           "      <arg name=\"body\" direction=\"in\" type=\"s\"/>\n"
                           "      <arg name=\"icon\" direction=\"in\" type=\"s\"/>\n"
                           "    </method>\n"
                           "    <signal name=\"ActiveChanged\">\n"
                           "      <arg name=\"new_value\" type=\"b\"/>\n"
                           "    </signal>\n"
                           "  </interface>\n");

    reply = dbus_message_new_method_return (message);

    xml = g_string_append (xml, "</node>\n");
    xml_string = g_string_free (xml, FALSE);

    dbus_message_append_args (reply,
                              DBUS_TYPE_STRING, &xml_string,
                              DBUS_TYPE_INVALID);

    g_free (xml_string);

    if (reply == NULL) {
        g_error ("No memory");
    }

    if (!dbus_connection_send (connection, reply, NULL)) {
        g_error ("No memory");
    }

    dbus_message_unref (reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_dbus_handle_session_message (DBusConnection *connection,
                                      DBusMessage *message,
                                      void *user_data,
                                      dbus_bool_t local_interface) {
    GSListenerDBus *listener = GS_LISTENER_DBUS (user_data);

#if 0
    g_message ("obj_path=%s interface=%s method=%s destination=%s",
               dbus_message_get_path (message),
               dbus_message_get_interface (message),
               dbus_message_get_member (message),
               dbus_message_get_destination (message));
#endif

    g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
    g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "Lock")) {
        g_signal_emit (listener, signals[LOCK], 0);
        return listener_dbus_confirm (connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "Quit")) {
        g_signal_emit (listener, signals[QUIT], 0);
        return listener_dbus_confirm (connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "Cycle")) {
        g_signal_emit (listener, signals[CYCLE], 0);
        return listener_dbus_confirm (connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "Inhibit")) {
        return listener_dbus_add_ref_entry (listener, REF_ENTRY_TYPE_INHIBIT, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "UnInhibit")) {
        return listener_dbus_remove_ref_entry (listener, REF_ENTRY_TYPE_INHIBIT, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "GetInhibitors")) {
        return listener_dbus_get_ref_entries (listener, REF_ENTRY_TYPE_INHIBIT, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "Throttle")) {
        return listener_dbus_add_ref_entry (listener, REF_ENTRY_TYPE_THROTTLE, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "UnThrottle")) {
        return listener_dbus_remove_ref_entry (listener, REF_ENTRY_TYPE_THROTTLE, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "SetActive")) {
        return listener_set_property (listener, connection, message, PROP_ACTIVE);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "GetActive")) {
        return listener_get_property (listener, connection, message, PROP_ACTIVE);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "GetActiveTime")) {
        return listener_get_active_time (listener, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "ShowMessage")) {
        return listener_show_message (listener, connection, message);
    }
    if (dbus_message_is_method_call (message, GS_LISTENER_DBUS_SERVICE, "SimulateUserActivity")) {
        g_signal_emit (listener, signals[SIMULATE_USER_ACTIVITY], 0);
        return listener_dbus_confirm (connection, message);
    }
    if (dbus_message_is_method_call (message, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        return do_introspect (connection, message, local_interface);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
_listener_dbus_message_path_is_our_session (GSListenerDBus *listener,
                                            DBusMessage *message) {
    const char *ssid;

    ssid = dbus_message_get_path (message);

    if (ssid == NULL)
        return FALSE;

    if (listener->priv->session_id == NULL)
        return FALSE;

    if (strcmp (ssid, listener->priv->session_id) == 0)
        return TRUE;

    return FALSE;
}

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
static gboolean
properties_changed_match (DBusMessage *message,
                          const char *property) {
    DBusMessageIter iter, sub, sub2;

    /* Checks whether a certain property is listed in the
     * specified PropertiesChanged message */

    if (!dbus_message_iter_init (message, &iter))
        goto failure;

    /* Jump over interface name */
    if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
        goto failure;

    dbus_message_iter_next (&iter);

    /* First, iterate through the changed properties array */
    if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_ARRAY
        || dbus_message_iter_get_element_type (&iter) != DBUS_TYPE_DICT_ENTRY)
        goto failure;

    dbus_message_iter_recurse (&iter, &sub);
    while (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_INVALID) {
        const char *name;

        if (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_DICT_ENTRY)
            goto failure;

        dbus_message_iter_recurse (&sub, &sub2);
        dbus_message_iter_get_basic (&sub2, &name);

        if (strcmp (name, property) == 0)
            return TRUE;

        dbus_message_iter_next (&sub);
    }

    dbus_message_iter_next (&iter);

    /* Second, iterate through the invalidated properties array */
    if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_ARRAY
        || dbus_message_iter_get_element_type (&iter) != DBUS_TYPE_STRING)
        goto failure;

    dbus_message_iter_recurse (&iter, &sub);
    while (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_INVALID) {
        const char *name;

        if (dbus_message_iter_get_arg_type (&sub) != DBUS_TYPE_STRING)
            goto failure;

        dbus_message_iter_get_basic (&sub, &name);

        if (strcmp (name, property) == 0)
            return TRUE;

        dbus_message_iter_next (&sub);
    }

    return FALSE;

failure:
    gs_debug ("Failed to decode PropertiesChanged message.");
    return FALSE;
}
#endif

static DBusHandlerResult
listener_dbus_handle_system_message (DBusConnection *connection,
                                     DBusMessage *message,
                                     void *user_data,
                                     dbus_bool_t local_interface) {
    GSListenerDBus *listener = GS_LISTENER_DBUS (user_data);

    g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
    g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

#if 0
    gs_debug ("obj_path=%s interface=%s method=%s destination=%s",
              dbus_message_get_path (message),
              dbus_message_get_interface (message),
              dbus_message_get_member (message),
              dbus_message_get_destination (message));
#endif

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    if (listener->priv->have_logind) {
        if (dbus_message_is_signal (message, LOGIND_SESSION_INTERFACE, "Unlock")) {
            if (_listener_dbus_message_path_is_our_session (listener, message)) {
                gs_debug ("Systemd requested session unlock");
                gs_listener_dbus_activate_saver (listener, FALSE);
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        } else if (dbus_message_is_signal (message, LOGIND_SESSION_INTERFACE, "Lock")) {
            if (_listener_dbus_message_path_is_our_session (listener, message)) {
                gs_debug ("Systemd requested session lock");
                g_signal_emit (listener, signals[LOCK], 0);
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        } else if (dbus_message_is_signal (message, LOGIND_INTERFACE, "PrepareForSleep")) {
            gboolean active = 0;
            DBusError error;

            gs_debug ("Handling Logind PrepareForSleep");

            dbus_error_init (&error);
            dbus_message_get_args (message, &error, DBUS_TYPE_BOOLEAN, &active, DBUS_TYPE_INVALID);
            if (active) {
                if (listener->priv->prefs->sleep_activation_enabled) {
                    gs_debug ("Logind requested session lock");
                    g_signal_emit (listener, signals[LOCK], 0);
                } else {
                    gs_debug ("Logind requested session lock, but lock on suspend is disabled");
                }

                gs_debug ("Releasing sleep inhibitor");
                remove_sleep_inhibit (listener);
            } else {
                gs_debug ("Reinstating logind sleep inhibitor lock");
                add_sleep_inhibit (listener);

                gs_debug ("Logind requested session unlock");
                // FIXME: there is no signal to request password prompt
                g_signal_emit (listener, signals[SHOW_MESSAGE], 0, NULL, NULL, NULL);
            }

            if (dbus_error_is_set (&error)) {
                dbus_error_free (&error);
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        } else if (dbus_message_is_signal (message, DBUS_INTERFACE_PROPERTIES, "PropertiesChanged")) {
            if (_listener_dbus_message_path_is_our_session (listener, message)) {
                if (properties_changed_match (message, "Active")) {
                    gboolean new_active;

                    /* Instead of going via the bus to read the new property state, let's
                     * shortcut this and ask directly the low-level information */
                    new_active = sd_session_is_active (listener->priv->session_id) != 0;
                    if (new_active)
                        g_signal_emit (listener, signals[SIMULATE_USER_ACTIVITY], 0);
                }
            }

            return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
#endif

#ifdef WITH_CONSOLE_KIT

    if (dbus_message_is_signal (message, HAL_DEVICE_INTERFACE, "Condition")) {
        DBusError error;
        const char *event;
        const char *keyname;

        dbus_error_init (&error);
        if (dbus_message_get_args (message, &error,
                                   DBUS_TYPE_STRING, &event,
                                   DBUS_TYPE_STRING, &keyname,
                                   DBUS_TYPE_INVALID)) {
            if ((event && strcmp (event, "ButtonPressed") == 0)
                && (keyname && strcmp (keyname, "coffee") == 0)) {
                gs_debug ("Coffee key was pressed - locking");
                g_signal_emit (listener, signals[LOCK], 0);
            }
        }

        if (dbus_error_is_set (&error)) {
            dbus_error_free (&error);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_signal (message, CK_SESSION_INTERFACE, "Unlock")) {
        if (_listener_dbus_message_path_is_our_session (listener, message)) {
            gs_debug ("Console kit requested session unlock");
            gs_listener_dbus_activate_saver (listener, FALSE);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_signal (message, CK_SESSION_INTERFACE, "Lock")) {
        if (_listener_dbus_message_path_is_our_session (listener, message)) {
            gs_debug ("ConsoleKit requested session lock");
            g_signal_emit (listener, signals[LOCK], 0);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_signal (message, CK_SESSION_INTERFACE, "ActiveChanged")) {
        /* NB that `ActiveChanged' refers to the active
         * session in ConsoleKit terminology - ie which
         * session is currently displayed on the screen.
         * xfce4-screensaver uses `active' to mean `is the
         * screensaver active' (ie, is the screen locked) but
         * that's not what we're referring to here.
         */

        if (_listener_dbus_message_path_is_our_session (listener, message)) {
            DBusError error;
            dbus_bool_t new_active;

            dbus_error_init (&error);
            if (dbus_message_get_args (message, &error,
                                       DBUS_TYPE_BOOLEAN, &new_active,
                                       DBUS_TYPE_INVALID)) {
                gs_debug ("ConsoleKit notified ActiveChanged %d", new_active);

                /* when we aren't active add an implicit throttle from CK
                 * when we become active remove the throttle and poke the lock */
                if (new_active) {
                    if (listener->priv->ck_throttle_cookie != 0) {
                        listener_remove_ck_ref_entry (listener,
                                                      REF_ENTRY_TYPE_THROTTLE,
                                                      listener->priv->ck_throttle_cookie);
                        listener->priv->ck_throttle_cookie = 0;
                    }

                    g_signal_emit (listener, signals[SIMULATE_USER_ACTIVITY], 0);
                } else {
                    if (listener->priv->ck_throttle_cookie != 0) {
                        g_warning ("ConsoleKit throttle already set");
                        listener_remove_ck_ref_entry (listener,
                                                      REF_ENTRY_TYPE_THROTTLE,
                                                      listener->priv->ck_throttle_cookie);
                        listener->priv->ck_throttle_cookie = 0;
                    }

                    listener_add_ck_ref_entry (listener,
                                               REF_ENTRY_TYPE_THROTTLE,
                                               connection,
                                               message,
                                               &listener->priv->ck_throttle_cookie);
                }
            }

            if (dbus_error_is_set (&error)) {
                dbus_error_free (&error);
            }
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }
#endif
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
gs_listener_dbus_message_handler (DBusConnection *connection,
                                  DBusMessage *message,
                                  void *user_data) {
    g_return_val_if_fail (connection != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
    g_return_val_if_fail (message != NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

#if 0
    g_message ("obj_path=%s interface=%s method=%s destination=%s",
               dbus_message_get_path (message),
               dbus_message_get_interface (message),
               dbus_message_get_member (message),
               dbus_message_get_destination (message));
#endif

    if (dbus_message_is_method_call (message, "org.freedesktop.DBus", "AddMatch")) {
        DBusMessage *reply;

        reply = dbus_message_new_method_return (message);

        if (reply == NULL) {
            g_error ("No memory");
        }

        if (!dbus_connection_send (connection, reply, NULL)) {
            g_error ("No memory");
        }

        dbus_message_unref (reply);

        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected")
               && strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
        dbus_connection_unref (connection);

        return DBUS_HANDLER_RESULT_HANDLED;
    } else {
        return listener_dbus_handle_session_message (connection, message, user_data, TRUE);
    }
}

static gboolean
gs_listener_dbus_dbus_init (GSListenerDBus *listener) {
    DBusError error;

    dbus_error_init (&error);

    if (listener->priv->connection == NULL) {
        listener->priv->connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
        if (listener->priv->connection == NULL) {
            if (dbus_error_is_set (&error)) {
                gs_debug ("Couldn't connect to session bus: %s",
                          error.message);
                dbus_error_free (&error);
            }
            return FALSE;
        }

        dbus_connection_setup_with_g_main (listener->priv->connection, NULL);
        dbus_connection_set_exit_on_disconnect (listener->priv->connection, FALSE);
    }

    if (listener->priv->system_connection == NULL) {
        listener->priv->system_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
        if (listener->priv->system_connection == NULL) {
            if (dbus_error_is_set (&error)) {
                gs_debug ("Couldn't connect to system bus: %s",
                          error.message);
                dbus_error_free (&error);
            }
            return FALSE;
        }

        dbus_connection_setup_with_g_main (listener->priv->system_connection, NULL);
        dbus_connection_set_exit_on_disconnect (listener->priv->system_connection, FALSE);
    }

    return TRUE;
}

static gboolean
reinit_dbus (gpointer user_data) {
    GSListenerDBus *listener = user_data;
    gboolean initialized;
    gboolean try_again;

    initialized = gs_listener_dbus_dbus_init (listener);

    /* if we didn't initialize then try again */
    /* FIXME: Should we keep trying forever?  If we fail more than
       once or twice then the session bus may have died.  The
       problem is that if it is restarted it will likely have a
       different bus address and we won't be able to find it */
    try_again = !initialized;

    return try_again;
}

static DBusHandlerResult
listener_dbus_filter_function (DBusConnection *connection,
                               DBusMessage *message,
                               void *user_data) {
    GSListenerDBus *listener = GS_LISTENER_DBUS (user_data);
    const char *path;

    path = dbus_message_get_path (message);

    /*
    g_message ("obj_path=%s interface=%s method=%s",
               dbus_message_get_path (message),
               dbus_message_get_interface (message),
               dbus_message_get_member (message));
    */

    if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected")
        && strcmp (path, DBUS_PATH_LOCAL) == 0) {
        g_message ("Got disconnected from the session message bus; "
                   "retrying to reconnect every 10 seconds");

        dbus_connection_unref (connection);
        listener->priv->connection = NULL;

        g_timeout_add_seconds (10, reinit_dbus, listener);
    } else if (dbus_message_is_signal (message,
                                       DBUS_INTERFACE_DBUS,
                                       "NameOwnerChanged")) {
        if (listener->priv->inhibitors != NULL) {
            listener_service_deleted (listener, message);
        }
    } else {
        return listener_dbus_handle_session_message (connection, message, user_data, FALSE);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
listener_dbus_system_filter_function (DBusConnection *connection,
                                      DBusMessage *message,
                                      void *user_data) {
    GSListenerDBus *listener = GS_LISTENER_DBUS (user_data);
    const char *path;

    path = dbus_message_get_path (message);

    if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected")
        && strcmp (path, DBUS_PATH_LOCAL) == 0) {
        g_message ("Got disconnected from the system message bus; "
                   "retrying to reconnect every 10 seconds");

        dbus_connection_unref (connection);
        listener->priv->system_connection = NULL;

        g_timeout_add_seconds (10, reinit_dbus, listener);
    } else {
        return listener_dbus_handle_system_message (connection, message, user_data, FALSE);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static void
gs_listener_dbus_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec) {
    GSListenerDBus *self;

    self = GS_LISTENER_DBUS (object);

    switch (prop_id) {
        case PROP_ACTIVE:
            gs_listener_dbus_activate_saver (self, g_value_get_boolean (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_listener_dbus_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec) {
    GSListenerDBus *self;

    self = GS_LISTENER_DBUS (object);

    switch (prop_id) {
        case PROP_ACTIVE:
            g_value_set_boolean (value, self->priv->active);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gs_listener_dbus_class_init (GSListenerDBusClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gs_listener_dbus_finalize;
    object_class->get_property = gs_listener_dbus_get_property;
    object_class->set_property = gs_listener_dbus_set_property;

    signals[LOCK] =
        g_signal_new ("lock",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, lock),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[QUIT] =
        g_signal_new ("quit",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, quit),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[CYCLE] =
        g_signal_new ("cycle",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, cycle),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[SIMULATE_USER_ACTIVITY] =
        g_signal_new ("simulate-user-activity",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, simulate_user_activity),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    signals[ACTIVE_CHANGED] =
        g_signal_new ("active-changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, active_changed),
                      NULL,
                      NULL,
                      gs_marshal_BOOLEAN__BOOLEAN,
                      G_TYPE_BOOLEAN,
                      1,
                      G_TYPE_BOOLEAN);
    signals[THROTTLE_CHANGED] =
        g_signal_new ("throttle-changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, throttle_changed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_BOOLEAN);
    signals[SHOW_MESSAGE] =
        g_signal_new ("show-message",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GSListenerDBusClass, show_message),
                      NULL,
                      NULL,
                      gs_marshal_VOID__STRING_STRING_STRING,
                      G_TYPE_NONE,
                      3,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      G_TYPE_STRING);

    g_object_class_install_property (object_class,
                                     PROP_ACTIVE,
                                     g_param_spec_boolean ("active",
                                                           NULL,
                                                           NULL,
                                                           FALSE,
                                                           G_PARAM_READWRITE));
}

static gboolean
screensaver_is_running (DBusConnection *connection) {
    DBusError error;
    gboolean exists;

    g_return_val_if_fail (connection != NULL, FALSE);

    dbus_error_init (&error);
    exists = dbus_bus_name_has_owner (connection, GS_LISTENER_DBUS_SERVICE, &error);
    if (dbus_error_is_set (&error)) {
        dbus_error_free (&error);
    }

    return exists;
}

gboolean
gs_listener_dbus_acquire (GSListenerDBus *listener,
                          GError **error) {
    int acquired;
    DBusError buserror;
    gboolean is_connected;

    g_return_val_if_fail (listener != NULL, FALSE);

    if (!listener->priv->connection) {
        g_set_error (error,
                     GS_LISTENER_DBUS_ERROR,
                     GS_LISTENER_DBUS_ERROR_ACQUISITION_FAILURE,
                     "%s",
                     _("failed to register with the message bus"));
        return FALSE;
    }

    is_connected = dbus_connection_get_is_connected (listener->priv->connection);
    if (!is_connected) {
        g_set_error (error,
                     GS_LISTENER_DBUS_ERROR,
                     GS_LISTENER_DBUS_ERROR_ACQUISITION_FAILURE,
                     "%s",
                     _("not connected to the message bus"));
        return FALSE;
    }

    if (screensaver_is_running (listener->priv->connection)) {
        g_set_error (error,
                     GS_LISTENER_DBUS_ERROR,
                     GS_LISTENER_DBUS_ERROR_ACQUISITION_FAILURE,
                     "%s",
                     _("screensaver already running in this session"));
        return FALSE;
    }

    dbus_error_init (&buserror);

    if (dbus_connection_register_object_path (listener->priv->connection,
                                              GS_LISTENER_DBUS_PATH,
                                              &gs_listener_dbus_vtable,
                                              listener)
        == FALSE) {
        g_critical ("out of memory registering object path");
        return FALSE;
    }

    acquired = dbus_bus_request_name (listener->priv->connection,
                                      GS_LISTENER_DBUS_SERVICE,
                                      DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                      &buserror);
    if (dbus_error_is_set (&buserror)) {
        g_set_error (error,
                     GS_LISTENER_DBUS_ERROR,
                     GS_LISTENER_DBUS_ERROR_ACQUISITION_FAILURE,
                     "%s",
                     buserror.message);
    }
    if (acquired == DBUS_REQUEST_NAME_REPLY_EXISTS) {
        g_set_error (error,
                     GS_LISTENER_DBUS_ERROR,
                     GS_LISTENER_DBUS_ERROR_ACQUISITION_FAILURE,
                     "%s",
                     _("screensaver already running in this session"));
        return FALSE;
    }

    dbus_error_free (&buserror);

    dbus_connection_add_filter (listener->priv->connection, listener_dbus_filter_function, listener, NULL);

    dbus_bus_add_match (listener->priv->connection,
                        "type='signal'"
                        ",interface='" DBUS_INTERFACE_DBUS "'"
                        ",sender='" DBUS_SERVICE_DBUS "'"
                        ",member='NameOwnerChanged'",
                        NULL);

    if (listener->priv->system_connection != NULL) {
        dbus_connection_add_filter (listener->priv->system_connection,
                                    listener_dbus_system_filter_function,
                                    listener,
                                    NULL);
#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
        if (listener->priv->have_logind) {
            dbus_bus_add_match (listener->priv->system_connection,
                                "type='signal'"
                                ",sender='" LOGIND_SERVICE "'"
                                ",interface='" LOGIND_SESSION_INTERFACE "'"
                                ",member='Unlock'",
                                NULL);
            dbus_bus_add_match (listener->priv->system_connection,
                                "type='signal'"
                                ",sender='" LOGIND_SERVICE "'"
                                ",interface='" LOGIND_SESSION_INTERFACE "'"
                                ",member='Lock'",
                                NULL);
            dbus_bus_add_match (listener->priv->system_connection,
                                "type='signal'"
                                ",sender='" LOGIND_SERVICE "'"
                                ",interface='" LOGIND_INTERFACE "'"
                                ",member='PrepareForSleep'",
                                NULL);
            dbus_bus_add_match (listener->priv->system_connection,
                                "type='signal'"
                                ",sender='" LOGIND_SERVICE "'"
                                ",interface='" DBUS_INTERFACE_PROPERTIES "'"
                                ",member='PropertiesChanged'",
                                NULL);

            return (acquired != -1);
        }
#endif

#ifdef WITH_CONSOLE_KIT
        dbus_bus_add_match (listener->priv->system_connection,
                            "type='signal'"
                            ",interface='" HAL_DEVICE_INTERFACE "'"
                            ",member='Condition'",
                            NULL);
        dbus_bus_add_match (listener->priv->system_connection,
                            "type='signal'"
                            ",interface='" CK_SESSION_INTERFACE "'"
                            ",member='Unlock'",
                            NULL);
        dbus_bus_add_match (listener->priv->system_connection,
                            "type='signal'"
                            ",interface='" CK_SESSION_INTERFACE "'"
                            ",member='Lock'",
                            NULL);
        dbus_bus_add_match (listener->priv->system_connection,
                            "type='signal'"
                            ",interface='" CK_SESSION_INTERFACE "'"
                            ",member='ActiveChanged'",
                            NULL);
#endif
    }

    return (acquired != -1);
}

static char *
query_session_id (GSListenerDBus *listener) {
    DBusMessage *message;
    DBusMessage *reply;
    DBusError error;
    DBusMessageIter reply_iter;
    char *ssid;

    if (listener->priv->system_connection == NULL) {
        gs_debug ("No connection to the system bus");
        return NULL;
    }

    ssid = NULL;

    dbus_error_init (&error);

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    if (listener->priv->have_logind) {
        dbus_uint32_t pid = getpid ();

        message = dbus_message_new_method_call (LOGIND_SERVICE,
                                                LOGIND_PATH,
                                                LOGIND_INTERFACE,
                                                "GetSessionByPID");
        if (message == NULL) {
            gs_debug ("Couldn't allocate the dbus message");
            return NULL;
        }

        if (dbus_message_append_args (message,
                                      DBUS_TYPE_UINT32,
                                      &pid, DBUS_TYPE_INVALID)
            == FALSE) {
            gs_debug ("Couldn't add args to the dbus message");
            return NULL;
        }

        /* FIXME: use async? */
        reply = dbus_connection_send_with_reply_and_block (listener->priv->system_connection,
                                                           message,
                                                           -1, &error);
        dbus_message_unref (message);

        if (dbus_error_is_set (&error)) {
            gs_debug ("%s raised:\n %s\n\n", error.name, error.message);
            dbus_error_free (&error);
            return NULL;
        }

        dbus_message_iter_init (reply, &reply_iter);
        dbus_message_iter_get_basic (&reply_iter, &ssid);

        dbus_message_unref (reply);

        return g_strdup (ssid);
    }
#endif

#ifdef WITH_CONSOLE_KIT
    message = dbus_message_new_method_call (CK_NAME, CK_MANAGER_PATH, CK_MANAGER_INTERFACE, "GetCurrentSession");
    if (message == NULL) {
        gs_debug ("Couldn't allocate the dbus message");
        return NULL;
    }

    /* FIXME: use async? */
    reply = dbus_connection_send_with_reply_and_block (listener->priv->system_connection,
                                                       message,
                                                       -1, &error);
    dbus_message_unref (message);

    if (dbus_error_is_set (&error)) {
        gs_debug ("%s raised:\n %s\n\n", error.name, error.message);
        dbus_error_free (&error);
        return NULL;
    }

    dbus_message_iter_init (reply, &reply_iter);
    dbus_message_iter_get_basic (&reply_iter, &ssid);

    dbus_message_unref (reply);

    return g_strdup (ssid);
#else
    return NULL;
#endif
}

static void
init_session_id (GSListenerDBus *listener) {
    g_free (listener->priv->session_id);
    listener->priv->session_id = query_session_id (listener);
    gs_debug ("Got session-id: %s", listener->priv->session_id);
}

static void
gs_listener_dbus_init (GSListenerDBus *listener) {
    listener->priv = gs_listener_dbus_get_instance_private (listener);

#if defined(WITH_SYSTEMD) || defined(WITH_ELOGIND)
    /* check if logind is running */
    listener->priv->have_logind = (access ("/run/systemd/seats/", F_OK) >= 0);
#endif
    listener->priv->prefs = gs_prefs_new ();

    gs_listener_dbus_dbus_init (listener);

    init_session_id (listener);

    listener->priv->inhibitors = g_hash_table_new_full (g_int_hash,
                                                        g_int_equal,
                                                        NULL,
                                                        (GDestroyNotify) gs_listener_dbus_ref_entry_free);
    listener->priv->throttlers = g_hash_table_new_full (g_int_hash,
                                                        g_int_equal,
                                                        NULL,
                                                        (GDestroyNotify) gs_listener_dbus_ref_entry_free);

    gs_debug ("Acquiring logind sleep inhibitor lock");
    add_sleep_inhibit (listener);
}

static void
gs_listener_dbus_finalize (GObject *object) {
    GSListenerDBus *listener;

    g_return_if_fail (object != NULL);
    g_return_if_fail (GS_IS_LISTENER_DBUS (object));

    listener = GS_LISTENER_DBUS (object);

    g_return_if_fail (listener->priv != NULL);

    if (listener->priv->inhibitors) {
        g_hash_table_destroy (listener->priv->inhibitors);
    }

    if (listener->priv->throttlers) {
        g_hash_table_destroy (listener->priv->throttlers);
    }

    g_free (listener->priv->session_id);
    remove_sleep_inhibit (listener);
    g_object_unref (listener->priv->prefs);

    G_OBJECT_CLASS (gs_listener_dbus_parent_class)->finalize (object);
}

GSListenerDBus *
gs_listener_dbus_new (void) {
    GSListenerDBus *listener;

    listener = g_object_new (GS_TYPE_LISTENER_DBUS, NULL);

    return GS_LISTENER_DBUS (listener);
}
