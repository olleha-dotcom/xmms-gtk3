#ifndef XMMS_GTK_COMPAT_DRAWING_H
#define XMMS_GTK_COMPAT_DRAWING_H

#include "types.h"
#include "cairo.h"
#include <string.h>

#if GTK_CHECK_VERSION(3, 0, 0)

static inline GdkVisual *gdk_rgb_get_visual(void)
{
	GdkScreen *screen = gdk_screen_get_default();
	return screen ? gdk_screen_get_system_visual(screen) : NULL;
}

static inline GdkFont *gdk_font_load(const gchar *name)
{
	GdkFont *font = g_malloc0(sizeof(GdkFont));
	(void) name;
	font->ascent = 8;
	font->descent = 4;
	return font;
}

static inline GdkFont *gdk_fontset_load(const gchar *name)
{
	return gdk_font_load(name);
}

static inline void gdk_font_unref(GdkFont *font)
{
	g_free(font);
}

static inline gint gdk_text_width(GdkFont *font, const gchar *text, gint text_length)
{
	(void) font;
	if (!text || text_length <= 0)
		return 0;
	return text_length * 6;
}

static inline gint gdk_mbstowcs(GdkWChar *dest, const gchar *src, gint dest_max)
{
	gint i = 0;
	if (!dest || !src || dest_max <= 0)
		return 0;
	for (i = 0; i < dest_max - 1 && src[i] != '\0'; i++)
		dest[i] = (guchar) src[i];
	dest[i] = 0;
	return i;
}

static inline gint gdk_text_width_wc(GdkFont *font, const GdkWChar *text, gint text_length)
{
	(void) font;
	(void) text;
	if (text_length <= 0)
		return 0;
	return text_length * 6;
}

static inline GdkPixmap *gdk_pixmap_new(gpointer drawable, gint width, gint height, gint depth)
{
	cairo_format_t format = CAIRO_FORMAT_ARGB32;
	(void) drawable;
	(void) depth;
	if (width <= 0 || height <= 0)
		return NULL;
	return cairo_image_surface_create(format, width, height);
}

static inline void gdk_pixmap_unref(GdkPixmap *pixmap)
{
	if (pixmap)
		cairo_surface_destroy(pixmap);
}

static inline GdkGC *gdk_gc_new(GdkDrawable *drawable)
{
	if (!drawable)
		return NULL;
	if (GDK_IS_WINDOW(drawable))
		return gdk_cairo_create((GdkWindow *) drawable);
	return cairo_create((cairo_surface_t *) drawable);
}

static inline void gdk_gc_unref(GdkGC *gc)
{
	if (gc)
		cairo_destroy(gc);
}

static inline void gdk_gc_destroy(GdkGC *gc)
{
	gdk_gc_unref(gc);
}

static inline void gdk_gc_set_foreground(GdkGC *gc, const GdkColor *color)
{
	if (!gc || !color)
		return;
	cairo_set_source_rgb(gc, color->red / 65535.0, color->green / 65535.0, color->blue / 65535.0);
}

static inline void xmms_cairo_apply_gc_source(cairo_t *cr, GdkGC *gc)
{
	cairo_pattern_t *pattern;
	if (!cr || !gc)
		return;
	pattern = cairo_get_source(gc);
	if (pattern)
		cairo_set_source(cr, pattern);
}

static inline void gdk_draw_pixmap(GdkDrawable *drawable, GdkGC *gc, GdkPixmap *src,
				   gint xsrc, gint ysrc, gint xdest, gint ydest,
				   gint width, gint height)
{
	cairo_t *cr;
	(void) gc;
	if (!drawable || !src)
		return;
	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
		return;
	cairo_save(cr);
	cairo_rectangle(cr, xdest, ydest, width, height);
	cairo_clip(cr);
	cairo_set_source_surface(cr, src, xdest - xsrc, ydest - ysrc);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_destroy(cr);
}

static inline void gdk_draw_rectangle(GdkDrawable *drawable, GdkGC *gc, gboolean filled,
				      gint x, gint y, gint width, gint height)
{
	cairo_t *cr;
	if (!drawable)
		return;
	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
		return;
	xmms_cairo_apply_gc_source(cr, gc);
	cairo_rectangle(cr, x, y, width, height);
	if (filled)
		cairo_fill(cr);
	else
		cairo_stroke(cr);
	cairo_destroy(cr);
}

static inline void gdk_draw_line(GdkDrawable *drawable, GdkGC *gc,
				 gint x1, gint y1, gint x2, gint y2)
{
	cairo_t *cr;
	if (!drawable)
		return;
	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
		return;
	xmms_cairo_apply_gc_source(cr, gc);
	cairo_move_to(cr, x1, y1);
	cairo_line_to(cr, x2, y2);
	cairo_stroke(cr);
	cairo_destroy(cr);
}

static inline void gdk_draw_point(GdkDrawable *drawable, GdkGC *gc, gint x, gint y)
{
	gdk_draw_line(drawable, gc, x, y, x, y);
}

static inline void gdk_draw_polygon(GdkDrawable *drawable, GdkGC *gc, gboolean filled,
				    GdkPoint *points, gint npoints)
{
	cairo_t *cr;
	gint i;
	if (!drawable || !points || npoints <= 0)
		return;
	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
		return;
	xmms_cairo_apply_gc_source(cr, gc);
	cairo_move_to(cr, points[0].x, points[0].y);
	for (i = 1; i < npoints; i++)
		cairo_line_to(cr, points[i].x, points[i].y);
	cairo_close_path(cr);
	if (filled)
		cairo_fill(cr);
	else
		cairo_stroke(cr);
	cairo_destroy(cr);
}

static inline void gdk_draw_rgb_image(GdkDrawable *drawable, GdkGC *gc, gint x, gint y,
				      gint width, gint height, gint dither,
				      const guchar *rgb_buf, gint rowstride)
{
	cairo_t *cr;
	cairo_surface_t *src;
	guchar *data;
	gint i, j, stride;
	(void) gc;
	(void) dither;
	if (!drawable || !rgb_buf || width <= 0 || height <= 0)
		return;

	stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);
	data = g_malloc((gsize) stride * (gsize) height);
	for (i = 0; i < height; i++)
	{
		const guchar *in = rgb_buf + i * rowstride;
		guint32 *out = (guint32 *) (data + i * stride);
		for (j = 0; j < width; j++)
		{
			guint32 r = in[j * 3 + 0];
			guint32 g = in[j * 3 + 1];
			guint32 b = in[j * 3 + 2];
			out[j] = (r << 16) | (g << 8) | b;
		}
	}

	src = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24, width, height, stride);
	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
	{
		cairo_surface_destroy(src);
		g_free(data);
		return;
	}
	cairo_set_source_surface(cr, src, x, y);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(src);
	g_free(data);
}

static inline GdkRgbCmap *gdk_rgb_cmap_new(guint32 *colors, gint n_colors)
{
	GdkRgbCmap *cmap;
	gint n = CLAMP(n_colors, 0, 256);
	cmap = g_malloc0(sizeof(GdkRgbCmap));
	cmap->n_colors = n;
	if (colors && n > 0)
		memcpy(cmap->colors, colors, (gsize) n * sizeof(guint32));
	return cmap;
}

static inline void gdk_rgb_cmap_free(GdkRgbCmap *cmap)
{
	g_free(cmap);
}

static inline void gdk_draw_indexed_image(GdkDrawable *drawable, GdkGC *gc,
					  gint x, gint y,
					  gint width, gint height,
					  gint dither,
					  const guchar *buf, gint rowstride,
					  GdkRgbCmap *cmap)
{
	guchar *rgb_buf;
	gint i, j;
	(void) dither;
	if (!drawable || !buf || !cmap || width <= 0 || height <= 0)
		return;
	rgb_buf = g_malloc((gsize) width * (gsize) height * 3);
	for (i = 0; i < height; i++)
	{
		const guchar *in = buf + i * rowstride;
		guchar *out = rgb_buf + (gsize) i * (gsize) width * 3;
		for (j = 0; j < width; j++)
		{
			guint32 c = cmap->colors[in[j]];
			out[j * 3 + 0] = (guchar) ((c >> 16) & 0xFF);
			out[j * 3 + 1] = (guchar) ((c >> 8) & 0xFF);
			out[j * 3 + 2] = (guchar) (c & 0xFF);
		}
	}
	gdk_draw_rgb_image(drawable, gc, x, y, width, height, dither, rgb_buf, width * 3);
	g_free(rgb_buf);
}

static inline void gdk_gc_set_clip_mask(GdkGC *gc, GdkBitmap *mask)
{
	(void) gc;
	(void) mask;
}

static inline void gdk_draw_text(GdkDrawable *drawable, GdkFont *font, GdkGC *gc,
				 gint x, gint y, const gchar *text, gint text_length)
{
	cairo_t *cr;
	(void) font;
	if (!drawable || !text || text_length <= 0)
		return;
	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
		return;
	xmms_cairo_apply_gc_source(cr, gc);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, text);
	cairo_destroy(cr);
}

static inline void gdk_draw_text_wc(GdkDrawable *drawable, GdkFont *font, GdkGC *gc,
				    gint x, gint y, const GdkWChar *text, gint text_length)
{
	gchar *tmp;
	gint i;
	if (!text || text_length <= 0)
		return;
	tmp = g_malloc((gsize) text_length + 1);
	for (i = 0; i < text_length; i++)
		tmp[i] = (text[i] <= 0xFF) ? (gchar) text[i] : '?';
	tmp[text_length] = '\0';
	gdk_draw_text(drawable, font, gc, x, y, tmp, text_length);
	g_free(tmp);
}

static inline void gdk_draw_image(GdkDrawable *drawable, GdkGC *gc, GdkImage *image,
				  gint xsrc, gint ysrc, gint xdest, gint ydest,
				  gint width, gint height)
{
	cairo_t *cr;
	cairo_surface_t *src;
	gint draw_width;
	gint draw_height;
	(void) gc;
	if (!drawable || !image || !image->data || width <= 0 || height <= 0)
		return;
	if (xsrc < 0 || ysrc < 0 || xsrc >= image->width || ysrc >= image->height)
		return;
	draw_width = MIN(width, image->width - xsrc);
	draw_height = MIN(height, image->height - ysrc);
	if (draw_width <= 0 || draw_height <= 0)
		return;

	src = cairo_image_surface_create_for_data(image->data, CAIRO_FORMAT_ARGB32,
						  image->width, image->height, image->bpl);
	cairo_surface_mark_dirty(src);

	if (GDK_IS_WINDOW(drawable))
		cr = gdk_cairo_create((GdkWindow *) drawable);
	else
		cr = cairo_create((cairo_surface_t *) drawable);
	if (!cr)
	{
		cairo_surface_destroy(src);
		return;
	}

	cairo_save(cr);
	cairo_rectangle(cr, xdest, ydest, draw_width, draw_height);
	cairo_clip(cr);
	cairo_set_source_surface(cr, src, xdest - xsrc, ydest - ysrc);
	cairo_paint(cr);
	cairo_restore(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(src);
}

static inline GdkPixmap *gdk_pixmap_create_from_xpm_d(gpointer window, GdkBitmap **mask,
						      const GdkColor *transparent_color,
						      gchar **data)
{
	GdkPixbuf *pixbuf;
	cairo_surface_t *surface;
	gint w, h, stride, x, y, n_channels;
	guchar *pixels;
	(void) window;
	(void) transparent_color;
	if (mask)
		*mask = NULL;
	pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) data);
	if (!pixbuf)
		return NULL;

	w = gdk_pixbuf_get_width(pixbuf);
	h = gdk_pixbuf_get_height(pixbuf);
	n_channels = gdk_pixbuf_get_n_channels(pixbuf);
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	pixels = gdk_pixbuf_get_pixels(pixbuf);
	stride = gdk_pixbuf_get_rowstride(pixbuf);
	{
		unsigned char *dst = cairo_image_surface_get_data(surface);
		int dst_stride = cairo_image_surface_get_stride(surface);
		for (y = 0; y < h; y++)
		{
			guint32 *row = (guint32 *) (dst + y * dst_stride);
			guchar *src = pixels + y * stride;
			for (x = 0; x < w; x++)
			{
				guchar r = src[x * n_channels + 0];
				guchar g = src[x * n_channels + 1];
				guchar b = src[x * n_channels + 2];
				guchar a = n_channels == 4 ? src[x * n_channels + 3] : 255;
				row[x] = ((guint32) a << 24) | ((guint32) r << 16) | ((guint32) g << 8) | b;
			}
		}
		cairo_surface_mark_dirty(surface);
	}
	g_object_unref(pixbuf);
	return surface;
}

static inline void gdk_window_get_size(gpointer window, gint *width, gint *height)
{
	if (window && GDK_IS_WINDOW(window))
	{
		gint x, y;
		gdk_window_get_geometry((GdkWindow *) window, &x, &y, width, height);
		return;
	}
	if (window)
	{
		cairo_surface_t *s = (cairo_surface_t *) window;
		if (width)
			*width = cairo_image_surface_get_width(s);
		if (height)
			*height = cairo_image_surface_get_height(s);
		return;
	}
	if (width)
		*width = 0;
	if (height)
		*height = 0;
}

static inline gboolean gdk_window_get_deskrelative_origin(GdkWindow *window, gint *x, gint *y)
{
	if (!window)
		return FALSE;
	gdk_window_get_root_origin(window, x, y);
	return TRUE;
}

static inline void xmms_gdk_window_get_pointer(GdkWindow *window, gint *x, gint *y, GdkModifierType *mask)
{
	GdkDisplay *display;
	GdkSeat *seat;
	GdkDevice *pointer;
	GdkWindow *target = window;

	if (x)
		*x = 0;
	if (y)
		*y = 0;
	if (mask)
		*mask = 0;

	display = window ? gdk_window_get_display(window) : gdk_display_get_default();
	if (!display)
		return;

	seat = gdk_display_get_default_seat(display);
	pointer = seat ? gdk_seat_get_pointer(seat) : NULL;
	if (!pointer)
		return;

	if (!target)
	{
		GdkScreen *screen = gdk_display_get_default_screen(display);
		target = screen ? gdk_screen_get_root_window(screen) : NULL;
	}
	if (!target)
		return;

	gdk_window_get_device_position(target, pointer, x, y, mask);
}

static inline void xmms_gdk_window_raise_if_valid(GdkWindow *window)
{
	if (window && GDK_IS_WINDOW(window))
		gdk_window_raise(window);
}

#ifndef gdk_window_get_pointer
#define gdk_window_get_pointer(window, x, y, mask) \
	xmms_gdk_window_get_pointer((window), (x), (y), (mask))
#endif

static inline void gdk_window_set_hints(GdkWindow *window,
					gint x, gint y,
					gint min_width, gint min_height,
					gint max_width, gint max_height,
					gint flags)
{
	(void) window;
	(void) x;
	(void) y;
	(void) min_width;
	(void) min_height;
	(void) max_width;
	(void) max_height;
	(void) flags;
}

static inline void gdk_window_set_back_pixmap(GdkWindow *window, GdkPixmap *pixmap, gboolean parent_relative)
{
	cairo_t *cr;
	if (!window)
		return;
	(void) parent_relative;
	g_object_set_data(G_OBJECT(window), "xmms-back-pixmap", pixmap);
	if (!pixmap)
		return;
	cr = gdk_cairo_create(window);
	if (!cr)
		return;
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_paint(cr);
	cairo_set_source_surface(cr, pixmap, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
}

static inline void gdk_window_clear_area(gpointer window, gint x, gint y, gint width, gint height)
{
	cairo_t *cr;
	if (!window || width <= 0 || height <= 0)
		return;
	if (GDK_IS_WINDOW(window))
	{
		cairo_surface_t *bg = g_object_get_data(G_OBJECT(window), "xmms-back-pixmap");
		if (bg)
		{
			cr = gdk_cairo_create((GdkWindow *) window);
			if (!cr)
				return;
			cairo_rectangle(cr, x, y, width, height);
			cairo_clip(cr);
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
			cairo_paint(cr);
			cairo_set_source_surface(cr, bg, 0, 0);
			cairo_paint(cr);
			cairo_destroy(cr);
			return;
		}
		GdkRectangle rect;
		rect.x = x;
		rect.y = y;
		rect.width = width;
		rect.height = height;
		gdk_window_invalidate_rect((GdkWindow *) window, &rect, TRUE);
		gdk_window_process_updates((GdkWindow *) window, TRUE);
		return;
	}
	cr = cairo_create((cairo_surface_t *) window);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_rectangle(cr, x, y, width, height);
	cairo_fill(cr);
	cairo_destroy(cr);
}

static inline void gdk_window_clear(gpointer window)
{
	gint width = 0;
	gint height = 0;
	gdk_window_get_size(window, &width, &height);
	if (width > 0 && height > 0)
		gdk_window_clear_area(window, 0, 0, width, height);
}

static inline GdkColormap *gdk_window_get_colormap(GdkWindow *window)
{
	(void) window;
	return NULL;
}

static inline GdkColormap *gdk_colormap_get_system(void)
{
	return NULL;
}

static inline gboolean gdk_color_alloc(GdkColormap *colormap, GdkColor *color)
{
	(void) colormap;
	(void) color;
	return TRUE;
}

static inline gboolean gdk_colormap_alloc_color(GdkColormap *colormap, GdkColor *color,
						 gboolean writeable, gboolean best_match)
{
	(void) writeable;
	(void) best_match;
	return gdk_color_alloc(colormap, color);
}

static inline void gdk_color_black(GdkColormap *colormap, GdkColor *color)
{
	(void) colormap;
	if (!color)
		return;
	color->red = 0;
	color->green = 0;
	color->blue = 0;
}

static inline GdkImage *gdk_image_get(GdkPixmap *pixmap, gint x, gint y, gint width, gint height)
{
	GdkImage *img;
	unsigned char *src, *dst;
	int src_stride, row;

	if (!pixmap || width <= 0 || height <= 0)
		return NULL;

	img = g_malloc0(sizeof(GdkImage));
	img->width = width;
	img->height = height;
	img->bpp = 4;
	img->bpl = width * 4;
	img->data = g_malloc0((gsize) img->bpl * (gsize) height);

	cairo_surface_flush(pixmap);
	src = cairo_image_surface_get_data(pixmap);
	src_stride = cairo_image_surface_get_stride(pixmap);
	dst = img->data;
	for (row = 0; row < height; row++)
	{
		memcpy(dst + row * img->bpl, src + (y + row) * src_stride + x * 4, img->bpl);
	}

	return img;
}

static inline GdkImage *gdk_image_new(gint type, GdkVisual *visual, gint width, gint height)
{
	GdkImage *img;
	(void) type;
	(void) visual;
	if (width <= 0 || height <= 0)
		return NULL;
	img = g_malloc0(sizeof(GdkImage));
	img->width = width;
	img->height = height;
	img->bpp = 4;
	img->bpl = width * 4;
	img->data = g_malloc0((gsize) img->bpl * (gsize) height);
	return img;
}

static inline guint32 gdk_image_get_pixel(GdkImage *image, gint x, gint y)
{
	const guint32 *row;
	if (!image || !image->data || x < 0 || y < 0 ||
	    x >= image->width || y >= image->height)
		return 0;
	row = (const guint32 *) (image->data + y * image->bpl);
	return row[x];
}

static inline void gdk_image_destroy(GdkImage *image)
{
	if (!image)
		return;
	g_free(image->data);
	g_free(image);
}

static inline void gdk_bitmap_unref(GdkBitmap *bitmap)
{
	gdk_pixmap_unref(bitmap);
}

static inline void gdk_cursor_destroy(GdkCursor *cursor)
{
	if (cursor)
		g_object_unref(cursor);
}

static inline void gdk_rgb_init(void)
{
}

static inline GdkColormap *gdk_rgb_get_cmap(void)
{
	return NULL;
}

static inline void gdk_set_sm_client_id(const gchar *sm_client_id)
{
	(void) sm_client_id;
}

static inline void gdk_window_set_icon(gpointer window, gpointer colormap, gpointer pixmap, gpointer mask)
{
	(void) window;
	(void) colormap;
	(void) pixmap;
	(void) mask;
}

#endif /* GTK_CHECK_VERSION(3, 0, 0) */

#endif /* XMMS_GTK_COMPAT_DRAWING_H */
