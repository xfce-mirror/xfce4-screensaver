/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu>
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
 */

#ifndef SRC_GS_AUTH_H_
#define SRC_GS_AUTH_H_

#include <glib.h>

G_BEGIN_DECLS

#ifdef HAVE_LINUX_PAM
#include <libintl.h>
#define pam_dgettext(msgid) dgettext ("Linux-PAM", msgid)
#else
#define pam_dgettext(msgid) (msgid)
#endif

typedef enum
{
    GS_AUTH_MESSAGE_PROMPT_ECHO_ON,
    GS_AUTH_MESSAGE_PROMPT_ECHO_OFF,
    GS_AUTH_MESSAGE_ERROR_MSG,
    GS_AUTH_MESSAGE_TEXT_INFO
} GSAuthMessageStyle;

typedef enum
{
    GS_AUTH_ERROR_GENERAL,
    GS_AUTH_ERROR_AUTH_ERROR,
    GS_AUTH_ERROR_USER_UNKNOWN,
    GS_AUTH_ERROR_AUTH_DENIED
} GSAuthError;

typedef gboolean  (* GSAuthMessageFunc) (GSAuthMessageStyle   style,
                                         const char          *msg,
                                         char               **response,
                                         gpointer             data);

#define GS_AUTH_ERROR gs_auth_error_quark ()

GQuark   gs_auth_error_quark (void);

void     gs_auth_set_verbose (gboolean verbose);
gboolean gs_auth_get_verbose (void);

gboolean gs_auth_priv_init   (void);
gboolean gs_auth_init        (void);
gboolean gs_auth_verify_user (const char         *username,
                              const char         *display,
                              GSAuthMessageFunc   func,
                              gpointer            data,
                              GError            **error);

G_END_DECLS

#endif /* SRC_GS_AUTH_H_ */
