/* Headless: cc -std=c99 -Wall -Wextra -Werror -I. tests/test_ui_scale.c
 *             $(pkg-config --cflags --libs cairo) -o /tmp/test_ui_scale */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xmms/ui_scale.h"
#include "gtk_compat/cairo.h"

typedef struct
{
	XmmsUiWindow window;
	int doublesize, linked, scale;
} ScaleCase;

static const ScaleCase cases[] = {
	{ XMMS_UI_MAIN, 0, 0, 1 },
	{ XMMS_UI_MAIN, 0, 1, 1 },
	{ XMMS_UI_MAIN, 1, 0, 2 },
	{ XMMS_UI_MAIN, 1, 1, 2 },
	{ XMMS_UI_EQUALIZER, 0, 0, 1 },
	{ XMMS_UI_EQUALIZER, 0, 1, 1 },
	{ XMMS_UI_EQUALIZER, 1, 0, 1 },
	{ XMMS_UI_EQUALIZER, 1, 1, 2 },
	{ XMMS_UI_PLAYLIST, 0, 0, 1 },
	{ XMMS_UI_PLAYLIST, 0, 1, 1 },
	{ XMMS_UI_PLAYLIST, 1, 0, 1 },
	{ XMMS_UI_PLAYLIST, 1, 1, 1 }
};

static void test_geometry(const ScaleCase *c)
{
	int scale = xmms_ui_skin_scale(c->window, c->doublesize, c->linked);
	int width = c->window == XMMS_UI_PLAYLIST ? 375 : 275;
	int height = c->window == XMMS_UI_PLAYLIST ? 232 : 116;

	assert(scale == c->scale);
	assert(xmms_ui_to_logical(scale, width) == width * c->scale);
	assert(xmms_ui_to_logical(scale, height) == height * c->scale);
	assert(xmms_ui_to_logical(scale, 14) == 14 * c->scale);
}

static int hit(int scale, double logical_x, double logical_y,
	       int x, int y, int width, int height)
{
	double skin_x, skin_y;
	xmms_ui_to_skin_xy(scale, logical_x, logical_y, &skin_x, &skin_y);
	return skin_x >= x && skin_x < x + width &&
		skin_y >= y && skin_y < y + height;
}

static void test_rectangle(int scale, int x, int y, int width, int height)
{
	double left = xmms_ui_to_logical(scale, x);
	double top = xmms_ui_to_logical(scale, y);
	double right = xmms_ui_to_logical(scale, x + width);
	double bottom = xmms_ui_to_logical(scale, y + height);

	assert(hit(scale, left, top, x, y, width, height));
	assert(hit(scale, right - 0.25, bottom - 0.25, x, y, width, height));
	assert(!hit(scale, left - 0.25, top, x, y, width, height));
	assert(!hit(scale, left, top - 0.25, x, y, width, height));
	assert(!hit(scale, right, top, x, y, width, height));
	assert(!hit(scale, left, bottom, x, y, width, height));
}

static void test_hit_boundaries(const ScaleCase *c)
{
	int scale = xmms_ui_skin_scale(c->window, c->doublesize, c->linked);

	if (c->window == XMMS_UI_MAIN)
	{
		test_rectangle(scale, 39, 88, 23, 18); /* Play */
		test_rectangle(scale, 219, 58, 23, 12); /* EQ toggle */
		test_rectangle(scale, 177, 4, 10, 7); /* Shaded play */
	}
	else if (c->window == XMMS_UI_EQUALIZER)
	{
		test_rectangle(scale, 14, 18, 25, 12); /* On */
		test_rectangle(scale, 217, 18, 44, 12); /* Presets */
		test_rectangle(scale, 78, 38, 14, 63); /* First band slider */
		test_rectangle(scale, 61, 4, 97, 8); /* Shaded volume */
	}
	else
	{
		test_rectangle(scale, 12, 232 - 29, 25, 18); /* Add menu */
		test_rectangle(scale, 375 - 31, 0, 9, 14); /* Shaded resize */
		test_rectangle(scale, 375 - 11, 3, 9, 9); /* Close */
	}
	if (c->window != XMMS_UI_PLAYLIST)
		test_rectangle(scale, 264, 3, 9, 9); /* Close at right edge */
	test_rectangle(scale, 0, 0, 275, 14); /* Titlebar */
}

static void test_event_copy(const ScaleCase *c)
{
	/* Same copy-then-transform contract as press, release and motion handlers.
	 * Root coordinates deliberately do not correspond to a window origin. */
	struct PointerEvent
	{
		double x, y, x_root, y_root;
		unsigned int button, time, state;
	} original = { 79.5, 185.25, -1520.5, 900.75, 1, 1234, 4 };
	struct PointerEvent before = original;
	int scale = xmms_ui_skin_scale(c->window, c->doublesize, c->linked);
	int dispatch;

	for (dispatch = 0; dispatch < 3; dispatch++)
	{
		struct PointerEvent copy = original;
		xmms_ui_to_skin_xy(scale, original.x, original.y, &copy.x, &copy.y);
		assert(copy.x == 79.5 / c->scale);
		assert(copy.y == 185.25 / c->scale);
		assert(copy.x_root == before.x_root && copy.y_root == before.y_root);
		assert(copy.time == before.time && copy.state == before.state);
		assert(copy.button == before.button);
		assert(original.x == before.x && original.y == before.y);
	}
	/* Grabbed motions may be outside every edge; never normalize by bounds. */
	{
		double x, y;
		xmms_ui_to_skin_xy(scale, -0.5, 800.5, &x, &y);
		assert(x == -0.5 / c->scale && y == 800.5 / c->scale);
		xmms_ui_to_skin_xy(scale, 1100.5, -40.5, &x, &y);
		assert(x == 1100.5 / c->scale && y == -40.5 / c->scale);
	}
}

static uint32_t pixel(cairo_surface_t *surface, int x, int y)
{
	uint32_t value;
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	memcpy(&value, data + y * stride + x * 4, sizeof(value));
	return value;
}

static void test_render_and_input(const ScaleCase *c, int device_scale)
{
	static const uint32_t colors[] = {
		0xffff0000, 0xff00ff00, 0xff0000ff,
		0xffffff00, 0xff00ffff, 0xffff00ff
	};
	int scale = xmms_ui_skin_scale(c->window, c->doublesize, c->linked);
	int factor = c->scale * device_scale;
	int x, y;
	cairo_surface_t *skin = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 3, 2);
	cairo_surface_t *target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
							    3 * factor + 2, 2 * factor + 2);
	cairo_t *cr;
	cairo_matrix_t before, after;
	cairo_pattern_t *source;
	unsigned char *data = cairo_image_surface_get_data(skin);
	int stride = cairo_image_surface_get_stride(skin);

	for (y = 0; y < 2; y++)
		memcpy(data + y * stride, colors + y * 3, 3 * sizeof(uint32_t));
	cairo_surface_mark_dirty(skin);
	/* Models GDK_SCALE without gtk_init() or a display. */
	cairo_surface_set_device_scale(target, device_scale, device_scale);
	cr = cairo_create(target);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);
	source = cairo_get_source(cr);
	cairo_get_matrix(cr, &before);
	assert(xmms_cairo_paint_skin(cr, skin, scale));
	cairo_get_matrix(cr, &after);
	assert(before.xx == after.xx && before.yy == after.yy);
	assert(before.xy == after.xy && before.yx == after.yx);
	assert(before.x0 == after.x0 && before.y0 == after.y0);
	assert(source == cairo_get_source(cr));
	assert(cairo_get_operator(cr) == CAIRO_OPERATOR_OVER);
	cairo_surface_flush(target);

	for (y = 0; y < 2 * factor + 2; y++)
	{
		for (x = 0; x < 3 * factor + 2; x++)
		{
			if (x < 3 * factor && y < 2 * factor)
			{
				double skin_x, skin_y;
				/* GDK delivers logical coordinates even at device scale 2. */
				xmms_ui_to_skin_xy(scale, (x + 0.5) / device_scale,
					(y + 0.5) / device_scale, &skin_x, &skin_y);
				assert(pixel(target, x, y) == colors[(y / factor) * 3 + x / factor]);
				assert(pixel(target, x, y) == colors[(int) skin_y * 3 + (int) skin_x]);
			}
			else
				assert(pixel(target, x, y) == 0xffffffff);
		}
	}
	cairo_destroy(cr);
	cairo_surface_destroy(target);
	cairo_surface_destroy(skin);
}

static void test_cairo_state(void)
{
	cairo_surface_t *skin = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 2);
	cairo_surface_t *target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
	cairo_t *cr = cairo_create(skin);
	cairo_matrix_t before, after;
	double x1, y1, x2, y2;
	int x, y;

	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cr = cairo_create(target);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);
	cairo_translate(cr, 2, 3);
	cairo_scale(cr, 2, 1);
	cairo_rectangle(cr, 1, 1, 2, 2);
	cairo_clip(cr);
	cairo_get_matrix(cr, &before);
	assert(xmms_cairo_paint_skin(cr, skin, 2));
	cairo_get_matrix(cr, &after);
	assert(before.xx == after.xx && before.yy == after.yy);
	assert(before.xy == after.xy && before.yx == after.yx);
	assert(before.x0 == after.x0 && before.y0 == after.y0);
	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
	assert(x1 == 1 && y1 == 1 && x2 == 3 && y2 == 3);
	cairo_surface_flush(target);
	for (y = 0; y < 16; y++)
		for (x = 0; x < 16; x++)
			assert(pixel(target, x, y) ==
				(x >= 4 && x < 8 && y >= 4 && y < 6 ? 0xffff0000 : 0xffffffff));
	assert(!xmms_cairo_paint_skin(cr, skin, 0));
	cairo_destroy(cr);
	cairo_surface_destroy(target);
	cairo_surface_destroy(skin);
}

int main(void)
{
	size_t i;
	int device_scale;

	assert(!xmms_cairo_paint_skin(NULL, NULL, 1));
	test_cairo_state();
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		test_geometry(&cases[i]);
		test_hit_boundaries(&cases[i]);
		test_event_copy(&cases[i]);
		for (device_scale = 1; device_scale <= 3; device_scale++)
			test_render_and_input(&cases[i], device_scale);
	}
	puts("ui_scale: 12 window policies, hit boundaries, event copies, device scales 1/2/3 passed");
	return 0;
}
