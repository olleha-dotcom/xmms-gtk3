#ifndef XMMS_GTK_COMPAT_TYPES_H
#define XMMS_GTK_COMPAT_TYPES_H

#include <gtk/gtk.h>
#include <cairo.h>

#if GTK_CHECK_VERSION(3, 0, 0)

/* Legacy GTK type aliases removed in GTK3 */
typedef cairo_surface_t GdkPixmap;
typedef cairo_surface_t GdkBitmap;
typedef cairo_t GdkGC;
typedef cairo_surface_t GdkDrawable;
typedef GDestroyNotify GtkDestroyNotify;
typedef struct _GtkItemFactory GtkItemFactory;
typedef struct _GtkFileSelection GtkFileSelection;
typedef struct _GtkCList GtkCList;
typedef struct _GtkCTree GtkCTree;
typedef struct _GtkCTreeNode GtkCTreeNode;
typedef struct _GtkOptionMenu GtkOptionMenu;
typedef struct _GtkTooltips GtkTooltips;
typedef GtkWidget GtkList;
typedef struct _GdkColormap GdkColormap;
typedef struct _XmmsGdkCompatImage GdkImage;
typedef struct _XmmsGdkCompatFont GdkFont;
typedef struct _XmmsGtkComboCompat GtkCombo;
typedef struct _XmmsGdkRgbCmap GdkRgbCmap;
typedef gunichar GdkWChar;
typedef void (*GtkItemFactoryCallback)(gpointer, guint, GtkWidget *);
typedef gchar *(*XmmsGtkItemFactoryTranslateFunc)(const gchar *path, gpointer user_data);
typedef struct _XmmsGtkItemFactoryEntry GtkItemFactoryEntry;

struct _XmmsGdkCompatImage
{
	gint width;
	gint height;
	gint bpp;
	gint bpl;
	guchar *data;
};

struct _XmmsGdkCompatFont
{
	gint ascent;
	gint descent;
};

struct _XmmsGtkComboCompat
{
	GtkWidget *entry;
	GtkWidget *list;
};

struct _GtkItemFactory
{
	GtkWidget *widget;
	GHashTable *path_map;
	gchar *root_path;
	XmmsGtkItemFactoryTranslateFunc translate_func;
	gpointer translate_data;
};

struct _XmmsGtkItemFactoryEntry
{
	const gchar *path;
	const gchar *accelerator;
	GtkItemFactoryCallback callback;
	guint action;
	const gchar *item_type;
};

struct _GtkTooltips
{
	gint unused;
};

struct _XmmsGdkRgbCmap
{
	gint n_colors;
	guint32 colors[256];
};

typedef GObject GtkObject;
typedef GCallback GtkSignalFunc;

#ifndef GDK_RGB_DITHER_MAX
#define GDK_RGB_DITHER_MAX 0
#endif

#ifndef GDK_RGB_DITHER_NONE
#define GDK_RGB_DITHER_NONE 0
#endif

#ifndef GDK_RGB_DITHER_NORMAL
#define GDK_RGB_DITHER_NORMAL 0
#endif

#ifndef GDK_IMAGE_NORMAL
#define GDK_IMAGE_NORMAL 0
#endif

#ifndef GDK_IMAGE_XIMAGE
#define GDK_IMAGE_XIMAGE(img) (img)
#endif

#ifndef GDK_Escape
#define GDK_Escape GDK_KEY_Escape
#endif

#ifndef GDK_Up
#define GDK_Up GDK_KEY_Up
#endif

#ifndef GDK_Down
#define GDK_Down GDK_KEY_Down
#endif

#ifndef GDK_Left
#define GDK_Left GDK_KEY_Left
#endif

#ifndef GDK_Right
#define GDK_Right GDK_KEY_Right
#endif

#ifndef GDK_KP_Up
#define GDK_KP_Up GDK_KEY_KP_Up
#endif

#ifndef GDK_KP_Down
#define GDK_KP_Down GDK_KEY_KP_Down
#endif

#ifndef GDK_KP_Left
#define GDK_KP_Left GDK_KEY_KP_Left
#endif

#ifndef GDK_KP_Right
#define GDK_KP_Right GDK_KEY_KP_Right
#endif

#ifndef GDK_Return
#define GDK_Return GDK_KEY_Return
#endif

#ifndef GDK_Page_Up
#define GDK_Page_Up GDK_KEY_Page_Up
#endif

#ifndef GDK_Page_Down
#define GDK_Page_Down GDK_KEY_Page_Down
#endif

#ifndef GDK_BackSpace
#define GDK_BackSpace GDK_KEY_BackSpace
#endif

#ifndef GDK_Delete
#define GDK_Delete GDK_KEY_Delete
#endif

#ifndef GDK_Home
#define GDK_Home GDK_KEY_Home
#endif

#ifndef GDK_End
#define GDK_End GDK_KEY_End
#endif

#ifndef GDK_KP_Enter
#define GDK_KP_Enter GDK_KEY_KP_Enter
#endif

#ifndef GDK_Insert
#define GDK_Insert GDK_KEY_Insert
#endif

#ifndef GDK_quoteleft
#define GDK_quoteleft GDK_KEY_quoteleft
#endif

#ifndef GDK_Tab
#define GDK_Tab GDK_KEY_Tab
#endif

#ifndef GDK_0
#define GDK_0 GDK_KEY_0
#endif
#ifndef GDK_1
#define GDK_1 GDK_KEY_1
#endif
#ifndef GDK_2
#define GDK_2 GDK_KEY_2
#endif
#ifndef GDK_3
#define GDK_3 GDK_KEY_3
#endif
#ifndef GDK_4
#define GDK_4 GDK_KEY_4
#endif
#ifndef GDK_5
#define GDK_5 GDK_KEY_5
#endif
#ifndef GDK_6
#define GDK_6 GDK_KEY_6
#endif
#ifndef GDK_7
#define GDK_7 GDK_KEY_7
#endif
#ifndef GDK_8
#define GDK_8 GDK_KEY_8
#endif
#ifndef GDK_9
#define GDK_9 GDK_KEY_9
#endif

#ifndef GDK_q
#define GDK_q GDK_KEY_q
#endif
#ifndef GDK_w
#define GDK_w GDK_KEY_w
#endif
#ifndef GDK_e
#define GDK_e GDK_KEY_e
#endif
#ifndef GDK_r
#define GDK_r GDK_KEY_r
#endif
#ifndef GDK_t
#define GDK_t GDK_KEY_t
#endif
#ifndef GDK_y
#define GDK_y GDK_KEY_y
#endif
#ifndef GDK_u
#define GDK_u GDK_KEY_u
#endif
#ifndef GDK_i
#define GDK_i GDK_KEY_i
#endif
#ifndef GDK_o
#define GDK_o GDK_KEY_o
#endif
#ifndef GDK_p
#define GDK_p GDK_KEY_p
#endif

#ifndef GDK_WINDOW_XWINDOW
#define GDK_WINDOW_XWINDOW(window) GDK_WINDOW_XID((window))
#endif

typedef struct _XmmsGdkEventClientCompat
{
	GdkAtom message_type;
} GdkEventClient;

#ifndef GTK_WINDOW_DIALOG
#define GTK_WINDOW_DIALOG GTK_WINDOW_TOPLEVEL
#endif

#ifndef GTK_SIDE_TOP
#define GTK_SIDE_TOP 0
#endif
#ifndef GTK_SIDE_BOTTOM
#define GTK_SIDE_BOTTOM 1
#endif
#ifndef GTK_SIDE_LEFT
#define GTK_SIDE_LEFT 2
#endif
#ifndef GTK_SIDE_RIGHT
#define GTK_SIDE_RIGHT 3
#endif

#ifndef GTK_ANCHOR_CENTER
#define GTK_ANCHOR_CENTER 0
#endif

typedef GtkWidget GtkPacker;

#ifndef GTK_SELECTION_EXTENDED
#define GTK_SELECTION_EXTENDED GTK_SELECTION_MULTIPLE
#endif

typedef gint (*GtkCListCompareFunc)(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2);

#endif /* GTK_CHECK_VERSION(3, 0, 0) */

#endif /* XMMS_GTK_COMPAT_TYPES_H */
