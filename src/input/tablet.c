// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "input/cursor.h"
#include "input/tablet.h"

static bool
tool_supports_absolute_motion(struct wlr_tablet_tool *tool)
{
	switch (tool->type) {
	case WLR_TABLET_TOOL_TYPE_MOUSE:
	case WLR_TABLET_TOOL_TYPE_LENS:
		return false;
	default:
		return true;
	}
}

static void
adjust_for_tablet_area(double tablet_width, double tablet_height,
		struct wlr_fbox box, double *x, double *y)
{
	if ((!box.x && !box.y && !box.width && !box.height)
			|| !tablet_width || !tablet_height) {
		return;
	}

	if (!box.width) {
		box.width = tablet_width - box.x;
	}
	if (!box.height) {
		box.height = tablet_height - box.y;
	}

	if (box.x + box.width <= tablet_width) {
		const double max_x = 1;
		double width_offset = max_x * box.x / tablet_width;
		*x = (*x - width_offset) * tablet_width / box.width;
	}
	if (box.y + box.height <= tablet_height) {
		const double max_y = 1;
		double height_offset = max_y * box.y / tablet_height;
		*y = (*y - height_offset) * tablet_height / box.height;
	}
}

static void
adjust_for_rotation(enum rotation rotation, double *x, double *y)
{
	double tmp;
	switch (rotation) {
	case LAB_ROTATE_NONE:
		break;
	case LAB_ROTATE_90:
		tmp = *x;
		*x = 1.0 - *y;
		*y = tmp;
		break;
	case LAB_ROTATE_180:
		*x = 1.0 - *x;
		*y = 1.0 - *y;
		break;
	case LAB_ROTATE_270:
		tmp = *x;
		*x = *y;
		*y = 1.0 - tmp;
		break;
	}
}

static void
handle_proximity(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_proximity_event *ev = data;

	if (!tool_supports_absolute_motion(ev->tool)) {
		if (ev->state == WLR_TABLET_TOOL_PROXIMITY_IN) {
			wlr_log(WLR_INFO, "ignoring not supporting tablet tool");
		}
	}
}

static void
handle_axis(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_axis_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;

	if (!tool_supports_absolute_motion(ev->tool)) {
		return;
	}

	if (ev->updated_axes & (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
		if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_X) {
			tablet->x = ev->x;
		}
		if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y) {
			tablet->y = ev->y;
		}

		double x = tablet->x;
		double y = tablet->y;
		adjust_for_tablet_area(tablet->tablet->width_mm, tablet->tablet->height_mm,
			rc.tablet.box, &x, &y);
		adjust_for_rotation(rc.tablet.rotation, &x, &y);
		cursor_emulate_move_absolute(tablet->seat, &ev->tablet->base, x, y, ev->time_msec);
	}
	// Ignore other events
}

static void
handle_tip(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_tip_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;

	uint32_t button = tablet_get_mapped_button(BTN_TOOL_PEN);
	if (!button) {
		return;
	}

	cursor_emulate_button(tablet->seat,
		button,
		ev->state == WLR_TABLET_TOOL_TIP_DOWN
			? WLR_BUTTON_PRESSED
			: WLR_BUTTON_RELEASED,
		ev->time_msec);
}

static void
handle_button(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_button_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;

	uint32_t button = tablet_get_mapped_button(ev->button);
	if (!button) {
		return;
	}

	cursor_emulate_button(tablet->seat, button, ev->state, ev->time_msec);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet *tablet =
		wl_container_of(listener, tablet, handlers.destroy);

	wl_list_remove(&tablet->handlers.tip.link);
	wl_list_remove(&tablet->handlers.button.link);
	wl_list_remove(&tablet->handlers.proximity.link);
	wl_list_remove(&tablet->handlers.axis.link);
	wl_list_remove(&tablet->handlers.destroy.link);
	free(tablet);
}

void
tablet_init(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_DEBUG, "setting up tablet");
	struct drawing_tablet *tablet = znew(*tablet);
	tablet->seat = seat;
	tablet->tablet = wlr_tablet_from_input_device(wlr_device);
	tablet->tablet->data = tablet;
	tablet->x = 0.0;
	tablet->y = 0.0;
	wlr_log(WLR_INFO, "tablet dimensions: %.2fmm x %.2fmm",
		tablet->tablet->width_mm, tablet->tablet->height_mm);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, axis);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, proximity);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, tip);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, button);
	CONNECT_SIGNAL(wlr_device, &tablet->handlers, destroy);
}
