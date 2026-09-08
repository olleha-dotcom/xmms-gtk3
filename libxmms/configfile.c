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
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "configfile.h"
#include <locale.h>

static ConfigSection *xmms_cfg_create_section(ConfigFile * cfg, char * name);
static ConfigLine *xmms_cfg_create_string(ConfigSection * section, char * key, char * value);
static ConfigSection *xmms_cfg_find_section(ConfigFile * cfg, char * name);
static ConfigLine *xmms_cfg_find_string(ConfigSection * section, char * key);

ConfigFile *xmms_cfg_new(void)
{
	ConfigFile *cfg;

	cfg = g_malloc0(sizeof (ConfigFile));

	return cfg;
}

static gchar *xmms_cfg_read_file(gchar *filename)
{
	int fd, saved_errno;
	struct stat stats;
	FILE *file = NULL;
	GString *buffer = NULL;
	gchar chunk[4096];
	gsize count;

	/* Follow regular-file symlinks, but never block opening a FIFO. Read to
	 * EOF from this descriptor instead of sizing a buffer from a path stat.
	 */
	fd = g_open(filename, O_RDONLY | O_NONBLOCK, 0);
	if (fd == -1 || fstat(fd, &stats) != 0)
		goto failure;
	if (!S_ISREG(stats.st_mode))
	{
		errno = EINVAL;
		goto failure;
	}
	file = fdopen(fd, "r");
	if (!file)
		goto failure;
	fd = -1;
	buffer = g_string_new(NULL);
	while ((count = fread(chunk, 1, sizeof(chunk), file)) != 0)
		g_string_append_len(buffer, chunk, count);
	if (ferror(file))
		goto failure;
	if (fclose(file) != 0)
	{
		file = NULL;
		goto failure;
	}
	return g_string_free(buffer, FALSE);

 failure:
	saved_errno = errno;
	if (file)
		fclose(file);
	if (fd != -1)
		close(fd);
	if (buffer)
		g_string_free(buffer, TRUE);
	g_warning("xmms_cfg_open_file: failed to read '%s': %s", filename,
		  g_strerror(saved_errno ? saved_errno : EIO));
	return NULL;
}

ConfigFile *xmms_cfg_open_file(gchar * filename)
{
	ConfigFile *cfg;

	char *buffer, **lines, *tmp;
	int i;
	ConfigSection *section = NULL;

	g_return_val_if_fail(filename != NULL, NULL);

	buffer = xmms_cfg_read_file(filename);
	if (!buffer)
		return NULL;

	cfg = g_malloc0(sizeof (ConfigFile));
	lines = g_strsplit(buffer, "\n", 0);
	g_free(buffer);
	i = 0;
	while (lines[i])
	{
		if (lines[i][0] == '[')
		{
			if ((tmp = strchr(lines[i], ']')))
			{
				*tmp = '\0';
				section = xmms_cfg_create_section(cfg, &lines[i][1]);
			}
		}
		else if (lines[i][0] != '#' && section)
		{
			if ((tmp = strchr(lines[i], '=')))
			{
				*tmp = '\0';
				tmp++;
				xmms_cfg_create_string(section, lines[i], tmp);
			}
		}
		i++;
	}
	g_strfreev(lines);
	return cfg;
}

gchar * xmms_cfg_get_default_filename(void)
{
	static char *filename = NULL;
	if (!filename)
		filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
	return filename;
}

ConfigFile * xmms_cfg_open_default_file(void)
{
	ConfigFile *ret;

	ret = xmms_cfg_open_file(xmms_cfg_get_default_filename());
	if (!ret)
		ret = xmms_cfg_new();
	return ret;
}

gboolean xmms_cfg_write_file(ConfigFile * cfg, gchar * filename)
{
	FILE *file = NULL;
	gchar *directory, *temporary;
	struct stat stats;
	mode_t mode = 0600;
	int fd = -1, saved_errno;
	gboolean success = FALSE;
	gboolean temporary_created = FALSE;
	GList *section_list, *line_list;
	ConfigSection *section;
	ConfigLine *line;

	g_return_val_if_fail(cfg != NULL, FALSE);
	g_return_val_if_fail(filename != NULL, FALSE);

	/* Never follow a link or replace a device/FIFO with a config file. */
	if (g_lstat(filename, &stats) == 0)
	{
		if (!S_ISREG(stats.st_mode))
		{
			g_warning("xmms_cfg_write_file: refusing non-regular file '%s'", filename);
			return FALSE;
		}
		mode = stats.st_mode & 0777;
	}
	else if (errno != ENOENT)
	{
		g_warning("xmms_cfg_write_file: failed to stat '%s': %s", filename, g_strerror(errno));
		return FALSE;
	}

	directory = g_path_get_dirname(filename);
	temporary = g_build_filename(directory, ".xmms-config-XXXXXX", NULL);
	g_free(directory);
	fd = g_mkstemp(temporary);
	if (fd == -1)
		goto out;
	temporary_created = TRUE;
	file = fdopen(fd, "w");
	if (!file)
		goto out;
	fd = -1; /* Owned by the stream from here on. */

	section_list = cfg->sections;
	while (section_list)
	{
		section = (ConfigSection *) section_list->data;
		if (section->lines)
		{
			if (fprintf(file, "[%s]\n", section->name) < 0)
				goto out;
			line_list = section->lines;
			while (line_list)
			{
				line = (ConfigLine *) line_list->data;
				if (fprintf(file, "%s=%s\n", line->key, line->value) < 0)
					goto out;
				line_list = g_list_next(line_list);
			}
			if (fprintf(file, "\n") < 0)
				goto out;
		}
		section_list = g_list_next(section_list);
	}
	if (fflush(file) != 0 || ferror(file) ||
	    fchmod(fileno(file), mode) != 0 || fsync(fileno(file)) != 0)
		goto out;
	if (fclose(file) != 0)
	{
		file = NULL;
		goto out;
	}
	file = NULL;
	/* This is the commit point; no failure before it touches the old file. */
	if (g_rename(temporary, filename) != 0)
		goto out;
	success = TRUE;

 out:
	saved_errno = errno;
	if (file)
		fclose(file);
	if (fd != -1)
		close(fd);
	if (!success)
	{
		if (temporary_created)
			g_unlink(temporary);
		g_warning("xmms_cfg_write_file: failed to save '%s': %s", filename,
			  g_strerror(saved_errno ? saved_errno : EIO));
	}
	g_free(temporary);
	return success;
}

gboolean xmms_cfg_write_default_file(ConfigFile * cfg)
{
	return xmms_cfg_write_file(cfg, xmms_cfg_get_default_filename());
}

gboolean xmms_cfg_read_string(ConfigFile * cfg, gchar * section, gchar * key, gchar ** value)
{
	ConfigSection *sect;
	ConfigLine *line;

	g_return_val_if_fail(cfg != NULL, FALSE);
	g_return_val_if_fail(section != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	if (!(sect = xmms_cfg_find_section(cfg, section)))
		return FALSE;
	if (!(line = xmms_cfg_find_string(sect, key)))
		return FALSE;
	*value = g_strdup(line->value);
	return TRUE;
}

gboolean xmms_cfg_read_int(ConfigFile * cfg, gchar * section, gchar * key, gint * value)
{
	char *str;

	g_return_val_if_fail(cfg != NULL, FALSE);
	g_return_val_if_fail(section != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	if (!xmms_cfg_read_string(cfg, section, key, &str))
		return FALSE;
	*value = atoi(str);
	g_free(str);

	return TRUE;
}

gboolean xmms_cfg_read_boolean(ConfigFile * cfg, gchar * section, gchar * key, gboolean * value)
{
	char *str;

	g_return_val_if_fail(cfg != NULL, FALSE);
	g_return_val_if_fail(section != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	if (!xmms_cfg_read_string(cfg, section, key, &str))
		return FALSE;
	if (!strcasecmp(str, "TRUE"))
		*value = TRUE;
	else
		*value = FALSE;
	g_free(str);
	return TRUE;
}

gboolean xmms_cfg_read_float(ConfigFile * cfg, gchar * section, gchar * key, gfloat * value)
{
	char *str, *locale;

	g_return_val_if_fail(cfg != NULL, FALSE);
	g_return_val_if_fail(section != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	if (!xmms_cfg_read_string(cfg, section, key, &str))
		return FALSE;

	locale = g_strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "C");
	*value = strtod(str, NULL);
	setlocale(LC_NUMERIC, locale);
	g_free(locale);
	g_free(str);

	return TRUE;
}

gboolean xmms_cfg_read_double(ConfigFile * cfg, gchar * section, gchar * key, gdouble * value)
{
	char *str, *locale;

	g_return_val_if_fail(cfg != NULL, FALSE);
	g_return_val_if_fail(section != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	if (!xmms_cfg_read_string(cfg, section, key, &str))
		return FALSE;

	locale = g_strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "C");
	*value = strtod(str, NULL);
	setlocale(LC_NUMERIC, locale);
	g_free(locale);
	g_free(str);

	return TRUE;
}

void xmms_cfg_write_string(ConfigFile * cfg, gchar * section, gchar * key, gchar * value)
{
	ConfigSection *sect;
	ConfigLine *line;

	g_return_if_fail(cfg != NULL);
	g_return_if_fail(section != NULL);
	g_return_if_fail(key != NULL);
	g_return_if_fail(value != NULL);
	
	sect = xmms_cfg_find_section(cfg, section);
	if (!sect)
		sect = xmms_cfg_create_section(cfg, section);
	if ((line = xmms_cfg_find_string(sect, key)))
	{
		g_free(line->value);
		line->value = g_strchug(g_strchomp(g_strdup(value)));
	}
	else
		xmms_cfg_create_string(sect, key, value);
}

void xmms_cfg_write_int(ConfigFile * cfg, gchar * section, gchar * key, gint value)
{
	char *strvalue;

	g_return_if_fail(cfg != NULL);
	g_return_if_fail(section != NULL);
	g_return_if_fail(key != NULL);

	strvalue = g_strdup_printf("%d", value);
	xmms_cfg_write_string(cfg, section, key, strvalue);
	g_free(strvalue);
}

void xmms_cfg_write_boolean(ConfigFile * cfg, gchar * section, gchar * key, gboolean value)
{
	g_return_if_fail(cfg != NULL);
	g_return_if_fail(section != NULL);
	g_return_if_fail(key != NULL);

	if (value)
		xmms_cfg_write_string(cfg, section, key, "TRUE");
	else
		xmms_cfg_write_string(cfg, section, key, "FALSE");
}

void xmms_cfg_write_float(ConfigFile * cfg, gchar * section, gchar * key, gfloat value)
{
	char *strvalue, *locale;

	g_return_if_fail(cfg != NULL);
	g_return_if_fail(section != NULL);
	g_return_if_fail(key != NULL);

	locale = g_strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "C");
	strvalue = g_strdup_printf("%g", value);
	setlocale(LC_NUMERIC, locale);
	xmms_cfg_write_string(cfg, section, key, strvalue);
	g_free(locale);
	g_free(strvalue);
}

void xmms_cfg_write_double(ConfigFile * cfg, gchar * section, gchar * key, gdouble value)
{
	char *strvalue, *locale;

	g_return_if_fail(cfg != NULL);
	g_return_if_fail(section != NULL);
	g_return_if_fail(key != NULL);

	locale = g_strdup(setlocale(LC_NUMERIC, NULL));
	setlocale(LC_NUMERIC, "C");
	strvalue = g_strdup_printf("%g", value);
	setlocale(LC_NUMERIC, locale);
	xmms_cfg_write_string(cfg, section, key, strvalue);
	g_free(locale);
	g_free(strvalue);
}

void xmms_cfg_remove_key(ConfigFile * cfg, gchar * section, gchar * key)
{
	ConfigSection *sect;
	ConfigLine *line;

	g_return_if_fail(cfg != NULL);
	g_return_if_fail(section != NULL);
	g_return_if_fail(key != NULL);

	if ((sect = xmms_cfg_find_section(cfg, section)) != NULL)
	{
		if ((line = xmms_cfg_find_string(sect, key)) != NULL)
		{
			g_free(line->key);
			g_free(line->value);
			g_free(line);
			sect->lines = g_list_remove(sect->lines, line);
		}
	}
}

void xmms_cfg_free(ConfigFile * cfg)
{
	ConfigSection *section;
	ConfigLine *line;
	GList *section_list, *line_list;

	if (cfg == NULL)
		return;

	section_list = cfg->sections;
	while (section_list)
	{
		section = (ConfigSection *) section_list->data;
		g_free(section->name);

		line_list = section->lines;
		while (line_list)
		{
			line = (ConfigLine *) line_list->data;
			g_free(line->key);
			g_free(line->value);
			g_free(line);
			line_list = g_list_next(line_list);
		}
		g_list_free(section->lines);
		g_free(section);

		section_list = g_list_next(section_list);
	}
	g_list_free(cfg->sections);
	g_free(cfg);
}

static ConfigSection *xmms_cfg_create_section(ConfigFile * cfg, char * name)
{
	ConfigSection *section;

	section = g_malloc0(sizeof (ConfigSection));
	section->name = g_strdup(name);
	cfg->sections = g_list_append(cfg->sections, section);

	return section;
}

static ConfigLine *xmms_cfg_create_string(ConfigSection * section, char * key, char * value)
{
	ConfigLine *line;

	line = g_malloc0(sizeof (ConfigLine));
	line->key = g_strchug(g_strchomp(g_strdup(key)));
	line->value = g_strchug(g_strchomp(g_strdup(value)));
	section->lines = g_list_append(section->lines, line);

	return line;
}

static ConfigSection *xmms_cfg_find_section(ConfigFile * cfg, char * name)
{
	ConfigSection *section;
	GList *list;

	list = cfg->sections;
	while (list)
	{
		section = (ConfigSection *) list->data;
		if (!strcasecmp(section->name, name))
			return section;
		list = g_list_next(list);
	}
	return NULL;
}

static ConfigLine *xmms_cfg_find_string(ConfigSection * section, char * key)
{
	ConfigLine *line;
	GList *list;

	list = section->lines;
	while (list)
	{
		line = (ConfigLine *) list->data;
		if (!strcasecmp(line->key, key))
			return line;
		list = g_list_next(list);
	}
	return NULL;
}
