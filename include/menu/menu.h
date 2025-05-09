/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_MENU_H
#define LABWC_MENU_H

#include <wayland-server.h>

/* forward declare arguments */
struct view;
struct server;
struct wl_list;
struct wlr_scene_tree;
struct wlr_scene_node;
struct scaled_font_buffer;

enum menuitem_type {
	LAB_MENU_ITEM = 0,
	LAB_MENU_SEPARATOR_LINE,
	LAB_MENU_TITLE,
};

struct menuitem {
	struct wl_list actions;
	char *text;
	char *icon_name;
	const char *arrow;
	struct menu *parent;
	struct menu *submenu;
	bool selectable;
	enum menuitem_type type;
	int native_width;
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *normal_tree;
	struct wlr_scene_tree *selected_tree;
	struct view *client_list_view;  /* used by internal client-list */
	struct wl_list link; /* menu.menuitems */
};

/* This could be the root-menu or a submenu */
struct menu {
	char *id;
	char *label;
	char *icon_name;
	char *execute;
	struct menu *parent;
	struct menu_pipe_context *pipe_ctx;

	struct {
		int width;
		int height;
	} size;
	struct wl_list menuitems;
	struct server *server;
	struct {
		struct menu *menu;
		struct menuitem *item;
	} selection;
	struct wlr_scene_tree *scene_tree;
	bool is_pipemenu_child;
	bool align_left;
	bool has_icons;

	/* Used to match a window-menu to the view that triggered it. */
	struct view *triggered_by_view;  /* may be NULL */
	struct wl_list link; /* server.menus */
};

/* For keyboard support */
void menu_item_select_next(struct server *server);
void menu_item_select_previous(struct server *server);
void menu_submenu_enter(struct server *server);
void menu_submenu_leave(struct server *server);
bool menu_call_selected_actions(struct server *server);

void menu_init(struct server *server);
void menu_finish(struct server *server);
void menu_on_view_destroy(struct view *view);

/**
 * menu_get_by_id - get menu by id
 *
 * @id id string defined in menu.xml like "root-menu"
 */
struct menu *menu_get_by_id(struct server *server, const char *id);

/**
 * menu_open_root - open menu on position (x, y)
 *
 * This function will close server->menu_current, open the
 * new menu and assign @menu to server->menu_current.
 *
 * Additionally, server->input_mode will be set to LAB_INPUT_STATE_MENU.
 */
void menu_open_root(struct menu *menu, int x, int y);

/**
 * menu_process_cursor_motion
 *
 * - handles hover effects
 * - may open/close submenus
 */
void menu_process_cursor_motion(struct wlr_scene_node *node);

/**
 * menu_call_actions - call actions associated with a menu node
 *
 * If menuitem connected to @node does not just open a submenu:
 * - associated actions will be called
 * - server->menu_current will be closed
 * - server->menu_current will be set to NULL
 *
 * Returns true if actions have actually been executed
 */
bool menu_call_actions(struct wlr_scene_node *node);

/**
 *  menu_close_root- close root menu
 *
 * This function will close server->menu_current and set it to NULL.
 * Asserts that server->input_mode is set to LAB_INPUT_STATE_MENU.
 *
 * Additionally, server->input_mode will be set to LAB_INPUT_STATE_PASSTHROUGH.
 */
void menu_close_root(struct server *server);

/* menu_reconfigure - reload theme and content */
void menu_reconfigure(struct server *server);

#endif /* LABWC_MENU_H */
