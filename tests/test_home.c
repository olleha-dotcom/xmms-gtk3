/* Test-only preload: cc -shared -fPIC -Wall -Wextra -Werror
 * tests/test_home.c -ldl -o /tmp/test_home.so
 * Never preload this into a normal session. HOME is deliberately unchanged. */
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *test_home;

__attribute__((constructor)) static void check_test_home(void)
{
	struct stat st;
	const char *active = getenv("XMMS_GUI_TEST_ACTIVE");
	const char *home = getenv("HOME");
	test_home = getenv("XMMS_TEST_HOME");
	if (!active || strcmp(active, "1") || !test_home || test_home[0] != '/' ||
	    (home && !strcmp(home, test_home)) || lstat(test_home, &st) ||
	    !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 077))
	{
		static const char message[] = "test_home: refusing non-private test home\n";
		ssize_t written = write(STDERR_FILENO, message, sizeof(message) - 1);
		(void) written;
		_exit(77);
	}
}

const char *g_get_home_dir(void)
{
	if (!test_home)
		check_test_home();
	return test_home;
}

/* Input-disable preferences do not prevent plugin init(), and output plugins
 * have no common disable flag. Block enumeration before any plugin can load.
 * These are all six roots used by init_plugins()/scan_plugins_from_build_tree.
 * GTK's own module/locale/font directories are not blocked. */
DIR *opendir(const char *path)
{
	static DIR *(*real_opendir)(const char *);
	static const char *const blocked[] = {
		"Plugins", "Input", "Output", "Effect", "General", "Visualization"
	};
	const char *base = strrchr(path, '/');
	size_t i;
	base = base ? base + 1 : path;
	for (i = 0; i < sizeof(blocked) / sizeof(blocked[0]); i++)
		if (!strcmp(base, blocked[i]))
		{
			errno = ENOENT;
			return NULL;
		}
	if (!real_opendir)
		real_opendir = (DIR *(*)(const char *)) dlsym(RTLD_NEXT, "opendir");
	if (!real_opendir)
	{
		errno = ENOSYS;
		return NULL;
	}
	return real_opendir(path);
}
