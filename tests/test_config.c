#include "libxmms/configfile.h"

#include <fcntl.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct
{
	gchar *directory;
	gchar *filename;
	ConfigFile *cfg;
} Fixture;

static void setup(Fixture *f, gconstpointer unused)
{
	(void)unused;
	f->directory = g_dir_make_tmp("xmms-config-test-XXXXXX", NULL);
	g_assert_nonnull(f->directory);
	f->filename = g_build_filename(f->directory, "config", NULL);
	f->cfg = xmms_cfg_new();
	xmms_cfg_write_int(f->cfg, "xmms", "volume", 75);
}

static void teardown(Fixture *f, gconstpointer unused)
{
	(void)unused;
	xmms_cfg_free(f->cfg);
	g_unlink(f->filename);
	g_assert_cmpint(g_rmdir(f->directory), ==, 0);
	g_free(f->filename);
	g_free(f->directory);
}

static void assert_no_temporary_files(Fixture *f)
{
	GDir *dir = g_dir_open(f->directory, 0, NULL);
	const gchar *name;
	g_assert_nonnull(dir);
	while ((name = g_dir_read_name(dir)))
		g_assert_false(g_str_has_prefix(name, ".xmms-config-"));
	g_dir_close(dir);
}

static void assert_volume(const gchar *path, gint expected)
{
	ConfigFile *loaded = xmms_cfg_open_file((gchar *)path);
	gint value = -1;
	g_assert_nonnull(loaded);
	g_assert_true(xmms_cfg_read_int(loaded, "xmms", "volume", &value));
	g_assert_cmpint(value, ==, expected);
	xmms_cfg_free(loaded);
}

static void test_roundtrip(Fixture *f, gconstpointer unused)
{
	ConfigFile *loaded;
	gchar *string = NULL;
	gboolean enabled = FALSE;
	gdouble number = 0;
	struct stat stats;
	mode_t old_umask;
	(void)unused;
	xmms_cfg_write_string(f->cfg, "xmms", "skin", "/tmp/skin with spaces");
	xmms_cfg_write_string(f->cfg, "xmms", "empty", "");
	xmms_cfg_write_boolean(f->cfg, "audio", "enabled", TRUE);
	xmms_cfg_write_double(f->cfg, "audio", "gain", 1.25);
	old_umask = umask(0);
	g_assert_true(xmms_cfg_write_file(f->cfg, f->filename));
	umask(old_umask);
	g_assert_cmpint(stat(f->filename, &stats), ==, 0);
	g_assert_cmpint(stats.st_mode & 0777, ==, 0600);
	assert_volume(f->filename, 75);
	loaded = xmms_cfg_open_file(f->filename);
	g_assert_nonnull(loaded);
	g_assert_true(xmms_cfg_read_string(loaded, "xmms", "skin", &string));
	g_assert_cmpstr(string, ==, "/tmp/skin with spaces");
	g_free(string);
	g_assert_true(xmms_cfg_read_string(loaded, "xmms", "empty", &string));
	g_assert_cmpstr(string, ==, "");
	g_free(string);
	g_assert_true(xmms_cfg_read_boolean(loaded, "audio", "enabled", &enabled));
	g_assert_true(enabled);
	g_assert_true(xmms_cfg_read_double(loaded, "audio", "gain", &number));
	g_assert_cmpfloat(number, ==, 1.25);
	xmms_cfg_free(loaded);
	assert_no_temporary_files(f);
}

static void test_atomic_replace(Fixture *f, gconstpointer unused)
{
	struct stat before, after;
	gchar *original;
	gsize length;
	gchar buffer[128] = { 0 };
	int fd;
	(void)unused;
	g_assert_true(xmms_cfg_write_file(f->cfg, f->filename));
	g_assert_cmpint(g_chmod(f->filename, 0640), ==, 0);
	g_assert_true(g_file_get_contents(f->filename, &original, &length, NULL));
	fd = g_open(f->filename, O_RDONLY, 0);
	g_assert_cmpint(fd, >=, 0);
	g_assert_cmpint(fstat(fd, &before), ==, 0);
	xmms_cfg_write_int(f->cfg, "xmms", "volume", 25);
	g_assert_true(xmms_cfg_write_file(f->cfg, f->filename));
	g_assert_cmpint(stat(f->filename, &after), ==, 0);
	g_assert_true(before.st_ino != after.st_ino);
	g_assert_cmpint(after.st_mode & 0777, ==, 0640);
	/* An already open reader must still see the complete old inode. */
	g_assert_cmpint(read(fd, buffer, sizeof(buffer)), ==, (gssize)length);
	g_assert_cmpmem(buffer, length, original, length);
	g_assert_cmpint(close(fd), ==, 0);
	assert_volume(f->filename, 25);
	g_free(original);
	assert_no_temporary_files(f);
}

static void test_write_failure(Fixture *f, gconstpointer data)
{
	struct stat before, after;
	gchar *original, *current, *large;
	gsize original_length, current_length;
	pid_t child;
	int status;
	g_assert_true(xmms_cfg_write_file(f->cfg, f->filename));
	g_assert_true(g_file_get_contents(f->filename, &original, &original_length, NULL));
	g_assert_cmpint(stat(f->filename, &before), ==, 0);
	xmms_cfg_write_int(f->cfg, "xmms", "volume", 25);
	large = g_strnfill(GPOINTER_TO_UINT(data), 'x');
	xmms_cfg_write_string(f->cfg, "xmms", "payload", large);
	g_free(large);
	child = fork();
	g_assert_cmpint(child, >=, 0);
	if (child == 0)
	{
		struct rlimit limit;
		gboolean result;
		/* A small value fails at fflush; a large one fails inside fprintf. */
		g_assert_cmpint(getrlimit(RLIMIT_FSIZE, &limit), ==, 0);
		limit.rlim_cur = GPOINTER_TO_UINT(data) > 8192 ? 512 : 0;
		g_assert_true(signal(SIGXFSZ, SIG_IGN) != SIG_ERR);
		g_assert_cmpint(setrlimit(RLIMIT_FSIZE, &limit), ==, 0);
		g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*failed to save*");
		result = xmms_cfg_write_file(f->cfg, f->filename);
		g_test_assert_expected_messages();
		_exit(result ? 1 : 0);
	}
	g_assert_cmpint(waitpid(child, &status, 0), ==, child);
	g_assert_true(WIFEXITED(status));
	g_assert_cmpint(WEXITSTATUS(status), ==, 0);
	g_assert_true(g_file_get_contents(f->filename, &current, &current_length, NULL));
	g_assert_cmpmem(current, current_length, original, original_length);
	g_assert_cmpint(stat(f->filename, &after), ==, 0);
	g_assert_true(before.st_ino == after.st_ino);
	g_assert_cmpint(before.st_mode, ==, after.st_mode);
	assert_volume(f->filename, 75);
	assert_no_temporary_files(f);
	g_free(current);
	g_free(original);
}

static void test_symlinks(Fixture *f, gconstpointer unused)
{
	gchar *link = g_build_filename(f->directory, "link", NULL);
	gchar *long_value = g_strnfill(16384, 'a');
	gchar *value = NULL;
	ConfigFile *loaded;
	struct stat stats;
	(void)unused;
	xmms_cfg_write_string(f->cfg, "xmms", "long", long_value);
	g_assert_true(xmms_cfg_write_file(f->cfg, f->filename));
	g_assert_cmpint(symlink("config", link), ==, 0);
	assert_volume(link, 75);
	loaded = xmms_cfg_open_file(link);
	g_assert_nonnull(loaded);
	g_assert_true(xmms_cfg_read_string(loaded, "xmms", "long", &value));
	g_assert_cmpstr(value, ==, long_value);
	g_free(value);
	g_free(long_value);
	xmms_cfg_free(loaded);
	xmms_cfg_write_int(f->cfg, "xmms", "volume", 25);
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*refusing non-regular file*");
	g_assert_false(xmms_cfg_write_file(f->cfg, link));
	g_test_assert_expected_messages();
	g_assert_cmpint(g_lstat(link, &stats), ==, 0);
	g_assert_true(S_ISLNK(stats.st_mode));
	assert_volume(f->filename, 75);
	/* A symlink longer than its target contents must also read correctly. */
	g_assert_true(g_file_set_contents(f->filename, "", 0, NULL));
	loaded = xmms_cfg_open_file(link);
	g_assert_nonnull(loaded);
	xmms_cfg_free(loaded);
	g_assert_cmpint(g_unlink(f->filename), ==, 0);
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*failed to read*");
	g_assert_null(xmms_cfg_open_file(link));
	g_test_assert_expected_messages();
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*refusing non-regular file*");
	g_assert_false(xmms_cfg_write_file(f->cfg, link));
	g_test_assert_expected_messages();
	g_assert_cmpint(g_unlink(link), ==, 0);
	g_free(link);
	assert_no_temporary_files(f);
}

static void test_bad_paths(Fixture *f, gconstpointer unused)
{
	gchar *missing = g_build_filename(f->directory, "missing", "config", NULL);
	(void)unused;
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*failed to save*");
	g_assert_false(xmms_cfg_write_file(f->cfg, missing));
	g_test_assert_expected_messages();
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*failed to read*");
	g_assert_null(xmms_cfg_open_file(f->directory));
	g_test_assert_expected_messages();
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*refusing non-regular file*");
	g_assert_false(xmms_cfg_write_file(f->cfg, f->directory));
	g_test_assert_expected_messages();
	g_assert_cmpint(mkfifo(f->filename, 0600), ==, 0);
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*failed to read*");
	g_assert_null(xmms_cfg_open_file(f->filename));
	g_test_assert_expected_messages();
	g_test_expect_message(NULL, G_LOG_LEVEL_WARNING, "*refusing non-regular file*");
	g_assert_false(xmms_cfg_write_file(f->cfg, f->filename));
	g_test_assert_expected_messages();
	g_free(missing);
	assert_no_temporary_files(f);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add("/config/roundtrip", Fixture, NULL, setup, test_roundtrip, teardown);
	g_test_add("/config/atomic-replace", Fixture, NULL, setup, test_atomic_replace, teardown);
	g_test_add("/config/flush-failure", Fixture, GUINT_TO_POINTER(16), setup, test_write_failure, teardown);
	g_test_add("/config/write-failure", Fixture, GUINT_TO_POINTER(65536), setup, test_write_failure, teardown);
	g_test_add("/config/symlinks", Fixture, NULL, setup, test_symlinks, teardown);
	g_test_add("/config/bad-paths", Fixture, NULL, setup, test_bad_paths, teardown);
	return g_test_run();
}
