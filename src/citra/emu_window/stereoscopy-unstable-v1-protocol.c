/*
 * Copyright © 2016 Emmanuel Gil Peyrot
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface zwp_stereoscopy_description_v1_interface;

static const struct wl_interface *types[] = {
	NULL,
	&wl_surface_interface,
	&zwp_stereoscopy_description_v1_interface,
	&wl_output_interface,
	NULL,
};

static const struct wl_message zwp_stereoscopy_v1_requests[] = {
	{ "destroy", "", types + 0 },
	{ "create_description", "on", types + 1 },
};

static const struct wl_message zwp_stereoscopy_v1_events[] = {
	{ "layout", "ou", types + 3 },
};

WL_EXPORT const struct wl_interface zwp_stereoscopy_v1_interface = {
	"zwp_stereoscopy_v1", 1,
	2, zwp_stereoscopy_v1_requests,
	1, zwp_stereoscopy_v1_events,
};

static const struct wl_message zwp_stereoscopy_description_v1_requests[] = {
	{ "destroy", "", types + 0 },
	{ "set_layout", "u", types + 0 },
	{ "set_default_side", "u", types + 0 },
};

WL_EXPORT const struct wl_interface zwp_stereoscopy_description_v1_interface = {
	"zwp_stereoscopy_description_v1", 1,
	3, zwp_stereoscopy_description_v1_requests,
	0, NULL,
};

