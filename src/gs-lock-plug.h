/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2005 William Jon McCann <mccann@jhu.edu>
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

#ifndef SRC_GS_LOCK_PLUG_H_
#define SRC_GS_LOCK_PLUG_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GS_LOCK_PLUG_RESPONSE_NONE = -1,
    GS_LOCK_PLUG_RESPONSE_OK = -2,
    GS_LOCK_PLUG_RESPONSE_CANCEL = -3
} GSPlugResponseType;

#define GS_TYPE_LOCK_PLUG (gs_lock_plug_get_type ())
#define GS_LOCK_PLUG(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_LOCK_PLUG, GSLockPlug))
#define GS_LOCK_PLUG_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GS_TYPE_LOCK_PLUG, GSLockPlugClass))
#define GS_IS_LOCK_PLUG(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_LOCK_PLUG))
#define GS_IS_LOCK_PLUG_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_LOCK_PLUG))
#define GS_LOCK_PLUG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_LOCK_PLUG, GSLockPlugClass))

typedef struct GSLockPlugPrivate GSLockPlugPrivate;

typedef struct
{
    GObject parent;

    GSLockPlugPrivate *priv;
} GSLockPlug;

typedef struct
{
    GObjectClass parent_class;

    void (*response) (GSLockPlug *plug, gint response_id);
} GSLockPlugClass;

GType
gs_lock_plug_get_type (void);
GSLockPlug *
gs_lock_plug_new (gboolean logout_enabled,
                  const gchar *logout_command,
                  gboolean switch_enabled,
                  const gchar *status_message,
                  gint monitor_index);
GtkWidget *
gs_lock_plug_get_widget (GSLockPlug *plug);

int
gs_lock_plug_run (GSLockPlug *plug);
void
gs_lock_plug_set_sensitive (GSLockPlug *plug,
                            gboolean sensitive);
void
gs_lock_plug_enable_prompt (GSLockPlug *plug,
                            const char *message,
                            gboolean visible);
void
gs_lock_plug_disable_prompt (GSLockPlug *plug);
void
gs_lock_plug_set_busy (GSLockPlug *plug);
void
gs_lock_plug_set_ready (GSLockPlug *plug);

void
gs_lock_plug_get_text (GSLockPlug *plug,
                       char **text);
void
gs_lock_plug_show_message (GSLockPlug *plug,
                           const char *message);

G_END_DECLS

#endif /* SRC_GS_LOCK_PLUG_H_ */
