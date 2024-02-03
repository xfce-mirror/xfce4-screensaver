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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef SRC_GS_SESSION_LOCK_MANAGER_H_
#define SRC_GS_SESSION_LOCK_MANAGER_H_

#include <glib-object.h>
#include "gs-window.h"

G_BEGIN_DECLS

#define GS_TYPE_SESSION_LOCK_MANAGER (gs_session_lock_manager_get_type ())
G_DECLARE_FINAL_TYPE (GSSessionLockManager, gs_session_lock_manager, GS, SESSION_LOCK_MANAGER, GObject)

GSSessionLockManager    *gs_session_lock_manager_new            (void);
gboolean                 gs_session_lock_manager_lock           (GSSessionLockManager     *manager);
void                     gs_session_lock_manager_unlock         (GSSessionLockManager     *manager);
void                     gs_session_lock_manager_add_window     (GSSessionLockManager     *manager,
                                                                 GSWindow                 *window);
void                     gs_session_lock_manager_remove_window  (GSSessionLockManager     *manager,
                                                                 GSWindow                 *window);

G_END_DECLS

#endif /* SRC_GS_SESSION_LOCK_MANAGER_H_ */
