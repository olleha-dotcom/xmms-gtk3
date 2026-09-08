#ifndef XMMS_GTK_COMPAT_CAIRO_H
#define XMMS_GTK_COMPAT_CAIRO_H

#include <cairo.h>

/* Paint the original skin in logical units. The target's device scale belongs
 * to GDK; nearest filtering preserves skin pixels at both UI and device scales.
 * This also works on image surfaces, without GTK initialization or a display. */
static inline int xmms_cairo_paint_skin(cairo_t *cr, cairo_surface_t *skin,
				       int scale)
{
	if (!cr || !skin || scale < 1)
		return 0;

	cairo_save(cr);
	cairo_scale(cr, scale, scale);
	cairo_set_source_surface(cr, skin, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
	cairo_paint(cr);
	cairo_restore(cr);
	return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

#endif /* XMMS_GTK_COMPAT_CAIRO_H */
