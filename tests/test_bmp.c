#include "xmms/bmp.h"

#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static gchar *directory, *filename;

static void put16(guchar *p, guint value)
{
	p[0] = value;
	p[1] = value >> 8;
}

static void put32(guchar *p, guint32 value)
{
	p[0] = value;
	p[1] = value >> 8;
	p[2] = value >> 16;
	p[3] = value >> 24;
}

static guint32 get32(const guchar *p)
{
	return (guint32)p[0] | ((guint32)p[1] << 8) |
	       ((guint32)p[2] << 16) | ((guint32)p[3] << 24);
}

static GByteArray *make_bmp(guint bits, guint header, gboolean masks,
			  gboolean top_down, guint gap)
{
	guint width = 3, height = 2;
	guint stride = ((width * bits + 31) / 32) * 4;
	guint colors = bits <= 8 ? 1u << bits : 0;
	guint entry_size = header == 12 ? 3 : 4;
	guint palette = 14 + header + (masks && header == 40 ? 12 : 0);
	guint offset = palette + colors * entry_size + gap;
	GByteArray *bmp = g_byte_array_sized_new(offset + stride * height);
	guchar *p;
	guint x, y;

	g_byte_array_set_size(bmp, offset + stride * height);
	memset(bmp->data, 0, bmp->len);
	p = bmp->data;
	p[0] = 'B'; p[1] = 'M';
	put32(p + 2, bmp->len);
	put32(p + 10, offset);
	put32(p + 14, header);
	if (header == 12)
	{
		put16(p + 18, width);
		put16(p + 20, height);
		put16(p + 22, 1);
		put16(p + 24, bits);
	}
	else
	{
		put32(p + 18, width);
		put32(p + 22, top_down ? 0u - height : height);
		put16(p + 26, 1);
		put16(p + 28, bits);
		put32(p + 30, masks ? 3 : 0);
		put32(p + 34, stride * height);
		put32(p + 46, colors);
	}
	if (masks)
	{
		put32(p + 54, bits == 16 ? 0xf800 : 0xff0000);
		put32(p + 58, bits == 16 ? 0x07e0 : 0x00ff00);
		put32(p + 62, bits == 16 ? 0x001f : 0x0000ff);
		if (header >= 56 && bits == 32)
			put32(p + 66, 0xff000000);
	}
	if (colors)
	{
		p[palette + 2] = 255;
		p[palette + entry_size + 1] = 255;
	}
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			guint green = (x + (top_down ? y : height - y - 1)) & 1;
			guchar *row = p + offset + y * stride;
			if (bits <= 8)
				row[x * bits / 8] |= green << (8 - bits - (x * bits % 8));
			else if (bits == 16)
				put16(row + x * 2, green ? (masks ? 0x07e0 : 0x03e0) :
					(masks ? 0xf800 : 0x7c00));
			else
			{
				row[x * (bits / 8) + (green ? 1 : 2)] = 255;
				if (bits == 32)
					row[x * 4 + 3] = x ? 128 : 0;
			}
		}
	return bmp;
}

static cairo_surface_t *load_bytes(const guchar *data, gsize length)
{
	g_assert_true(g_file_set_contents(filename, (const gchar *)data, length, NULL));
	return read_bmp(filename);
}

static guint32 pixel(cairo_surface_t *surface, guint x, guint y)
{
	guchar *row;
	cairo_surface_flush(surface);
	row = cairo_image_surface_get_data(surface) +
		y * cairo_image_surface_get_stride(surface);
	return ((guint32 *)row)[x];
}

static void assert_pattern(GByteArray *bmp)
{
	cairo_surface_t *surface = load_bytes(bmp->data, bmp->len);
	guint x, y;
	g_assert_nonnull(surface);
	g_assert_cmpint(cairo_surface_status(surface), ==, CAIRO_STATUS_SUCCESS);
	g_assert_cmpint(cairo_image_surface_get_format(surface), ==, CAIRO_FORMAT_ARGB32);
	g_assert_cmpint(cairo_image_surface_get_width(surface), ==, 3);
	g_assert_cmpint(cairo_image_surface_get_height(surface), ==, 2);
	for (y = 0; y < 2; y++)
		for (x = 0; x < 3; x++)
			g_assert_cmphex(pixel(surface, x, y), ==,
				       (x + y) & 1 ? 0xff00ff00u : 0xffff0000u);
	cairo_surface_destroy(surface);
}

static void test_formats(void)
{
	const guint headers[] = { 12, 40, 52, 56, 108, 124 };
	const guint depths[] = { 1, 4, 8, 16, 24, 32 };
	guint h, b, top, masks;
	for (h = 0; h < G_N_ELEMENTS(headers); h++)
		for (b = 0; b < G_N_ELEMENTS(depths); b++)
			for (top = 0; top < 2; top++)
				for (masks = 0; masks < 2; masks++)
				{
					GByteArray *bmp;
					if (headers[h] == 12 && (top || masks || depths[b] == 16 || depths[b] == 32))
						continue;
					if (masks && depths[b] != 16 && depths[b] != 32)
						continue;
					g_test_message("header=%u depth=%u top-down=%u masks=%u",
						       headers[h], depths[b], top, masks);
					bmp = make_bmp(depths[b], headers[h], masks, top, 7);
					assert_pattern(bmp);
					g_byte_array_unref(bmp);
				}
}

static void assert_truncations(GByteArray *bmp)
{
	gsize length;
	for (length = 0; length < bmp->len; length++)
	{
		/* Also fix bfSize, so row/RLE validation, not just bfSize, is tested. */
		put32(bmp->data + 2, length);
		g_assert_null(load_bytes(bmp->data, length));
	}
	put32(bmp->data + 2, bmp->len);
}

static void test_truncated(void)
{
	const guint depths[] = { 1, 4, 8, 16, 24, 32 };
	guint i;
	for (i = 0; i < G_N_ELEMENTS(depths); i++)
	{
		GByteArray *bmp = make_bmp(depths[i], 124, depths[i] == 16 || depths[i] == 32, FALSE, 0);
		put32(bmp->data + 34, 0);
		assert_truncations(bmp);
		g_byte_array_unref(bmp);
	}
}

static GByteArray *make_rle(guint bits, const guchar *commands, guint length)
{
	GByteArray *bmp = make_bmp(bits, 40, FALSE, FALSE, 0);
	guint offset = get32(bmp->data + 10);
	g_byte_array_set_size(bmp, offset);
	g_byte_array_append(bmp, commands, length);
	put32(bmp->data + 2, bmp->len);
	put32(bmp->data + 18, 5);
	put32(bmp->data + 22, 3);
	put32(bmp->data + 30, bits == 4 ? 2 : 1);
	put32(bmp->data + 34, 0);
	put32(bmp->data + 46, 2);
	return bmp;
}

static void test_rle(void)
{
	const guchar rle4[] = { 5, 0, 0, 0, 0, 5, 0x01, 0x01, 0, 0,
		0, 0, 0, 2, 1, 0, 2, 0x11, 0, 1 };
	const guchar rle8[] = { 5, 0, 0, 0, 0, 5, 0, 1, 0, 1, 0, 0,
		0, 0, 0, 2, 1, 0, 2, 1, 0, 1 };
	guint bits;
	for (bits = 4; bits <= 8; bits += 4)
	{
		GByteArray *bmp = make_rle(bits, bits == 4 ? rle4 : rle8,
					 bits == 4 ? sizeof(rle4) : sizeof(rle8));
		cairo_surface_t *surface = load_bytes(bmp->data, bmp->len);
		g_assert_nonnull(surface);
		g_assert_cmphex(pixel(surface, 0, 0), ==, 0xff000000u);
		g_assert_cmphex(pixel(surface, 1, 0), ==, 0xff00ff00u);
		g_assert_cmphex(pixel(surface, 1, 1), ==, 0xff00ff00u);
		g_assert_cmphex(pixel(surface, 0, 2), ==, 0xffff0000u);
		cairo_surface_destroy(surface);
		assert_truncations(bmp);
		g_byte_array_unref(bmp);
	}
}

static void test_bad_rle(void)
{
	const guchar commands[][8] = {
		{ 1 }, /* Original review repro: truncated encoded run. */
		{ 0 }, { 0, 2, 1 }, { 0, 3, 0 }, { 0, 5, 0, 0, 0 },
		{ 6, 0, 0, 1 }, { 0, 2, 6, 0, 0, 1 },
		{ 0, 2, 0, 3, 0, 1 }, { 1, 0x22, 0, 1 },
		{ 0, 3, 0x22, 0x22, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 1, 0 }
	};
	const guint lengths[] = { 1, 1, 3, 3, 5, 4, 6, 6, 4, 6, 8 };
	guint bits, i;
	for (bits = 4; bits <= 8; bits += 4)
		for (i = 0; i < G_N_ELEMENTS(commands); i++)
		{
			GByteArray *bmp = make_rle(bits, commands[i], lengths[i]);
			g_assert_null(load_bytes(bmp->data, bmp->len));
			g_byte_array_unref(bmp);
		}
}

static void test_bad_headers(void)
{
	const struct { guint offset; guint32 value; } changes[] = {
		{ 0, 0 }, { 2, 0xffffffff }, { 10, 0 }, { 10, 0xffffffff },
		{ 14, 0xffffffff }, { 18, 0 }, { 18, 0xffffffff },
		{ 18, 16385 }, { 22, 0 }, { 22, 0x80000000 }, { 22, 16385 },
		{ 26, 0 }, { 28, 2 }, { 30, 1 }, { 30, 2 }, { 30, 4 },
		{ 34, 1 }, { 34, 0xffffffff }, { 46, 0xffffffff }
	};
	guint i;
	for (i = 0; i < G_N_ELEMENTS(changes); i++)
	{
		GByteArray *bmp = make_bmp(24, 40, FALSE, FALSE, 0);
		put32(bmp->data + changes[i].offset, changes[i].value);
		g_assert_null(load_bytes(bmp->data, bmp->len));
		g_byte_array_unref(bmp);
	}
	{
		const guchar end[] = { 0, 1 };
		GByteArray *bmp = make_rle(4, end, sizeof(end));
		cairo_surface_t *surface = load_bytes(bmp->data, bmp->len);
		g_assert_nonnull(surface);
		cairo_surface_destroy(surface);
		/* Tiny, complete compressed payload: rejection must be due to the
		 * pixel limit, not missing uncompressed row bytes.
		 */
		put32(bmp->data + 18, 8192);
		put32(bmp->data + 22, 8192);
		g_assert_null(load_bytes(bmp->data, bmp->len));
		put32(bmp->data + 18, 16385);
		put32(bmp->data + 22, 1);
		g_assert_null(load_bytes(bmp->data, bmp->len));
		put32(bmp->data + 18, 1);
		put32(bmp->data + 22, 16385);
		g_assert_null(load_bytes(bmp->data, bmp->len));
		g_byte_array_unref(bmp);
	}
}

static void test_v5_profile(void)
{
	GByteArray *bmp = make_bmp(24, 124, FALSE, FALSE, 0);
	guint original_size = bmp->len;
	const guchar profile[4] = { 0 };
	g_byte_array_append(bmp, profile, sizeof(profile));
	put32(bmp->data + 2, bmp->len);
	put32(bmp->data + 126, original_size - 14);
	put32(bmp->data + 130, sizeof(profile));
	assert_pattern(bmp);
	put32(bmp->data + 130, sizeof(profile) + 1);
	g_assert_null(load_bytes(bmp->data, bmp->len));
	put32(bmp->data + 130, sizeof(profile));
	put32(bmp->data + 126, original_size - 15);
	g_assert_null(load_bytes(bmp->data, bmp->len));
	put32(bmp->data + 126, 0xffffffff);
	g_assert_null(load_bytes(bmp->data, bmp->len));
	put32(bmp->data + 126, 0);
	g_assert_null(load_bytes(bmp->data, bmp->len));
	g_byte_array_unref(bmp);
}

static void test_bad_masks_palette(void)
{
	const guint32 masks[] = { 0, 0x07e0, 0xa800, 0x10000 };
	guint i;
	for (i = 0; i < G_N_ELEMENTS(masks); i++)
	{
		GByteArray *bmp = make_bmp(16, 40, TRUE, FALSE, 0);
		put32(bmp->data + 54, masks[i]);
		g_assert_null(load_bytes(bmp->data, bmp->len));
		g_byte_array_unref(bmp);
	}
	for (i = 1; i <= 8; i *= 2)
	{
		GByteArray *bmp;
		if (i == 2)
			continue;
		bmp = make_bmp(i, 40, FALSE, FALSE, 0);
		put32(bmp->data + 46, 1); /* Pixels still reference entry 1. */
		g_assert_null(load_bytes(bmp->data, bmp->len));
		put32(bmp->data + 46, (1u << i) + 1);
		g_assert_null(load_bytes(bmp->data, bmp->len));
		g_byte_array_unref(bmp);
	}
}

static void test_paths(void)
{
	GByteArray *bmp = make_bmp(24, 40, FALSE, FALSE, 0);
	gchar *link = g_build_filename(directory, "link.bmp", NULL);
	cairo_surface_t *surface;
	int fd;
	assert_pattern(bmp);
	g_assert_cmpint(symlink(filename, link), ==, 0);
	surface = read_bmp(link);
	g_assert_nonnull(surface);
	cairo_surface_destroy(surface);
	g_assert_cmpint(g_unlink(link), ==, 0);
	g_assert_null(read_bmp(link));
	g_assert_null(read_bmp(directory));
	g_assert_cmpint(mkfifo(link, 0600), ==, 0);
	g_assert_null(read_bmp(link));
	g_assert_cmpint(g_unlink(link), ==, 0);
	fd = g_open(filename, O_WRONLY, 0);
	g_assert_cmpint(fd, >=, 0);
	g_assert_cmpint(ftruncate(fd, 64 * 1024 * 1024 + 1), ==, 0);
	g_assert_cmpint(close(fd), ==, 0);
	g_assert_null(read_bmp(filename));
	g_free(link);
	g_byte_array_unref(bmp);
}

int main(int argc, char **argv)
{
	int result;
	g_test_init(&argc, &argv, NULL);
	directory = g_dir_make_tmp("xmms-bmp-test-XXXXXX", NULL);
	g_assert_nonnull(directory);
	filename = g_build_filename(directory, "image.bmp", NULL);
	g_test_add_func("/bmp/formats", test_formats);
	g_test_add_func("/bmp/truncated", test_truncated);
	g_test_add_func("/bmp/rle", test_rle);
	g_test_add_func("/bmp/bad-rle", test_bad_rle);
	g_test_add_func("/bmp/bad-headers", test_bad_headers);
	g_test_add_func("/bmp/bad-masks-palette", test_bad_masks_palette);
	g_test_add_func("/bmp/v5-profile", test_v5_profile);
	g_test_add_func("/bmp/paths", test_paths);
	result = g_test_run();
	g_unlink(filename);
	g_rmdir(directory);
	g_free(filename);
	g_free(directory);
	return result;
}
