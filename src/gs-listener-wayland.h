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

#ifndef GS_LISTENER_WAYLAND_H_
#define GS_LISTENER_WAYLAND_H_

#include "gs-listener.h"

G_BEGIN_DECLS

#define GS_TYPE_LISTENER_WAYLAND (gs_listener_wayland_get_type ())
G_DECLARE_FINAL_TYPE (GSListenerWayland, gs_listener_wayland, GS, LISTENER_WAYLAND, GSListener)

G_END_DECLS

#endif /* GS_LISTENER_WAYLAND_H_ */
