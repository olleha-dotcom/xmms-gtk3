/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2002  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2002  Haavard Kvaalen		 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "bmp.h"

#include <errno.h>
#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Skin artwork must not be able to exhaust memory before decoding starts. */
#define BMP_MAX_FILE_SIZE (64 * 1024 * 1024)
#define BMP_MAX_PIXELS (16 * 1024 * 1024)
#define BMP_MAX_DIMENSION 16384

static guint16 bmp_u16(const guchar *p)
{
	return (guint16)p[0] | ((guint16)p[1] << 8);
}

static guint32 bmp_u32(const guchar *p)
{
	return (guint32)p[0] | ((guint32)p[1] << 8) |
	       ((guint32)p[2] << 16) | ((guint32)p[3] << 24);
}

static void bmp_put32(guchar *p, guint32 value)
{
	p[0] = value;
	p[1] = value >> 8;
	p[2] = value >> 16;
	p[3] = value >> 24;
}

static gboolean bmp_mask_valid(guint32 mask, guint bits)
{
	if (!mask || (bits == 16 && (mask >> 16)))
		return FALSE;
	while (!(mask & 1))
		mask >>= 1;
	return (mask & (mask + 1)) == 0;
}

/* Validate commands without writing pixels. Pixbuf's incremental BMP loader
 * can successfully close an incomplete image, so close() alone is not enough.
 * Skipped pixels and an early end-of-bitmap are legal (opaque black in skins).
 */
static gboolean bmp_rle_valid(const guchar *data, gsize size, guint width,
			      guint height, guint bits, guint colors)
{
	gsize pos = 0;
	guint x = 0, y = 0;

	while (size - pos >= 2)
	{
		guint count = data[pos++], code = data[pos++], i;
		gsize bytes;

		if (count)
		{
			if (y >= height || count > width - x)
				return FALSE;
			if (bits == 8 ? code >= colors :
			    ((code >> 4) >= colors || (count > 1 && (code & 15) >= colors)))
				return FALSE;
			x += count;
		}
		else if (code == 1)
			return TRUE;
		else if (code == 0)
		{
			if (y >= height)
				return FALSE;
			x = 0;
			y++;
		}
		else if (code == 2)
		{
			if (size - pos < 2 || y >= height ||
			    data[pos] > width - x || data[pos + 1] >= height - y)
				return FALSE;
			x += data[pos++];
			y += data[pos++];
		}
		else
		{
			bytes = bits == 8 ? code : (code + 1) / 2;
			bytes = (bytes + 1) & ~(gsize)1;
			if (y >= height || code > width - x || bytes > size - pos)
				return FALSE;
			for (i = 0; i < code; i++)
			{
				guint index = bits == 8 ? data[pos + i] :
					(data[pos + i / 2] >> ((i & 1) ? 0 : 4)) & 15;
				if (index >= colors)
					return FALSE;
			}
			pos += bytes;
			x += code;
		}
	}
	return FALSE; /* Every compressed bitmap needs an end-of-bitmap command. */
}

static GdkPixbuf *bmp_decode(const guchar *data, gsize length)
{
	guint32 header, offset, file_size, width, raw_height, height;
	guint32 compression = 0, image_size = 0, colors = 0;
	guint bits, planes, entry_size, i;
	gsize palette, stride, payload, metadata_end, profile_start = 0;
	gboolean top_down = FALSE, written, closed;
	guchar normalized[14 + 56 + 256 * 4] = { 'B', 'M' };
	guint normalized_header, normalized_size;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf = NULL;

	if (length < 26 || data[0] != 'B' || data[1] != 'M')
		return NULL;
	file_size = bmp_u32(data + 2);
	offset = bmp_u32(data + 10);
	header = bmp_u32(data + 14);
	if (file_size)
	{
		if (file_size > length || file_size < 26)
			return NULL;
		length = file_size;
	}
	if (header != 12 && header != 40 && header != 52 &&
	    header != 56 && header != 108 && header != 124)
		return NULL;
	if (14 + header > length || offset < 14 + header || offset > length)
		return NULL;
	palette = 14 + header;
	entry_size = header == 12 ? 3 : 4;
	if (header == 12)
	{
		width = bmp_u16(data + 18);
		raw_height = bmp_u16(data + 20);
		planes = bmp_u16(data + 22);
		bits = bmp_u16(data + 24);
	}
	else
	{
		width = bmp_u32(data + 18);
		raw_height = bmp_u32(data + 22);
		top_down = (raw_height & 0x80000000u) != 0;
		planes = bmp_u16(data + 26);
		bits = bmp_u16(data + 28);
		compression = bmp_u32(data + 30);
		image_size = bmp_u32(data + 34);
		colors = bmp_u32(data + 46);
	}
	height = top_down ? 0u - raw_height : raw_height;
	if (planes != 1 || !width || !height || width > BMP_MAX_DIMENSION ||
	    height > BMP_MAX_DIMENSION || (guint64)width * height > BMP_MAX_PIXELS)
		return NULL;
	if (bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32)
		return NULL;
	if (compression > 3 || (compression == 1 && bits != 8) ||
	    (compression == 2 && bits != 4) ||
	    (compression == 3 && bits != 16 && bits != 32) ||
	    (top_down && (compression == 1 || compression == 2)))
		return NULL;

	/* Feed a compact header to Pixbuf: older BMP loaders do not consistently
	 * honor gaps before bitfield pixels. V4/V5 color profiles are irrelevant
	 * to the classic skin's opaque RGB surface; retain their channel masks.
	 */
	normalized_header = compression == 3 ? 56 : 40;
	bmp_put32(normalized + 14, normalized_header);
	bmp_put32(normalized + 18, width);
	bmp_put32(normalized + 22, raw_height);
	normalized[26] = 1;
	normalized[28] = bits;
	bmp_put32(normalized + 30, compression);
	if (compression == 3)
	{
		guint32 masks[4] = { 0 }, used = 0;
		if (header == 40)
			palette += 12;
		if (palette > offset)
			return NULL;
		for (i = 0; i < 4; i++)
		{
			if (i < 3 || header >= 56)
				masks[i] = bmp_u32(data + 54 + i * 4);
			if ((i < 3 || masks[i]) &&
			    (!bmp_mask_valid(masks[i], bits) || (used & masks[i])))
				return NULL;
			used |= masks[i];
			bmp_put32(normalized + 54 + i * 4, masks[i]);
		}
	}
	if (bits <= 8)
	{
		if (!colors)
			colors = 1u << bits;
		if (colors > (1u << bits) || colors * entry_size > offset - palette)
			return NULL;
		bmp_put32(normalized + 46, colors);
		for (i = 0; i < colors; i++)
			memcpy(normalized + 14 + normalized_header + i * 4,
			       data + palette + i * entry_size, 3);
		metadata_end = palette + colors * entry_size;
	}
	else
	{
		if (colors > 256 || colors * entry_size > offset - palette)
			return NULL;
		metadata_end = palette + colors * entry_size;
		colors = 0;
	}
	payload = length - offset;
	if (header == 124)
	{
		guint32 profile_offset = bmp_u32(data + 126);
		guint32 profile_size = bmp_u32(data + 130);
		if ((profile_offset == 0) != (profile_size == 0))
			return NULL;
		if (profile_size)
		{
			if (profile_offset > length - 14 ||
			    profile_size > length - 14 - profile_offset)
				return NULL;
			profile_start = 14 + (gsize)profile_offset;
			if (profile_start < metadata_end ||
			    (profile_start < offset && profile_size > offset - profile_start))
				return NULL;
			if (profile_start >= offset)
				payload = profile_start - offset;
		}
	}
	if (image_size)
	{
		if (image_size > payload)
			return NULL;
		payload = image_size;
	}
	if (compression == 1 || compression == 2)
	{
		if (!bmp_rle_valid(data + offset, payload, width, height, bits, colors))
			return NULL;
	}
	else
	{
		stride = (((gsize)width * bits + 31) / 32) * 4;
		if (stride * height > payload)
			return NULL;
		payload = stride * height;
		if (bits <= 8 && colors < (1u << bits))
		{
			guint x, y;
			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++)
				{
					guint bit = x * bits;
					guint index = (data[offset + y * stride + bit / 8] >>
						(8 - bits - bit % 8)) & ((1u << bits) - 1);
					if (index >= colors)
						return NULL;
				}
		}
	}
	normalized_size = 14 + normalized_header + colors * 4;
	bmp_put32(normalized + 2, normalized_size + payload);
	bmp_put32(normalized + 10, normalized_size);
	bmp_put32(normalized + 34, payload);
	loader = gdk_pixbuf_loader_new_with_type("bmp", NULL);
	if (!loader)
		return NULL;
	written = gdk_pixbuf_loader_write(loader, normalized, normalized_size, NULL) &&
		  gdk_pixbuf_loader_write(loader, data + offset, payload, NULL);
	closed = gdk_pixbuf_loader_close(loader, NULL);
	if (written && closed)
	{
		pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
		if (pixbuf && gdk_pixbuf_get_width(pixbuf) == (gint)width &&
		    gdk_pixbuf_get_height(pixbuf) == (gint)height)
			g_object_ref(pixbuf);
		else
			pixbuf = NULL;
	}
	g_object_unref(loader);
	return pixbuf;
}

cairo_surface_t *read_bmp(gchar *filename)
{
	struct stat stats;
	int fd;
	guchar *data;
	gsize length, done = 0;
	GdkPixbuf *pixbuf = NULL;
	cairo_surface_t *surface = NULL;
	gint width, height, x, y, channels, source_stride, target_stride;
	guchar *source, *target;

	if (!filename)
		return NULL;
	fd = g_open(filename, O_RDONLY | O_NONBLOCK, 0);
	if (fd == -1)
		return NULL;
	if (fstat(fd, &stats) != 0 || !S_ISREG(stats.st_mode) ||
	    stats.st_size < 26 || stats.st_size > BMP_MAX_FILE_SIZE)
	{
		close(fd);
		return NULL;
	}
	length = stats.st_size;
	data = g_try_malloc(length);
	if (data)
	{
		while (done < length)
		{
			ssize_t count = read(fd, data + done, length - done);
			if (count < 0 && errno == EINTR)
				continue;
			if (count <= 0)
				break;
			done += count;
		}
	}
	if (close(fd) == 0 && data && done == length)
		pixbuf = bmp_decode(data, length);
	g_free(data);
	if (!pixbuf)
		return NULL;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(surface);
		g_object_unref(pixbuf);
		return NULL;
	}
	source = gdk_pixbuf_get_pixels(pixbuf);
	source_stride = gdk_pixbuf_get_rowstride(pixbuf);
	channels = gdk_pixbuf_get_n_channels(pixbuf);
	cairo_surface_flush(surface);
	target = cairo_image_surface_get_data(surface);
	target_stride = cairo_image_surface_get_stride(surface);
	/* Classic skin BMPs are opaque, even when their reserved/alpha byte is 0. */
	for (y = 0; y < height; y++)
	{
		const guchar *in = source + y * source_stride;
		guint32 *out = (guint32 *)(target + y * target_stride);
		for (x = 0; x < width; x++, in += channels)
			out[x] = 0xff000000u | ((guint32)in[0] << 16) |
				 ((guint32)in[1] << 8) | in[2];
	}
	cairo_surface_mark_dirty(surface);
	g_object_unref(pixbuf);
	return surface;
}
