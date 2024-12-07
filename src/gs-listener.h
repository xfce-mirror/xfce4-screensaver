/*
 * Copyright (C) 2024 Gaël Bonithon <gael@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef SRC_GS_LISTENER_H_
#define SRC_GS_LISTENER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define GS_TYPE_LISTENER (gs_listener_get_type ())
G_DECLARE_DERIVABLE_TYPE (GSListener, gs_listener, GS, LISTENER, GObject)

struct _GSListenerClass
{
    GObjectClass parent_class;

    void       (*set_timeouts)          (GSListener    *listener,
                                         gboolean       enabled,
                                         guint          timeout,
                                         guint          lock_timeout);
};

GSListener *gs_listener_new (void);

G_END_DECLS

#endif /* SRC_GS_LISTENER_H_ */
