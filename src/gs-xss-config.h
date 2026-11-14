/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 John Audia <therealgraysky@proton.me>
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
 */

#ifndef SRC_GS_XSS_CONFIG_H_
#define SRC_GS_XSS_CONFIG_H_

#include <glib.h>

G_BEGIN_DECLS

/* One entry of the xscreensaver "programs" resource */
typedef struct {
    gchar *command; /* shell command, visual and label stripped; NULL if empty */
    gboolean enabled; /* FALSE if the entry was prefixed with '-' */
} GSXssProgram;

/* The subset of an xscreensaver config that matters for picking a hack */
typedef struct {
    gchar *mode; /* "random", "random-same", "one", "blank", "off" or NULL */
    gint selected; /* index into programs for mode "one", -1 if unset */
    GPtrArray *programs; /* GSXssProgram, in file order */
} GSXssConfig;

typedef gboolean (*GSXssAvailableFunc) (const gchar *command,
                                        gpointer user_data);

GSXssConfig *
gs_xss_config_parse (const gchar *contents);
void
gs_xss_config_merge (GSXssConfig *config,
                     const GSXssConfig *defaults);
void
gs_xss_config_free (GSXssConfig *config);
GSXssConfig *
gs_xss_config_load (void);
gboolean
gs_xss_config_command_available (const gchar *command,
                                 gpointer user_data);
const gchar *
gs_xss_config_pick (const GSXssConfig *config,
                    const gchar *previous,
                    GSXssAvailableFunc available,
                    gpointer user_data);
gchar *
gs_xss_config_wrap_command (const gchar *command);
gchar *
gs_xss_config_get_command (void);

G_END_DECLS

#endif /* SRC_GS_XSS_CONFIG_H_ */
