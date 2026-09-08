#ifndef XMMS_GTK_COMPAT_SIGNALS_H
#define XMMS_GTK_COMPAT_SIGNALS_H

#include "types.h"

#if GTK_CHECK_VERSION(3, 0, 0)

/* GTK1/GTK2 legacy object and signal shims */
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

#ifndef g_thread_init
#define g_thread_init(vtable) ((void) 0)
#endif

#endif /* GTK_CHECK_VERSION(3, 0, 0) */

#endif /* XMMS_GTK_COMPAT_SIGNALS_H */
