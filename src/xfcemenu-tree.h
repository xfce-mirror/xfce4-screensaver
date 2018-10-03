/*
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __MATEMENU_TREE_H__
#define __MATEMENU_TREE_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XfceMenuTree          XfceMenuTree;
typedef struct XfceMenuTreeItem      XfceMenuTreeItem;
typedef struct XfceMenuTreeDirectory XfceMenuTreeDirectory;
typedef struct XfceMenuTreeEntry     XfceMenuTreeEntry;
typedef struct XfceMenuTreeSeparator XfceMenuTreeSeparator;
typedef struct XfceMenuTreeHeader    XfceMenuTreeHeader;
typedef struct XfceMenuTreeAlias     XfceMenuTreeAlias;

typedef void (*XfceMenuTreeChangedFunc) (XfceMenuTree* tree, gpointer user_data);

typedef enum {
	MATEMENU_TREE_ITEM_INVALID = 0,
	MATEMENU_TREE_ITEM_DIRECTORY,
	MATEMENU_TREE_ITEM_ENTRY,
	MATEMENU_TREE_ITEM_SEPARATOR,
	MATEMENU_TREE_ITEM_HEADER,
	MATEMENU_TREE_ITEM_ALIAS
} XfceMenuTreeItemType;

#define MATEMENU_TREE_ITEM(i)      ((XfceMenuTreeItem*)(i))
#define MATEMENU_TREE_DIRECTORY(i) ((XfceMenuTreeDirectory*)(i))
#define MATEMENU_TREE_ENTRY(i)     ((XfceMenuTreeEntry*)(i))
#define MATEMENU_TREE_SEPARATOR(i) ((XfceMenuTreeSeparator*)(i))
#define MATEMENU_TREE_HEADER(i)    ((XfceMenuTreeHeader*)(i))
#define MATEMENU_TREE_ALIAS(i)     ((XfceMenuTreeAlias*)(i))

typedef enum {
	MATEMENU_TREE_FLAGS_NONE                = 0,
	MATEMENU_TREE_FLAGS_INCLUDE_EXCLUDED    = 1 << 0,
	MATEMENU_TREE_FLAGS_SHOW_EMPTY          = 1 << 1,
	MATEMENU_TREE_FLAGS_INCLUDE_NODISPLAY   = 1 << 2,
	MATEMENU_TREE_FLAGS_SHOW_ALL_SEPARATORS = 1 << 3,
	MATEMENU_TREE_FLAGS_MASK                = 0x0f
} XfceMenuTreeFlags;

typedef enum {
	#define MATEMENU_TREE_SORT_FIRST MATEMENU_TREE_SORT_NAME
	MATEMENU_TREE_SORT_NAME = 0,
	MATEMENU_TREE_SORT_DISPLAY_NAME
	#define MATEMENU_TREE_SORT_LAST MATEMENU_TREE_SORT_DISPLAY_NAME
} XfceMenuTreeSortKey;

XfceMenuTree* xfcemenu_tree_lookup(const char* menu_file, XfceMenuTreeFlags flags);

void xfcemenu_tree_unref(XfceMenuTree* tree);

XfceMenuTreeDirectory* xfcemenu_tree_get_root_directory(XfceMenuTree* tree);

void xfcemenu_tree_item_unref(gpointer item);

XfceMenuTreeItemType xfcemenu_tree_item_get_type(XfceMenuTreeItem* item);


GSList* xfcemenu_tree_directory_get_contents(XfceMenuTreeDirectory* directory);

const char* xfcemenu_tree_entry_get_name(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_exec(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_desktop_file_id(XfceMenuTreeEntry* entry);

#ifdef __cplusplus
}
#endif

#endif /* __MATEMENU_TREE_H__ */
