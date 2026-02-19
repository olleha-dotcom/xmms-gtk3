#ifndef XMMS_GTK_COMPAT_H
#define XMMS_GTK_COMPAT_H

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 0, 0)
#include <glib-object.h>
#include <cairo.h>
#include <gdk/gdk.h>
#include <stdarg.h>
#include <string.h>

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

/* GTK1/GTK2 legacy object and signal shims */
typedef GObject GtkObject;
typedef GCallback GtkSignalFunc;

#ifndef GTK_OBJECT
#define GTK_OBJECT(obj) G_OBJECT(obj)
#endif

#ifndef GTK_SIGNAL_FUNC
#define GTK_SIGNAL_FUNC(func) G_CALLBACK(func)
#endif

typedef struct _XmmsGtkLegacySelectRowBridge
{
	GCallback callback;
	gpointer data;
	gboolean want_selected;
} XmmsGtkLegacySelectRowBridge;

static inline void xmms_gtk_legacy_select_row_bridge_cb(GtkTreeSelection *selection, gpointer user_data)
{
	XmmsGtkLegacySelectRowBridge *bridge = (XmmsGtkLegacySelectRowBridge *) user_data;
	GtkTreePath *path = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gint row = -1;
	GtkTreeView *view = NULL;
	typedef void (*XmmsGtkSelectRowFunc)(GtkCList *, gint, gint, GdkEventButton *, gpointer);

	if (!bridge || !bridge->callback)
		return;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		path = gtk_tree_model_get_path(model, &iter);
	}
	if (path)
	{
		gint *indices = gtk_tree_path_get_indices(path);
		if (indices)
			row = indices[0];
		gtk_tree_path_free(path);
	}

	if (bridge->want_selected && row < 0)
		return;
	if (!bridge->want_selected && row >= 0)
		return;

	view = gtk_tree_selection_get_tree_view(selection);
	if (!view)
		return;
	((XmmsGtkSelectRowFunc) bridge->callback)((GtkCList *) view, row, 0, NULL, bridge->data);
}

static inline gulong xmms_gtk_connect_legacy_select_row(gpointer object, GCallback func, gpointer data, gboolean after, gboolean want_selected)
{
	XmmsGtkLegacySelectRowBridge *bridge;
	GtkTreeSelection *selection;
	if (!GTK_IS_TREE_VIEW(object) || !func)
		return 0;
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(object));
	if (!selection)
		return 0;
	bridge = g_malloc0(sizeof(*bridge));
	bridge->callback = func;
	bridge->data = data;
	bridge->want_selected = want_selected;
	return g_signal_connect_data(
		selection,
		"changed",
		G_CALLBACK(xmms_gtk_legacy_select_row_bridge_cb),
		bridge,
		(GClosureNotify) g_free,
		after ? G_CONNECT_AFTER : 0);
}

static inline gulong xmms_gtk_signal_connect(gpointer object, const gchar *name, GCallback func, gpointer data)
{
	const gchar *mapped = name;
	gchar *canonical = NULL;
	if (!object || !name || !func)
		return 0;
	if (g_strcmp0(name, "client_event") == 0)
		return 0;
	if (g_strcmp0(name, "select_row") == 0)
		return xmms_gtk_connect_legacy_select_row(object, func, data, FALSE, TRUE);
	if (g_strcmp0(name, "unselect_row") == 0)
		return xmms_gtk_connect_legacy_select_row(object, func, data, FALSE, FALSE);
	canonical = g_strdup(mapped);
	g_strdelimit(canonical, "_", '-');
	mapped = canonical;
	{
		gulong signal_id = g_signal_connect(object, mapped, func, data);
		g_free(canonical);
		return signal_id;
	}
}

static inline gulong xmms_gtk_signal_connect_after(gpointer object, const gchar *name, GCallback func, gpointer data)
{
	const gchar *mapped = name;
	gchar *canonical = NULL;
	if (!object || !name || !func)
		return 0;
	if (g_strcmp0(name, "client_event") == 0)
		return 0;
	if (g_strcmp0(name, "select_row") == 0)
		return xmms_gtk_connect_legacy_select_row(object, func, data, TRUE, TRUE);
	if (g_strcmp0(name, "unselect_row") == 0)
		return xmms_gtk_connect_legacy_select_row(object, func, data, TRUE, FALSE);
	canonical = g_strdup(mapped);
	g_strdelimit(canonical, "_", '-');
	mapped = canonical;
	{
		gulong signal_id = g_signal_connect_after(object, mapped, func, data);
		g_free(canonical);
		return signal_id;
	}
}

#ifndef gtk_signal_connect
#define gtk_signal_connect(object, name, func, data) \
	xmms_gtk_signal_connect((object), (name), G_CALLBACK(func), (data))
#endif

#ifndef gtk_signal_connect_after
#define gtk_signal_connect_after(object, name, func, data) \
	xmms_gtk_signal_connect_after((object), (name), G_CALLBACK(func), (data))
#endif

#ifndef gtk_signal_connect_object
#define gtk_signal_connect_object(object, name, func, slot_object) \
	g_signal_connect_swapped((object), (name), G_CALLBACK(func), (slot_object))
#endif

#ifndef gtk_signal_disconnect_by_func
#define gtk_signal_disconnect_by_func(object, func, data) \
	g_signal_handlers_disconnect_by_func((object), (func), (data))
#endif

#ifndef gtk_signal_emit_stop_by_name
#define gtk_signal_emit_stop_by_name(object, signal_name) \
	g_signal_stop_emission_by_name((object), (signal_name))
#endif

#ifndef gtk_signal_emit_by_name
#define gtk_signal_emit_by_name(object, signal_name) \
	g_signal_emit_by_name((object), (signal_name))
#endif

#ifndef gtk_idle_add
#define gtk_idle_add(function, data) \
	g_idle_add((GSourceFunc) (function), (data))
#endif

static inline GtkWidget *xmms_gtk_color_selection_new(void)
{
	return gtk_color_chooser_widget_new();
}

static inline void xmms_gtk_color_selection_get_color(GtkColorSelection *colorsel, gdouble *color)
{
	GdkRGBA rgba;
	if (!colorsel || !color)
		return;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorsel), &rgba);
	color[0] = rgba.red;
	color[1] = rgba.green;
	color[2] = rgba.blue;
}

static inline void xmms_gtk_color_selection_set_color(GtkColorSelection *colorsel, const gdouble *color)
{
	GdkRGBA rgba;
	if (!colorsel || !color)
		return;
	rgba.red = color[0];
	rgba.green = color[1];
	rgba.blue = color[2];
	rgba.alpha = 1.0;
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorsel), &rgba);
}

#define gtk_color_selection_new() \
	xmms_gtk_color_selection_new()
#define gtk_color_selection_get_color(colorsel, color) \
	xmms_gtk_color_selection_get_color((colorsel), (color))
#define gtk_color_selection_set_color(colorsel, color) \
	xmms_gtk_color_selection_set_color((colorsel), (color))

#ifndef gtk_object_get_data_by_id
#define gtk_object_get_data_by_id(object, id) \
	g_object_get_qdata(G_OBJECT((object)), (id))
#endif

#ifndef gtk_object_remove_data_by_id
#define gtk_object_remove_data_by_id(object, id) \
	g_object_set_qdata(G_OBJECT((object)), (id), NULL)
#endif

#ifndef gtk_object_set_data_by_id_full
#define gtk_object_set_data_by_id_full(object, id, data, destroy) \
	g_object_set_qdata_full(G_OBJECT((object)), (id), (data), (destroy))
#endif

#ifndef gtk_object_get_data
#define gtk_object_get_data(object, key) \
	g_object_get_data(G_OBJECT((object)), (key))
#endif

#ifndef gtk_object_set_data
#define gtk_object_set_data(object, key, data) \
	g_object_set_data(G_OBJECT((object)), (key), (data))
#endif

#ifndef gtk_object_remove_data
#define gtk_object_remove_data(object, key) \
	g_object_set_data(G_OBJECT((object)), (key), NULL)
#endif

/* Legacy timer API */
static inline guint gtk_timeout_add(guint interval, GSourceFunc function, gpointer data)
{
	return g_timeout_add(interval, function, data);
}

static inline void gtk_timeout_remove(guint tag)
{
	g_source_remove(tag);
}

/* GTK1/GTK2 geometry API */
static inline void gtk_widget_set_usize(GtkWidget *widget, gint width, gint height)
{
	gtk_widget_set_size_request(widget, width, height);
}

static inline void gtk_container_border_width(GtkContainer *container, guint border_width)
{
	gtk_container_set_border_width(container, border_width);
}

/* Legacy widget flags used for default buttons */
#ifndef GTK_CAN_DEFAULT
#define GTK_CAN_DEFAULT 1
#endif

static inline void xmms_gtk_widget_set_flags(GtkWidget *widget, gint flags)
{
	if ((flags & GTK_CAN_DEFAULT) != 0)
		gtk_widget_set_can_default(widget, TRUE);
}

#ifndef GTK_WIDGET_SET_FLAGS
#define GTK_WIDGET_SET_FLAGS(widget, flags) xmms_gtk_widget_set_flags((widget), (flags))
#endif

#ifndef GTK_WIDGET_VISIBLE
#define GTK_WIDGET_VISIBLE(widget) gtk_widget_get_visible((widget))
#endif

#ifndef GTK_WIDGET_MAPPED
#define GTK_WIDGET_MAPPED(widget) gtk_widget_get_mapped((widget))
#endif

#ifndef GTK_WIDGET_REALIZED
#define GTK_WIDGET_REALIZED(widget) gtk_widget_get_realized((widget))
#endif

#ifndef g_thread_init
#define g_thread_init(vtable) ((void) 0)
#endif

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

#ifndef GTK_PACKER
#define GTK_PACKER(widget) ((GtkPacker *) (widget))
#endif

static inline GtkWidget *gtk_packer_new(void)
{
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
	return grid;
}

static inline void gtk_packer_add(GtkPacker *packer, GtkWidget *child, gint side, gint anchor, gint options,
				       guint border_width, guint pad_x, guint pad_y, gint i_pad_x, gint i_pad_y)
{
	gint col = 1, row = 1;
	(void) anchor;
	(void) options;
	(void) border_width;
	(void) i_pad_x;
	(void) i_pad_y;
	if (!GTK_IS_GRID(packer))
	{
		gtk_container_add(GTK_CONTAINER(packer), child);
		return;
	}
	switch (side)
	{
		case GTK_SIDE_TOP:
			row = 0;
			break;
		case GTK_SIDE_BOTTOM:
			row = 2;
			break;
		case GTK_SIDE_LEFT:
			col = 0;
			break;
		case GTK_SIDE_RIGHT:
			col = 2;
			break;
		default:
			break;
	}
	gtk_widget_set_margin_start(child, (gint) pad_x);
	gtk_widget_set_margin_end(child, (gint) pad_x);
	gtk_widget_set_margin_top(child, (gint) pad_y);
	gtk_widget_set_margin_bottom(child, (gint) pad_y);
	gtk_grid_attach(GTK_GRID(packer), child, col, row, 1, 1);
}

#ifndef GTK_SELECTION_EXTENDED
#define GTK_SELECTION_EXTENDED GTK_SELECTION_MULTIPLE
#endif

static inline GdkVisual *gdk_rgb_get_visual(void)
{
	GdkScreen *screen = gdk_screen_get_default();
	return screen ? gdk_screen_get_system_visual(screen) : NULL;
}

static inline GtkCombo *xmms_gtk_combo_get(GtkWidget *widget)
{
	return (GtkCombo *) g_object_get_data(G_OBJECT(widget), "xmms-gtk-combo");
}

#ifndef GTK_COMBO
#define GTK_COMBO(widget) xmms_gtk_combo_get((GtkWidget *) (widget))
#endif

static inline GtkWidget *gtk_combo_new(void)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkCombo *combo = g_malloc0(sizeof(GtkCombo));
	combo->entry = gtk_entry_new();
	combo->list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(box), combo->entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), combo->list, FALSE, FALSE, 0);
	gtk_widget_hide(combo->list);
	gtk_widget_show(combo->entry);
	g_object_set_data(G_OBJECT(combo->list), "xmms-parent-combo", combo);
	g_object_set_data_full(G_OBJECT(box), "xmms-gtk-combo", combo, g_free);
	return box;
}

static inline void gtk_combo_set_popdown_strings(GtkCombo *combo, GList *strings)
{
	GList *node;
	GList *children;
	if (!combo || !combo->list)
		return;
	children = gtk_container_get_children(GTK_CONTAINER(combo->list));
	for (node = children; node != NULL; node = node->next)
		gtk_widget_destroy(GTK_WIDGET(node->data));
	g_list_free(children);
	for (node = strings; node != NULL; node = node->next)
	{
		GtkWidget *item = gtk_label_new(node->data ? (const gchar *) node->data : "");
		gtk_container_add(GTK_CONTAINER(combo->list), item);
		gtk_widget_show(item);
	}
}

static inline void gtk_combo_set_use_arrows_always(GtkCombo *combo, gboolean use_arrows)
{
	(void) combo;
	(void) use_arrows;
}

#ifndef gtk_radio_button_group
#define gtk_radio_button_group(button) gtk_radio_button_get_group((button))
#endif

#ifndef GTK_ITEM
#define GTK_ITEM(widget) (widget)
#endif

#ifndef GTK_LIST
#define GTK_LIST(widget) ((GtkList *) (widget))
#endif

static inline GtkWidget *gtk_list_item_new_with_label(const gchar *label)
{
	return gtk_label_new(label);
}

static inline void gtk_list_select_item(GtkList *list, gint item)
{
	GList *children, *node;
	gint idx = 0;
	if (!list)
		return;
	children = gtk_container_get_children(GTK_CONTAINER(list));
	for (node = children; node != NULL; node = node->next, idx++)
	{
		if (idx == item)
		{
			GtkWidget *child = GTK_WIDGET(node->data);
			GtkCombo *combo = g_object_get_data(G_OBJECT(list), "xmms-parent-combo");
			const gchar *mapped = g_object_get_data(G_OBJECT(child), "xmms-combo-item-string");
			if (combo && combo->entry)
			{
				if (mapped)
					gtk_entry_set_text(GTK_ENTRY(combo->entry), mapped);
				else if (GTK_IS_LABEL(child))
					gtk_entry_set_text(GTK_ENTRY(combo->entry), gtk_label_get_text(GTK_LABEL(child)));
			}
			break;
		}
	}
	g_list_free(children);
}

static inline void gtk_combo_set_item_string(GtkCombo *combo, GtkWidget *item, const gchar *item_value)
{
	(void) combo;
	g_object_set_data_full(G_OBJECT(item), "xmms-combo-item-string",
			       g_strdup(item_value ? item_value : ""), g_free);
}

static inline void gtk_button_box_set_spacing(GtkButtonBox *widget, gint spacing)
{
	gtk_box_set_spacing(GTK_BOX(widget), spacing);
}

static inline void gtk_widget_set_uposition(GtkWidget *widget, gint x, gint y)
{
	if (GTK_IS_WINDOW(widget))
		gtk_window_move(GTK_WINDOW(widget), x, y);
}

static inline void gtk_window_set_policy(GtkWindow *window, gboolean allow_shrink, gboolean allow_grow, gboolean auto_shrink)
{
	(void) allow_shrink;
	(void) auto_shrink;
	gtk_window_set_resizable(window, allow_grow);
}

static inline void gtk_entry_select_region(GtkEntry *entry, gint start, gint end)
{
	gtk_editable_select_region(GTK_EDITABLE(entry), start, end);
}

static inline void gtk_entry_set_editable(GtkEntry *entry, gboolean editable)
{
	gtk_editable_set_editable(GTK_EDITABLE(entry), editable);
}

static inline GtkWidget *gtk_entry_new_with_max_length(gint max)
{
	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), max);
	return entry;
}

static inline void gtk_box_pack_start_defaults(GtkBox *box, GtkWidget *child)
{
	gtk_box_pack_start(box, child, TRUE, TRUE, 0);
}

static inline void gtk_button_box_set_child_size(GtkButtonBox *widget, gint min_width, gint min_height)
{
	(void) widget;
	(void) min_width;
	(void) min_height;
}

static inline void gtk_notebook_set_page(GtkNotebook *notebook, gint page_num)
{
	gtk_notebook_set_current_page(notebook, page_num);
}

static inline void gtk_widget_set(GtkWidget *widget, const gchar *first_property_name, ...)
{
	va_list ap;
	const gchar *name = first_property_name;
	va_start(ap, first_property_name);
	while (name)
	{
		if (g_strcmp0(name, "label") == 0)
		{
			const gchar *label = va_arg(ap, const gchar *);
			if (GTK_IS_BUTTON(widget))
				gtk_button_set_label(GTK_BUTTON(widget), label ? label : "");
			else if (GTK_IS_LABEL(widget))
				gtk_label_set_text(GTK_LABEL(widget), label ? label : "");
		}
		else
		{
			(void) va_arg(ap, gpointer);
		}
		name = va_arg(ap, const gchar *);
	}
	va_end(ap);
}

static inline void gtk_exit(gint error_code)
{
	(void) error_code;
	gtk_main_quit();
}

static inline GtkTooltips *gtk_tooltips_new(void)
{
	return g_malloc0(sizeof(GtkTooltips));
}

static inline void gtk_tooltips_set_tip(GtkTooltips *tooltips, GtkWidget *widget, const gchar *tip_text, const gchar *tip_private)
{
	(void) tooltips;
	(void) tip_private;
	gtk_widget_set_tooltip_text(widget, tip_text);
}

static inline void gtk_tooltips_enable(GtkTooltips *tooltips)
{
	(void) tooltips;
}

static inline void xmms_gtk_check_menu_item_set_active(GtkWidget *check_menu_item, gboolean is_active)
{
	if (GTK_IS_CHECK_MENU_ITEM(check_menu_item))
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(check_menu_item), is_active);
}

static inline void xmms_gtk_menu_item_set_submenu(GtkWidget *menu_item, GtkWidget *submenu)
{
	if (GTK_IS_MENU_ITEM(menu_item))
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
}

#ifndef gtk_check_menu_item_set_active
#define gtk_check_menu_item_set_active(check_menu_item, is_active) \
	xmms_gtk_check_menu_item_set_active((GtkWidget *) (check_menu_item), (is_active))
#endif

#ifndef gtk_menu_item_set_submenu
#define gtk_menu_item_set_submenu(menu_item, submenu) \
	xmms_gtk_menu_item_set_submenu((GtkWidget *) (menu_item), (submenu))
#endif

#ifndef GTK_OPTION_MENU
#define GTK_OPTION_MENU(widget) ((GtkOptionMenu *) (widget))
#endif

#ifndef GTK_FILE_SELECTION
#define GTK_FILE_SELECTION(widget) ((GtkFileSelection *) (widget))
#endif

static inline GtkWidget *gtk_option_menu_new(void)
{
	return gtk_combo_box_text_new();
}

static inline void gtk_option_menu_remove_menu(GtkOptionMenu *omenu)
{
	if (GTK_IS_COMBO_BOX_TEXT(omenu))
		gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(omenu));
	g_object_set_data(G_OBJECT(omenu), "xmms-option-menu", NULL);
}

static inline void gtk_option_menu_set_menu(GtkOptionMenu *omenu, GtkWidget *menu)
{
	GList *children, *node;
	if (!GTK_IS_COMBO_BOX_TEXT(omenu))
	{
		g_object_set_data(G_OBJECT(omenu), "xmms-option-menu", menu);
		return;
	}
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(omenu));
	if (GTK_IS_MENU(menu))
	{
		children = gtk_container_get_children(GTK_CONTAINER(menu));
		for (node = children; node != NULL; node = node->next)
		{
			GtkWidget *item = GTK_WIDGET(node->data);
			const gchar *label = NULL;
			if (GTK_IS_MENU_ITEM(item))
			{
				GtkWidget *child = gtk_bin_get_child(GTK_BIN(item));
				if (GTK_IS_LABEL(child))
					label = gtk_label_get_text(GTK_LABEL(child));
			}
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(omenu), label ? label : "");
		}
		g_list_free(children);
	}
	g_object_set_data(G_OBJECT(omenu), "xmms-option-menu", menu);
}

static inline void gtk_option_menu_set_history(GtkOptionMenu *omenu, guint index)
{
	if (GTK_IS_COMBO_BOX(omenu))
		gtk_combo_box_set_active(GTK_COMBO_BOX(omenu), (gint) index);
}

static inline GtkWidget *gtk_file_selection_new(const gchar *title)
{
	return gtk_file_chooser_dialog_new(title, NULL,
					   GTK_FILE_CHOOSER_ACTION_OPEN,
					   "Cancel", GTK_RESPONSE_CANCEL,
					   "OK", GTK_RESPONSE_ACCEPT,
					   NULL);
}

static inline void gtk_file_selection_set_filename(GtkFileSelection *filesel, const gchar *filename)
{
	if (!GTK_IS_FILE_CHOOSER(filesel) || !filename)
		return;
	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filesel), filename);
	else
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(filesel), filename);
}

static inline gchar *gtk_file_selection_get_filename(GtkFileSelection *filesel)
{
	gchar *filename;
	if (!GTK_IS_FILE_CHOOSER(filesel))
		return NULL;
	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel));
	g_object_set_data_full(G_OBJECT(filesel), "xmms-filesel-filename", filename, g_free);
	return filename;
}

static inline GtkWidget *xmms_gtk_file_selection_get_ok_button(GtkFileSelection *filesel)
{
	return gtk_dialog_get_widget_for_response(GTK_DIALOG(filesel), GTK_RESPONSE_ACCEPT);
}

static inline GtkWidget *xmms_gtk_file_selection_get_cancel_button(GtkFileSelection *filesel)
{
	return gtk_dialog_get_widget_for_response(GTK_DIALOG(filesel), GTK_RESPONSE_CANCEL);
}

static inline GtkWidget *xmms_gtk_file_selection_get_action_area(GtkFileSelection *filesel)
{
	return gtk_dialog_get_action_area(GTK_DIALOG(filesel));
}

static inline void gtk_menu_append(GtkMenu *menu, GtkWidget *child)
{
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), child);
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
	pixbuf = gdk_pixbuf_new_from_xpm_data((const char * const *) data);
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

static inline GtkWidget *gtk_pixmap_new(GdkPixmap *pixmap, GdkBitmap *mask)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	(void) mask;
	if (!pixmap)
		return gtk_image_new();
	pixbuf = gdk_pixbuf_get_from_surface(pixmap, 0, 0,
					     cairo_image_surface_get_width(pixmap),
					     cairo_image_surface_get_height(pixmap));
	image = pixbuf ? gtk_image_new_from_pixbuf(pixbuf) : gtk_image_new();
	if (pixbuf)
		g_object_unref(pixbuf);
	return image;
}

static inline void gtk_widget_shape_combine_mask(GtkWidget *widget, GdkBitmap *mask, gint offset_x, gint offset_y)
{
	(void) widget;
	(void) mask;
	(void) offset_x;
	(void) offset_y;
}

#ifndef GTK_CLIST
#define GTK_CLIST(widget) ((GtkCList *) (widget))
#endif

typedef gint (*GtkCListCompareFunc)(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2);

static inline GtkWidget *gtk_clist_new_with_titles(gint columns, gchar *titles[]);

static inline gint xmms_gtk_clist_num_columns(GtkCList *clist)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(clist), "xmms-clist-num-columns"));
}

static inline gint xmms_gtk_clist_row_data_column(GtkCList *clist)
{
	return xmms_gtk_clist_num_columns(clist);
}

static inline gboolean xmms_gtk_clist_iter_for_row(GtkCList *clist, gint row, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	if (!GTK_IS_TREE_VIEW(clist) || row < 0)
		return FALSE;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(clist));
	return gtk_tree_model_iter_nth_child(model, iter, NULL, row);
}

static inline gint xmms_gtk_clist_get_selected_row(GtkCList *clist)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indices;
	gint row = -1;

	if (!GTK_IS_TREE_VIEW(clist))
		return -1;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(clist));
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return -1;

	path = gtk_tree_model_get_path(model, &iter);
	if (!path)
		return -1;
	indices = gtk_tree_path_get_indices(path);
	if (indices)
		row = indices[0];
	gtk_tree_path_free(path);
	return row;
}

static inline gboolean xmms_gtk_clist_has_selection(GtkCList *clist)
{
	return xmms_gtk_clist_get_selected_row(clist) >= 0;
}

static inline gint xmms_gtk_clist_get_focus_row(GtkCList *clist)
{
	GtkTreePath *path = NULL;
	gint *indices;
	gint row = -1;

	if (!GTK_IS_TREE_VIEW(clist))
		return -1;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(clist), &path, NULL);
	if (!path)
		return xmms_gtk_clist_get_selected_row(clist);
	indices = gtk_tree_path_get_indices(path);
	if (indices)
		row = indices[0];
	gtk_tree_path_free(path);
	return row;
}

static inline void xmms_gtk_clist_set_focus_row(GtkCList *clist, gint row)
{
	GtkTreePath *path;
	if (!GTK_IS_TREE_VIEW(clist) || row < 0)
		return;
	path = gtk_tree_path_new_from_indices(row, -1);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(clist), path, NULL, FALSE);
	gtk_tree_path_free(path);
}

static inline void xmms_gtk_clist_apply_theme(GtkWidget *view)
{
	static GtkCssProvider *provider = NULL;
	GtkStyleContext *ctx;
	GdkScreen *screen;
	GError *error = NULL;
	const gchar css[] =
		"treeview.xmms-clist.view, treeview.view.xmms-clist {"
		"background-color: #0a120a;"
		"color: #d0d0d0;"
		"}"
		"treeview.xmms-clist.view:selected, treeview.view.xmms-clist:selected {"
		"background-color: #0a124a;"
		"color: #ffffff;"
		"}"
		"treeview.xmms-clist.view row:selected, treeview.view.xmms-clist row:selected {"
		"background-color: #0a124a;"
		"color: #ffffff;"
		"}";

	if (!view)
		return;

	if (!provider)
	{
		provider = gtk_css_provider_new();
		gtk_css_provider_load_from_data(provider, css, -1, &error);
		if (error)
		{
			g_warning("xmms gtk compat: failed to load clist css: %s", error->message);
			g_error_free(error);
			return;
		}
		screen = gdk_screen_get_default();
		if (screen)
			gtk_style_context_add_provider_for_screen(
				screen, GTK_STYLE_PROVIDER(provider),
				GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	ctx = gtk_widget_get_style_context(view);
	if (ctx)
		gtk_style_context_add_class(ctx, "xmms-clist");
}

static inline GtkWidget *gtk_clist_new(gint columns)
{
	return gtk_clist_new_with_titles(columns, NULL);
}

static inline GtkWidget *gtk_clist_new_with_titles(gint columns, gchar *titles[])
{
	GType *types;
	GtkListStore *store;
	GtkWidget *view;
	gint i;

	if (columns <= 0)
		columns = 1;

	types = g_new0(GType, (gsize) columns + 1);
	for (i = 0; i < columns; i++)
		types[i] = G_TYPE_STRING;
	types[columns] = G_TYPE_POINTER;

	store = gtk_list_store_newv(columns + 1, types);
	g_free(types);

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), titles != NULL);
	xmms_gtk_clist_apply_theme(view);

	for (i = 0; i < columns; i++)
	{
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
			(titles && titles[i]) ? titles[i] : "", renderer, "text", i, NULL);
		g_object_set(renderer,
			     "foreground", "#d0d0d0",
			     "foreground-set", TRUE,
			     NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	}

	g_object_set_data(G_OBJECT(view), "xmms-clist-num-columns", GINT_TO_POINTER(columns));
	g_object_set_data(G_OBJECT(view), "xmms-clist-sort-column", GINT_TO_POINTER(0));
	g_object_set_data(G_OBJECT(view), "xmms-clist-sort-type", GINT_TO_POINTER(GTK_SORT_ASCENDING));
	return view;
}

static inline void gtk_clist_column_titles_passive(GtkCList *clist)
{
	(void) clist;
}

static inline void gtk_clist_columns_autosize(GtkCList *clist)
{
	(void) clist;
}

static inline void gtk_clist_freeze(GtkCList *clist)
{
	(void) clist;
}

static inline void gtk_clist_thaw(GtkCList *clist)
{
	(void) clist;
}

static inline void gtk_clist_clear(GtkCList *clist)
{
	GtkListStore *store;
	if (!GTK_IS_TREE_VIEW(clist))
		return;
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));
	gtk_list_store_clear(store);
}

static inline gint gtk_clist_append(GtkCList *clist, gchar *text[])
{
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint columns, i, row;

	if (!GTK_IS_TREE_VIEW(clist))
		return -1;

	columns = xmms_gtk_clist_num_columns(clist);
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));
	gtk_list_store_append(store, &iter);
	for (i = 0; i < columns; i++)
		gtk_list_store_set(store, &iter, i, (text && text[i]) ? text[i] : "", -1);
	gtk_list_store_set(store, &iter, xmms_gtk_clist_row_data_column(clist), NULL, -1);

	model = GTK_TREE_MODEL(store);
	row = gtk_tree_model_iter_n_children(model, NULL) - 1;
	return row;
}

static inline gint gtk_clist_prepend(GtkCList *clist, gchar *text[])
{
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint columns, i;

	if (!GTK_IS_TREE_VIEW(clist))
		return -1;

	columns = xmms_gtk_clist_num_columns(clist);
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));
	gtk_list_store_insert(store, &iter, 0);
	for (i = 0; i < columns; i++)
		gtk_list_store_set(store, &iter, i, (text && text[i]) ? text[i] : "", -1);
	gtk_list_store_set(store, &iter, xmms_gtk_clist_row_data_column(clist), NULL, -1);

	model = GTK_TREE_MODEL(store);
	return gtk_tree_model_iter_n_children(model, NULL) > 0 ? 0 : -1;
}

static inline gint xmms_gtk_clist_row_count(GtkCList *clist)
{
	GtkTreeModel *model;
	if (!GTK_IS_TREE_VIEW(clist))
		return 0;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(clist));
	return gtk_tree_model_iter_n_children(model, NULL);
}

static inline void gtk_clist_set_row_data_full(GtkCList *clist, gint row, gpointer data, GDestroyNotify destroy)
{
	GtkListStore *store;
	GtkTreeIter iter;
	(void) destroy;
	if (!xmms_gtk_clist_iter_for_row(clist, row, &iter))
		return;
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));
	gtk_list_store_set(store, &iter, xmms_gtk_clist_row_data_column(clist), data, -1);
}

static inline gpointer gtk_clist_get_row_data(GtkCList *clist, gint row)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer data = NULL;
	if (!xmms_gtk_clist_iter_for_row(clist, row, &iter))
		return NULL;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(clist));
	gtk_tree_model_get(model, &iter, xmms_gtk_clist_row_data_column(clist), &data, -1);
	return data;
}

static inline void gtk_clist_get_text(GtkCList *clist, gint row, gint column, gchar **text)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *value = NULL;
	if (text)
		*text = NULL;
	if (!text || !xmms_gtk_clist_iter_for_row(clist, row, &iter))
		return;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(clist));
	gtk_tree_model_get(model, &iter, column, &value, -1);
	g_object_set_data_full(G_OBJECT(clist), "xmms-clist-temp-text", value, g_free);
	*text = value;
}

static inline void gtk_clist_remove(GtkCList *clist, gint row)
{
	GtkListStore *store;
	GtkTreeIter iter;
	if (!xmms_gtk_clist_iter_for_row(clist, row, &iter))
		return;
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));
	gtk_list_store_remove(store, &iter);
}

static inline void gtk_clist_set_column_auto_resize(GtkCList *clist, gint column, gboolean auto_resize)
{
	GtkTreeViewColumn *tvcol;
	(void) auto_resize;
	if (!GTK_IS_TREE_VIEW(clist))
		return;
	tvcol = gtk_tree_view_get_column(GTK_TREE_VIEW(clist), column);
	if (tvcol)
		gtk_tree_view_column_set_sizing(tvcol, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
}

static inline void gtk_clist_set_column_justification(GtkCList *clist, gint column, GtkJustification justification)
{
	GtkTreeViewColumn *tvcol;
	if (!GTK_IS_TREE_VIEW(clist))
		return;
	tvcol = gtk_tree_view_get_column(GTK_TREE_VIEW(clist), column);
	if (!tvcol)
		return;
	if (justification == GTK_JUSTIFY_RIGHT)
		gtk_tree_view_column_set_alignment(tvcol, 1.0f);
	else if (justification == GTK_JUSTIFY_CENTER)
		gtk_tree_view_column_set_alignment(tvcol, 0.5f);
	else
		gtk_tree_view_column_set_alignment(tvcol, 0.0f);
}

static inline void gtk_clist_set_selection_mode(GtkCList *clist, GtkSelectionMode mode)
{
	GtkTreeSelection *selection;
	if (!GTK_IS_TREE_VIEW(clist))
		return;
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(clist));
	gtk_tree_selection_set_mode(selection, mode);
}

static inline void gtk_clist_select_row(GtkCList *clist, gint row, gint column)
{
	GtkTreePath *path;
	GtkTreeSelection *selection;
	(void) column;
	if (!GTK_IS_TREE_VIEW(clist) || row < 0)
		return;
	path = gtk_tree_path_new_from_indices(row, -1);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(clist));
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(clist), path, NULL, FALSE);
	gtk_tree_path_free(path);
}

static inline void gtk_clist_moveto(GtkCList *clist, gint row, gint column, gfloat row_align, gfloat col_align)
{
	GtkTreePath *path;
	GtkTreeViewColumn *tvcol = NULL;
	if (!GTK_IS_TREE_VIEW(clist) || row < 0)
		return;
	path = gtk_tree_path_new_from_indices(row, -1);
	if (column >= 0)
		tvcol = gtk_tree_view_get_column(GTK_TREE_VIEW(clist), column);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(clist), path, tvcol, TRUE, row_align, col_align);
	gtk_tree_path_free(path);
}

static inline void gtk_clist_set_sort_column(GtkCList *clist, gint column)
{
	g_object_set_data(G_OBJECT(clist), "xmms-clist-sort-column", GINT_TO_POINTER(column));
}

static inline void gtk_clist_set_sort_type(GtkCList *clist, GtkSortType sort_type)
{
	g_object_set_data(G_OBJECT(clist), "xmms-clist-sort-type", GINT_TO_POINTER(sort_type));
}

static inline void gtk_clist_set_compare_func(GtkCList *clist, GtkCListCompareFunc cmp_func)
{
	g_object_set_data(G_OBJECT(clist), "xmms-clist-compare-func", cmp_func);
}

static inline void gtk_clist_sort(GtkCList *clist)
{
	GtkTreeSortable *sortable;
	gint column;
	GtkSortType sort_type;
	if (!GTK_IS_TREE_VIEW(clist))
		return;
	sortable = GTK_TREE_SORTABLE(gtk_tree_view_get_model(GTK_TREE_VIEW(clist)));
	column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(clist), "xmms-clist-sort-column"));
	sort_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(clist), "xmms-clist-sort-type"));
	gtk_tree_sortable_set_sort_column_id(sortable, column, sort_type);
}

static inline void gtk_clist_set_reorderable(GtkCList *clist, gboolean reorderable)
{
	if (GTK_IS_TREE_VIEW(clist))
		gtk_tree_view_set_reorderable(GTK_TREE_VIEW(clist), reorderable);
}

#ifndef GTK_BUTTON_IGNORED
#define GTK_BUTTON_IGNORED 0
#endif

static inline void gtk_clist_set_button_actions(GtkCList *clist, gint button, gint actions)
{
	(void) clist;
	(void) button;
	(void) actions;
}

static inline void gtk_clist_set_selectable(GtkCList *clist, gint row, gboolean selectable)
{
	(void) clist;
	(void) row;
	(void) selectable;
}

static inline GtkAdjustment *gtk_clist_get_vadjustment(GtkCList *clist)
{
	if (GTK_IS_SCROLLABLE(clist))
		return gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(clist));
	return NULL;
}

static inline void gtk_clist_set_vadjustment(GtkCList *clist, GtkAdjustment *adjustment)
{
	(void) clist;
	(void) adjustment;
}

static inline void gdk_cursor_destroy(GdkCursor *cursor)
{
	if (cursor)
		g_object_unref(cursor);
}

static inline void gtk_item_factory_dump_rc(const gchar *filename, gpointer user_data, gboolean include_n_nodes)
{
	(void) filename;
	(void) user_data;
	(void) include_n_nodes;
}

#ifndef GTK_ITEM_FACTORY
#define GTK_ITEM_FACTORY(widget) ((GtkItemFactory *) (widget))
#endif

static inline void gtk_item_factory_parse_rc(const gchar *filename)
{
	(void) filename;
}

static inline gpointer gtk_item_factory_popup_data_from_widget(GtkWidget *w)
{
	return g_object_get_data(G_OBJECT(w), "xmms-ifactory-popup-data");
}

typedef struct _XmmsGtkItemFactoryCallbackData
{
	GtkItemFactoryCallback callback;
	guint action;
	gpointer callback_data;
} XmmsGtkItemFactoryCallbackData;

static inline void xmms_gtk_item_factory_activate_cb(GtkWidget *widget, gpointer user_data)
{
	XmmsGtkItemFactoryCallbackData *cb = (XmmsGtkItemFactoryCallbackData *) user_data;
	if (cb && cb->callback)
		cb->callback(cb->callback_data, cb->action, widget);
}

static inline gchar *xmms_gtk_to_utf8_label(const gchar *text)
{
	gchar *utf8;
	if (!text)
		return g_strdup("");
	if (g_utf8_validate(text, -1, NULL))
		return g_strdup(text);
	utf8 = g_locale_to_utf8(text, -1, NULL, NULL, NULL);
	if (utf8)
		return utf8;
	return g_strdup("?");
}

static inline GtkWidget *xmms_gtk_item_factory_get_menu_for_path(GtkItemFactory *ifactory, const gchar *path)
{
	GtkWidget *item;
	GtkWidget *submenu;
	gchar *parent;
	gchar *slash;
	const gchar *label;
	gchar *utf8_label;

	if (!ifactory || !path || path[0] == '\0')
		return ifactory ? ifactory->widget : NULL;

	if (g_strcmp0(path, "/") == 0)
		return ifactory->widget;

	if (g_hash_table_lookup(ifactory->path_map, path))
	{
		item = g_hash_table_lookup(ifactory->path_map, path);
		if (GTK_IS_MENU(item))
			return item;
		submenu = GTK_IS_MENU_ITEM(item) ? gtk_menu_item_get_submenu(GTK_MENU_ITEM(item)) : NULL;
		return submenu ? submenu : ifactory->widget;
	}

	parent = g_strdup(path);
	slash = strrchr(parent, '/');
	if (!slash || slash == parent)
	{
		g_free(parent);
		return ifactory->widget;
	}
	*slash = '\0';
	label = slash + 1;
	submenu = xmms_gtk_item_factory_get_menu_for_path(ifactory, parent);
	utf8_label = xmms_gtk_to_utf8_label(label);
	item = gtk_menu_item_new_with_label(utf8_label);
	g_free(utf8_label);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	gtk_widget_show(item);
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	g_hash_table_insert(ifactory->path_map, g_strdup(path), item);
	g_free(parent);
	return submenu;
}

static inline GtkWidget *gtk_item_factory_get_widget(GtkItemFactory *ifactory, const gchar *path)
{
	GtkWidget *widget;
	if (!ifactory)
		return NULL;
	if (!path || path[0] == '\0')
		return ifactory->widget;
	if (ifactory->root_path && g_strcmp0(path, ifactory->root_path) == 0)
		return ifactory->widget;
	widget = g_hash_table_lookup(ifactory->path_map, path);
	return widget ? widget : NULL;
}

static inline GtkItemFactory *gtk_item_factory_new(GType container_type, const gchar *path, GtkAccelGroup *accel_group)
{
	GtkItemFactory *ifactory = g_malloc0(sizeof(GtkItemFactory));
	(void) container_type;
	(void) path;
	(void) accel_group;
	ifactory->widget = gtk_menu_new();
	ifactory->path_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ifactory->root_path = g_strdup(path ? path : "");
	return ifactory;
}

static inline void gtk_item_factory_create_items(GtkItemFactory *ifactory, guint n_entries,
						 GtkItemFactoryEntry *entries, gpointer callback_data)
{
	guint i;
	if (!ifactory || !entries)
		return;
	for (i = 0; i < n_entries; i++)
	{
		GtkItemFactoryEntry *entry = &entries[i];
		GtkWidget *parent_menu;
		GtkWidget *item = NULL;
		gchar *parent_path;
		gchar *slash;
		const gchar *label;
		gchar *label_copy = NULL;
		gchar *utf8_label;
		if (!entry->path || entry->path[0] == '\0')
			continue;

		parent_path = g_strdup(entry->path);
		slash = strrchr(parent_path, '/');
		if (!slash)
		{
			g_free(parent_path);
			continue;
		}
		if (slash == parent_path)
		{
			parent_menu = ifactory->widget;
			label = slash + 1;
		}
		else
		{
			*slash = '\0';
			label = slash + 1;
			parent_menu = xmms_gtk_item_factory_get_menu_for_path(ifactory, parent_path);
		}
		label_copy = g_strdup(label);
		g_free(parent_path);
		label = label_copy;

		if (g_strcmp0(label, "-") == 0 || g_strcmp0(entry->item_type, "<Separator>") == 0)
			item = gtk_separator_menu_item_new();
		else if (g_strcmp0(entry->item_type, "<Branch>") == 0)
		{
			utf8_label = xmms_gtk_to_utf8_label(label);
			item = gtk_menu_item_new_with_label(utf8_label);
			g_free(utf8_label);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), gtk_menu_new());
		}
		else if (g_strcmp0(entry->item_type, "<RadioItem>") == 0 ||
			 (entry->item_type && entry->item_type[0] == '/'))
		{
			GSList *group = NULL;
			GtkWidget *group_item = NULL;
			const gchar *group_ref = NULL;
			utf8_label = xmms_gtk_to_utf8_label(label);

			if (entry->item_type && entry->item_type[0] == '/')
				group_ref = entry->item_type;

			if (group_ref)
			{
				group_item = g_hash_table_lookup(ifactory->path_map, group_ref);
				if (GTK_IS_RADIO_MENU_ITEM(group_item))
					group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(group_item));
			}

			item = gtk_radio_menu_item_new_with_label(group, utf8_label);
			g_free(utf8_label);
		}
		else if (g_strcmp0(entry->item_type, "<CheckItem>") == 0 ||
			 g_strcmp0(entry->item_type, "<ToggleItem>") == 0)
		{
			utf8_label = xmms_gtk_to_utf8_label(label);
			item = gtk_check_menu_item_new_with_label(utf8_label);
			g_free(utf8_label);
		}
		else
		{
			utf8_label = xmms_gtk_to_utf8_label(label);
			item = gtk_menu_item_new_with_label(utf8_label);
			g_free(utf8_label);
		}

		gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), item);
		gtk_widget_show(item);
		g_hash_table_insert(ifactory->path_map, g_strdup(entry->path), item);

		if (entry->callback && g_strcmp0(entry->item_type, "<Branch>") != 0)
		{
			XmmsGtkItemFactoryCallbackData *cb = g_malloc0(sizeof(*cb));
			cb->callback = entry->callback;
			cb->action = entry->action;
			cb->callback_data = callback_data;
			g_signal_connect_data(item, "activate",
					      G_CALLBACK(xmms_gtk_item_factory_activate_cb),
					      cb, (GClosureNotify) g_free, 0);
		}
		g_free(label_copy);
	}
}

static inline void gtk_item_factory_set_translate_func(GtkItemFactory *ifactory,
						       gpointer func, gpointer data, gpointer notify)
{
	(void) ifactory;
	(void) func;
	(void) data;
	(void) notify;
}

static inline gchar *gtk_set_locale(void)
{
	return NULL;
}

static inline void gdk_rgb_init(void)
{
}

static inline GdkColormap *gdk_rgb_get_cmap(void)
{
	return NULL;
}

static inline void gtk_widget_set_default_colormap(GdkColormap *colormap)
{
	(void) colormap;
}

static inline void gtk_widget_set_default_visual(GdkVisual *visual)
{
	(void) visual;
}

static inline void gdk_set_sm_client_id(const gchar *sm_client_id)
{
	(void) sm_client_id;
}

static inline void gtk_rc_init(void)
{
}

static inline void gdk_window_set_icon(gpointer window, gpointer colormap, gpointer pixmap, gpointer mask)
{
	(void) window;
	(void) colormap;
	(void) pixmap;
	(void) mask;
}

#endif /* GTK_CHECK_VERSION(3, 0, 0) */

#endif /* XMMS_GTK_COMPAT_H */
