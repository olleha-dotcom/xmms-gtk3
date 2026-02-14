/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2002  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2002  Haavard Kvaalen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "config.h"

#include "../xmms/i18n.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>

#include <string.h>

#include "gtk_compat.h"

enum
{
	COL_NAME = 0,
	COL_PATH,
	COL_SCANNED,
	COL_DUMMY,
	N_COLUMNS
};

static gboolean check_for_subdir(const char *path)
{
	DIR *dir;
	struct dirent *dirent;
	struct stat statbuf;
	char *npath;

	dir = opendir(path);
	if (!dir)
		return FALSE;

	while ((dirent = readdir(dir)) != NULL)
	{
		if (dirent->d_name[0] == '.')
			continue;

		npath = g_strconcat(path, dirent->d_name, NULL);
		if (stat(npath, &statbuf) != -1 && S_ISDIR(statbuf.st_mode))
		{
			g_free(npath);
			closedir(dir);
			return TRUE;
		}
		g_free(npath);
	}

	closedir(dir);
	return FALSE;
}

static void add_dummy_child(GtkTreeStore *store, GtkTreeIter *parent)
{
	GtkTreeIter child;

	gtk_tree_store_append(store, &child, parent);
	gtk_tree_store_set(store, &child,
			   COL_NAME, "",
			   COL_PATH, NULL,
			   COL_SCANNED, TRUE,
			   COL_DUMMY, TRUE,
			   -1);
}

static void add_dir(GtkTreeStore *store, GtkTreeIter *parent, const char *parent_path, const char *dir_name)
{
	struct stat statbuf;
	gboolean has_subdir;
	char *path;
	char *path_with_slash;
	GtkTreeIter node;

	if (!dir_name || dir_name[0] == '.')
		return;

	path = g_strconcat(parent_path, dir_name, NULL);
	if (stat(path, &statbuf) == -1 || !S_ISDIR(statbuf.st_mode))
	{
		g_free(path);
		return;
	}

	path_with_slash = g_strconcat(path, "/", NULL);
	has_subdir = check_for_subdir(path_with_slash);

	gtk_tree_store_append(store, &node, parent);
	gtk_tree_store_set(store, &node,
			   COL_NAME, dir_name,
			   COL_PATH, path_with_slash,
			   COL_SCANNED, FALSE,
			   COL_DUMMY, FALSE,
			   -1);

	if (has_subdir)
		add_dummy_child(store, &node);

	g_free(path_with_slash);
	g_free(path);
}

static void ensure_node_scanned(GtkTreeStore *store, GtkTreeIter *parent)
{
	gboolean scanned = FALSE;
	gboolean dummy = FALSE;
	char *parent_path = NULL;
	GtkTreeIter child;
	gboolean valid;

	gtk_tree_model_get(GTK_TREE_MODEL(store), parent,
			   COL_SCANNED, &scanned,
			   COL_DUMMY, &dummy,
			   COL_PATH, &parent_path,
			   -1);

	if (dummy || scanned || !parent_path)
	{
		g_free(parent_path);
		return;
	}

	valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child, parent);
	while (valid)
	{
		gboolean child_dummy = FALSE;
		GtkTreeIter next = child;

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &next);
		gtk_tree_model_get(GTK_TREE_MODEL(store), &child, COL_DUMMY, &child_dummy, -1);
		if (child_dummy)
			gtk_tree_store_remove(store, &child);
		child = next;
	}

	{
		DIR *dir = opendir(parent_path);
		if (dir)
		{
			struct dirent *dirent;
			while ((dirent = readdir(dir)) != NULL)
				add_dir(store, parent, parent_path, dirent->d_name);
			closedir(dir);
		}
	}

	gtk_tree_store_set(store, parent, COL_SCANNED, TRUE, -1);
	g_free(parent_path);
}

static void row_expanded_cb(GtkTreeView *view, GtkTreeIter *iter, GtkTreePath *path, gpointer data)
{
	GtkTreeStore *store;

	(void) path;
	(void) data;

	store = GTK_TREE_STORE(gtk_tree_view_get_model(view));
	ensure_node_scanned(store, iter);
}

static void row_activated_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	void (*handler) (char *);
	char *dir_path = NULL;

	(void) column;
	(void) data;

	model = gtk_tree_view_get_model(view);
	if (!gtk_tree_model_get_iter(model, &iter, path))
		return;

	handler = g_object_get_data(G_OBJECT(view), "xmms-handler");
	if (!handler)
		return;

	gtk_tree_model_get(model, &iter, COL_PATH, &dir_path, -1);
	if (dir_path)
	{
		handler(dir_path);
		g_free(dir_path);
	}
}

static void show_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeView *view = GTK_TREE_VIEW(data);
	GtkTreePath *selected_path;

	(void) widget;

	selected_path = g_object_get_data(G_OBJECT(view), "selected_path");
	if (!selected_path)
		return;

	gtk_tree_view_expand_to_path(view, selected_path);
	gtk_tree_view_scroll_to_cell(view, selected_path, NULL, TRUE, 0.6, 0.0);
}

static void emit_selected_path(GtkTreeModel *model, GtkTreeIter *iter, void (*handler) (char *))
{
	char *dir_path = NULL;

	gtk_tree_model_get(model, iter, COL_PATH, &dir_path, -1);
	if (dir_path)
	{
		handler(dir_path);
		g_free(dir_path);
	}
}

static void ok_clicked(GtkWidget *widget, GtkWidget *tree)
{
	GtkWidget *window;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	void (*handler) (char *);
	GtkTreeIter iter;

	window = g_object_get_data(G_OBJECT(widget), "xmms-window");
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
	handler = g_object_get_data(G_OBJECT(tree), "xmms-handler");

	if (handler)
	{
		if (gtk_tree_selection_get_mode(selection) == GTK_SELECTION_MULTIPLE)
		{
			GList *rows = gtk_tree_selection_get_selected_rows(selection, NULL);
			GList *l;
			for (l = rows; l != NULL; l = l->next)
			{
				GtkTreePath *path = l->data;
				if (gtk_tree_model_get_iter(model, &iter, path))
					emit_selected_path(model, &iter, handler);
			}
			g_list_free_full(rows, (GDestroyNotify) gtk_tree_path_free);
		}
		else if (gtk_tree_selection_get_selected(selection, &model, &iter))
			emit_selected_path(model, &iter, handler);
	}

	if (window)
		gtk_widget_destroy(window);
}

static gboolean find_child_by_name(GtkTreeStore *store, GtkTreeIter *parent, const char *name, GtkTreeIter *out)
{
	GtkTreeIter child;
	gboolean valid;

	valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child, parent);
	while (valid)
	{
		gboolean dummy = FALSE;
		char *child_name = NULL;

		gtk_tree_model_get(GTK_TREE_MODEL(store), &child,
				   COL_DUMMY, &dummy,
				   COL_NAME, &child_name,
				   -1);
		if (!dummy && child_name && strcmp(child_name, name) == 0)
		{
			*out = child;
			g_free(child_name);
			return TRUE;
		}

		g_free(child_name);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &child);
	}

	return FALSE;
}

GtkWidget *xmms_create_dir_browser(char *title, char *current_path, GtkSelectionMode mode, void (*handler) (char *))
{
	GtkWidget *window, *scroll_win, *tree, *vbox, *bbox, *ok, *cancel, *sep;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter root_iter;
	GtkTreePath *selected_path;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	scroll_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_win, TRUE, TRUE, 0);

	store = gtk_tree_store_new(N_COLUMNS,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_BOOLEAN,
				   G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), COL_NAME, GTK_SORT_ASCENDING);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "text", COL_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, mode);

	g_signal_connect(tree, "row-expanded", G_CALLBACK(row_expanded_cb), NULL);
	g_signal_connect(tree, "row-activated", G_CALLBACK(row_activated_cb), NULL);
	g_signal_connect(window, "show", G_CALLBACK(show_cb), tree);
	g_object_set_data(G_OBJECT(tree), "xmms-handler", handler);

	gtk_container_add(GTK_CONTAINER(scroll_win), tree);

	gtk_tree_store_append(store, &root_iter, NULL);
	gtk_tree_store_set(store, &root_iter,
			   COL_NAME, "/",
			   COL_PATH, "/",
			   COL_SCANNED, FALSE,
			   COL_DUMMY, FALSE,
			   -1);
	if (check_for_subdir("/"))
		add_dummy_child(store, &root_iter);

	selected_path = NULL;
	if (current_path && *current_path)
	{
		char **parts = g_strsplit(current_path, "/", 0);
		GtkTreeIter current_iter = root_iter;
		int i;

		for (i = 0; parts[i] != NULL; i++)
		{
			GtkTreeIter child;
			if (parts[i][0] == '\0')
				continue;

			ensure_node_scanned(store, &current_iter);
			if (!find_child_by_name(store, &current_iter, parts[i], &child))
				break;
			current_iter = child;
		}
		selected_path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &current_iter);
		g_strfreev(parts);
	}
	if (!selected_path)
		selected_path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &root_iter);

	gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree), selected_path);
	gtk_tree_selection_select_path(selection, selected_path);
	g_object_set_data_full(G_OBJECT(tree), "selected_path", selected_path,
			       (GDestroyNotify) gtk_tree_path_free);

	sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

	bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 5);

	ok = gtk_button_new_with_label(_("OK"));
	g_object_set_data(G_OBJECT(ok), "xmms-window", window);
	gtk_widget_set_can_default(ok, TRUE);
	gtk_window_set_default(GTK_WINDOW(window), ok);
	gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 0);
	g_signal_connect(ok, "clicked", G_CALLBACK(ok_clicked), tree);

	cancel = gtk_button_new_with_label(_("Cancel"));
	gtk_widget_set_can_default(cancel, TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
	g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_widget_destroy), window);

	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	gtk_widget_show_all(window);
	return window;
}
