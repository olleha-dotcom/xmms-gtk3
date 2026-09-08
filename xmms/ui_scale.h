#ifndef XMMS_UI_SCALE_H
#define XMMS_UI_SCALE_H

/* Skin pixels and GTK window-local logical coordinates are different spaces.
 * GDK owns logical-to-device scaling (including the default GDK_SCALE=2).
 * Never include the monitor scale or root/window origins in these transforms. */
typedef enum
{
	XMMS_UI_MAIN,
	XMMS_UI_EQUALIZER,
	XMMS_UI_PLAYLIST
} XmmsUiWindow;

static inline int xmms_ui_skin_scale(XmmsUiWindow window, int doublesize,
				    int eq_doublesize_linked)
{
	return doublesize && (window == XMMS_UI_MAIN ||
		(window == XMMS_UI_EQUALIZER && eq_doublesize_linked)) ? 2 : 1;
}

/* Pass original local coordinates by value; write only into an event copy.
 * Keep fractional and out-of-window positions intact for grabbed drags. */
static inline void xmms_ui_to_skin_xy(int scale, double logical_x,
				      double logical_y, double *skin_x,
				      double *skin_y)
{
	*skin_x = logical_x / scale;
	*skin_y = logical_y / scale;
}

/* scale must come from xmms_ui_skin_scale(). Geometry stays in logical units. */
static inline int xmms_ui_to_logical(int scale, int skin_pixels)
{
	return skin_pixels * scale;
}

#endif /* XMMS_UI_SCALE_H */
