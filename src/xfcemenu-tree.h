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

XfceMenuTree* xfcemenu_tree_ref(XfceMenuTree* tree);
void xfcemenu_tree_unref(XfceMenuTree* tree);

void xfcemenu_tree_set_user_data(XfceMenuTree* tree, gpointer user_data, GDestroyNotify dnotify);
gpointer xfcemenu_tree_get_user_data(XfceMenuTree* tree);

const char* xfcemenu_tree_get_menu_file(XfceMenuTree* tree);
XfceMenuTreeDirectory* xfcemenu_tree_get_root_directory(XfceMenuTree* tree);
XfceMenuTreeDirectory* xfcemenu_tree_get_directory_from_path(XfceMenuTree* tree, const char* path);

XfceMenuTreeSortKey xfcemenu_tree_get_sort_key(XfceMenuTree* tree);
void xfcemenu_tree_set_sort_key(XfceMenuTree* tree, XfceMenuTreeSortKey sort_key);



gpointer xfcemenu_tree_item_ref(gpointer item);
void xfcemenu_tree_item_unref(gpointer item);

void xfcemenu_tree_item_set_user_data(XfceMenuTreeItem* item, gpointer user_data, GDestroyNotify dnotify);
gpointer xfcemenu_tree_item_get_user_data(XfceMenuTreeItem* item);

XfceMenuTreeItemType xfcemenu_tree_item_get_type(XfceMenuTreeItem* item);
XfceMenuTreeDirectory* xfcemenu_tree_item_get_parent(XfceMenuTreeItem* item);


GSList* xfcemenu_tree_directory_get_contents(XfceMenuTreeDirectory* directory);
const char* xfcemenu_tree_directory_get_name(XfceMenuTreeDirectory* directory);
const char* xfcemenu_tree_directory_get_comment(XfceMenuTreeDirectory* directory);
const char* xfcemenu_tree_directory_get_icon(XfceMenuTreeDirectory* directory);
const char* xfcemenu_tree_directory_get_desktop_file_path(XfceMenuTreeDirectory* directory);
const char* xfcemenu_tree_directory_get_menu_id(XfceMenuTreeDirectory* directory);
XfceMenuTree* xfcemenu_tree_directory_get_tree(XfceMenuTreeDirectory* directory);

gboolean xfcemenu_tree_directory_get_is_nodisplay(XfceMenuTreeDirectory* directory);

char* xfcemenu_tree_directory_make_path(XfceMenuTreeDirectory* directory, XfceMenuTreeEntry* entry);


const char* xfcemenu_tree_entry_get_name(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_generic_name(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_display_name(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_comment(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_icon(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_exec(XfceMenuTreeEntry* entry);
gboolean xfcemenu_tree_entry_get_launch_in_terminal(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_desktop_file_path(XfceMenuTreeEntry* entry);
const char* xfcemenu_tree_entry_get_desktop_file_id(XfceMenuTreeEntry* entry);
gboolean xfcemenu_tree_entry_get_is_excluded(XfceMenuTreeEntry* entry);
gboolean xfcemenu_tree_entry_get_is_nodisplay(XfceMenuTreeEntry* entry);

XfceMenuTreeDirectory* xfcemenu_tree_header_get_directory(XfceMenuTreeHeader* header);

XfceMenuTreeDirectory* xfcemenu_tree_alias_get_directory(XfceMenuTreeAlias* alias);
XfceMenuTreeItem* xfcemenu_tree_alias_get_item(XfceMenuTreeAlias* alias);

void xfcemenu_tree_add_monitor(XfceMenuTree* tree, XfceMenuTreeChangedFunc callback, gpointer user_data);
void xfcemenu_tree_remove_monitor(XfceMenuTree* tree, XfceMenuTreeChangedFunc callback, gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif /* __MATEMENU_TREE_H__ */
