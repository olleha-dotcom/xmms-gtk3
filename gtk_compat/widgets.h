#ifndef XMMS_GTK_COMPAT_WIDGETS_H
#define XMMS_GTK_COMPAT_WIDGETS_H

#include "types.h"
#include "signals.h"
#include "drawing.h"
#include <stdarg.h>
#include <string.h>

#if GTK_CHECK_VERSION(3, 0, 0)

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
		GtkWidget *item;
		const gchar *text = node->data ? (const gchar *) node->data : "";
		item = gtk_label_new(text);
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
	if (!list || item < 0)
		return;
	children = gtk_container_get_children(GTK_CONTAINER(list));
	for (node = children; node != NULL; node = node->next, idx++)
	{
		if (idx == item)
		{
			GtkWidget *child = GTK_WIDGET(node->data);
			GtkCombo *combo = g_object_get_data(G_OBJECT(list), "xmms-parent-combo");
			const gchar *mapped = g_object_get_data(G_OBJECT(child), "xmms-combo-item-string");
			if (combo && combo->entry && child)
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

#ifndef GTK_FILE_SELECTION
#define GTK_FILE_SELECTION(widget) ((GtkFileSelection *) (widget))
#endif

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

static inline gchar *gtk_set_locale(void)
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

static inline void gtk_rc_init(void)
{
}

#endif /* GTK_CHECK_VERSION(3, 0, 0) */

#endif /* XMMS_GTK_COMPAT_WIDGETS_H */
