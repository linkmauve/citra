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

#ifndef STEREOSCOPY_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define STEREOSCOPY_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct wl_output;
struct wl_surface;
struct zwp_stereoscopy_description_v1;
struct zwp_stereoscopy_v1;

extern const struct wl_interface zwp_stereoscopy_v1_interface;
extern const struct wl_interface zwp_stereoscopy_description_v1_interface;

#ifndef ZWP_STEREOSCOPY_V1_ERROR_ENUM
#define ZWP_STEREOSCOPY_V1_ERROR_ENUM
enum zwp_stereoscopy_v1_error {
	ZWP_STEREOSCOPY_V1_ERROR_STEREOSCOPY_DESCRIPTION_EXISTS = 0,
};
#endif /* ZWP_STEREOSCOPY_V1_ERROR_ENUM */

/**
 * zwp_stereoscopy_v1 - factory for creating stereoscopy descriptors
 * @layout: supported stereoscopic layout
 *
 * TODO
 */
struct zwp_stereoscopy_v1_listener {
	/**
	 * layout - supported stereoscopic layout
	 * @output: the output supporting this specific stereoscopy
	 *	layout
	 * @layout: a layout supported by this output
	 *
	 * This event advertises one stereoscopic layout that the output
	 * supports. All the supported layouts are advertised once when the
	 * client binds to this interface. A roundtrip after binding
	 * guarantees that the client has received all supported formats.
	 *
	 * For the definition of the layout codes, see create request.
	 *
	 * TODO: should we send one for none?
	 */
	void (*layout)(void *data,
		       struct zwp_stereoscopy_v1 *zwp_stereoscopy_v1,
		       struct wl_output *output,
		       uint32_t layout);
};

static inline int
zwp_stereoscopy_v1_add_listener(struct zwp_stereoscopy_v1 *zwp_stereoscopy_v1,
				const struct zwp_stereoscopy_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwp_stereoscopy_v1,
				     (void (**)(void)) listener, data);
}

#define ZWP_STEREOSCOPY_V1_DESTROY	0
#define ZWP_STEREOSCOPY_V1_CREATE_DESCRIPTION	1

#define ZWP_STEREOSCOPY_V1_DESTROY_SINCE_VERSION	1
#define ZWP_STEREOSCOPY_V1_CREATE_DESCRIPTION_SINCE_VERSION	1

static inline void
zwp_stereoscopy_v1_set_user_data(struct zwp_stereoscopy_v1 *zwp_stereoscopy_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_stereoscopy_v1, user_data);
}

static inline void *
zwp_stereoscopy_v1_get_user_data(struct zwp_stereoscopy_v1 *zwp_stereoscopy_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_stereoscopy_v1);
}

static inline void
zwp_stereoscopy_v1_destroy(struct zwp_stereoscopy_v1 *zwp_stereoscopy_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_stereoscopy_v1,
			 ZWP_STEREOSCOPY_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_stereoscopy_v1);
}

static inline struct zwp_stereoscopy_description_v1 *
zwp_stereoscopy_v1_create_description(struct zwp_stereoscopy_v1 *zwp_stereoscopy_v1, struct wl_surface *surface)
{
	struct wl_proxy *stereoscopy_description_id;

	stereoscopy_description_id = wl_proxy_marshal_constructor((struct wl_proxy *) zwp_stereoscopy_v1,
			 ZWP_STEREOSCOPY_V1_CREATE_DESCRIPTION, &zwp_stereoscopy_description_v1_interface, surface, NULL);

	return (struct zwp_stereoscopy_description_v1 *) stereoscopy_description_id;
}

#ifndef ZWP_STEREOSCOPY_DESCRIPTION_V1_ERROR_ENUM
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_ERROR_ENUM
enum zwp_stereoscopy_description_v1_error {
	ZWP_STEREOSCOPY_DESCRIPTION_V1_ERROR_INVALID_LAYOUT = 0,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_ERROR_INVALID_DEFAULT_SIDE = 1,
};
#endif /* ZWP_STEREOSCOPY_DESCRIPTION_V1_ERROR_ENUM */

#ifndef ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_ENUM
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_ENUM
/**
 * zwp_stereoscopy_description_v1_layout - list of possible stereoscopy
 *	layouts
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_NONE: default one, single image
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_SIDE_BY_SIDE: two half-width
 *	images, side-by-side, in a same buffer
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_TOP_BOTTOM: two half-height
 *	images, top-bottom, in a same buffer
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_FRAME_PACKING: TODO: something
 *	top-bottom with a border on the middle?
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_TWICE_AS_MANY_DMABUF: TODO: is
 *	this the right place for that?
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_QUAD_BUFFERING: TODO:
 *	shouldn’t this be implemented with the previous one?
 *
 * TODO
 */
enum zwp_stereoscopy_description_v1_layout {
	ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_NONE = 0,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_SIDE_BY_SIDE = 1,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_TOP_BOTTOM = 2,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_FRAME_PACKING = 3,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_TWICE_AS_MANY_DMABUF = 4,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_QUAD_BUFFERING = 5,
};
#endif /* ZWP_STEREOSCOPY_DESCRIPTION_V1_LAYOUT_ENUM */

#ifndef ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_ENUM
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_ENUM
/**
 * zwp_stereoscopy_description_v1_side - list of sides
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_DEFAULT: default one, as
 *	preferred by the compositor
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_LEFT: left side
 * @ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_RIGHT: right side
 *
 * TODO
 */
enum zwp_stereoscopy_description_v1_side {
	ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_DEFAULT = 0,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_LEFT = 1,
	ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_RIGHT = 2,
};
#endif /* ZWP_STEREOSCOPY_DESCRIPTION_V1_SIDE_ENUM */

#define ZWP_STEREOSCOPY_DESCRIPTION_V1_DESTROY	0
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_SET_LAYOUT	1
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_SET_DEFAULT_SIDE	2

#define ZWP_STEREOSCOPY_DESCRIPTION_V1_DESTROY_SINCE_VERSION	1
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_SET_LAYOUT_SINCE_VERSION	1
#define ZWP_STEREOSCOPY_DESCRIPTION_V1_SET_DEFAULT_SIDE_SINCE_VERSION	1

static inline void
zwp_stereoscopy_description_v1_set_user_data(struct zwp_stereoscopy_description_v1 *zwp_stereoscopy_description_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwp_stereoscopy_description_v1, user_data);
}

static inline void *
zwp_stereoscopy_description_v1_get_user_data(struct zwp_stereoscopy_description_v1 *zwp_stereoscopy_description_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwp_stereoscopy_description_v1);
}

static inline void
zwp_stereoscopy_description_v1_destroy(struct zwp_stereoscopy_description_v1 *zwp_stereoscopy_description_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_stereoscopy_description_v1,
			 ZWP_STEREOSCOPY_DESCRIPTION_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zwp_stereoscopy_description_v1);
}

static inline void
zwp_stereoscopy_description_v1_set_layout(struct zwp_stereoscopy_description_v1 *zwp_stereoscopy_description_v1, uint32_t layout)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_stereoscopy_description_v1,
			 ZWP_STEREOSCOPY_DESCRIPTION_V1_SET_LAYOUT, layout);
}

static inline void
zwp_stereoscopy_description_v1_set_default_side(struct zwp_stereoscopy_description_v1 *zwp_stereoscopy_description_v1, uint32_t default_side)
{
	wl_proxy_marshal((struct wl_proxy *) zwp_stereoscopy_description_v1,
			 ZWP_STEREOSCOPY_DESCRIPTION_V1_SET_DEFAULT_SIDE, default_side);
}

#ifdef  __cplusplus
}
#endif

#endif
