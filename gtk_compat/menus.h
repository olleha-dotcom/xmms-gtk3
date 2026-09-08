#ifndef XMMS_GTK_COMPAT_MENUS_H
#define XMMS_GTK_COMPAT_MENUS_H

#include "types.h"
#include <string.h>

#if GTK_CHECK_VERSION(3, 0, 0)

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

static inline void gtk_menu_append(GtkMenu *menu, GtkWidget *child)
{
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), child);
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

#endif /* GTK_CHECK_VERSION(3, 0, 0) */

#endif /* XMMS_GTK_COMPAT_MENUS_H */
