/* Real X11 integration driver. Only run via run-gui-test.sh on its own Xvfb.
 * cc -std=c99 -Wall -Wextra -Werror -I. tests/test_gui.c
 *   $(pkg-config --cflags --libs x11 xtst glib-2.0) -Llibxmms/.libs -lxmms
 *   -Wl,-rpath,"$PWD/libxmms/.libs" -o /tmp/test_gui
 * Requires the parent to build XMMS and libxmms first. */
#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libxmms/xmmsctrl.h"
#include "libxmms/configfile.h"

static Display *display;

static void fail(const char *message)
{
	fprintf(stderr, "GUI FAIL: %s\n", message);
	exit(1);
}

static void deadline(int signo)
{
	static const char message[] = "GUI FAIL: test deadline exceeded\n";
	ssize_t written;
	(void) signo;
	written = write(STDERR_FILENO, message, sizeof(message) - 1);
	(void) written;
	_exit(1);
}

static int isolated(void)
{
	const char *active = getenv("XMMS_GUI_TEST_ACTIVE");
	const char *home = getenv("XMMS_TEST_HOME");
	const char *tmp = getenv("TMPDIR");
	const char *wanted_display = getenv("XMMS_TEST_DISPLAY");
	const char *actual_display = getenv("DISPLAY");
	return active && !strcmp(active, "1") && home && tmp &&
		!strcmp(g_get_home_dir(), home) && !strcmp(g_get_tmp_dir(), tmp) &&
		wanted_display && actual_display &&
		!strcmp(wanted_display, actual_display) &&
		strlen(tmp) + strlen(g_get_user_name()) + 10 < 108;
}

static Window find_window(const char *name, int device_scale)
{
	Window root, parent, *children = NULL, result = None;
	unsigned int count = 0, i;
	if (!XQueryTree(display, DefaultRootWindow(display), &root, &parent, &children, &count))
		return None;
	for (i = 0; i < count; i++)
	{
		XClassHint hint = { NULL, NULL };
		XWindowAttributes attr;
		if (XGetClassHint(display, children[i], &hint))
		{
			if (hint.res_name && !strcmp(hint.res_name, name) &&
			    XGetWindowAttributes(display, children[i], &attr) && attr.map_state == IsViewable)
			{
				char *title = NULL;
				XSizeHints sizes = { 0 };
				long supplied = 0;
				int has_sizes;
				/* check_pposition() can leave its mapped 1x1-request probe
				 * alive without a WM. It shares both class and title with
				 * the player; distinguish it by the requested size hints,
				 * not the actual geometry that this test must validate. */
				has_sizes = XGetWMNormalHints(display, children[i], &sizes, &supplied);
				if (!strcmp(name, "XMMS_Player") && has_sizes &&
				    (sizes.flags & PMinSize) && sizes.min_width == device_scale &&
				    sizes.min_height == device_scale)
				{
					if (hint.res_name) XFree(hint.res_name);
					if (hint.res_class) XFree(hint.res_class);
					continue;
				}
				XFetchName(display, children[i], &title);
				fprintf(stderr, "GUI window 0x%lx class=%s title=%s size=%dx%d min=%dx%d\n",
					children[i], name, title ? title : "(none)", attr.width, attr.height,
					sizes.min_width, sizes.min_height);
				if (title) XFree(title);
				if (!result) result = children[i];
			}
			if (hint.res_name) XFree(hint.res_name);
			if (hint.res_class) XFree(hint.res_class);
		}
	}
	if (children) XFree(children);
	return result;
}

static void geometry(Window window, int width, int height)
{
	XWindowAttributes attr = { 0 };
	int attempt;
	for (attempt = 0; attempt < 150; attempt++)
	{
		if (XGetWindowAttributes(display, window, &attr) && attr.map_state == IsViewable &&
		    attr.width == width && attr.height == height)
			return;
		g_usleep(20000);
	}
	fprintf(stderr, "expected geometry %dx%d, got %dx%d (map=%d)\n",
		width, height, attr.width, attr.height, attr.map_state);
	fail("window geometry did not settle");
}

static void click(Window window, int x, int y)
{
	Window child;
	int root_x, root_y;
	XRaiseWindow(display, window);
	if (!XTranslateCoordinates(display, window, DefaultRootWindow(display),
				   x, y, &root_x, &root_y, &child))
		fail("cannot translate click coordinates");
	XTestFakeMotionEvent(display, DefaultScreen(display), root_x, root_y, CurrentTime);
	XSync(display, False);
	g_usleep(30000);
	XTestFakeButtonEvent(display, 1, True, CurrentTime);
	XFlush(display);
	g_usleep(30000);
	XTestFakeButtonEvent(display, 1, False, CurrentTime);
	XSync(display, False);
	g_usleep(100000);
}

static void toggle_doublesize(Window mainwin)
{
	KeyCode ctrl = XKeysymToKeycode(display, XK_Control_L);
	KeyCode key = XKeysymToKeycode(display, XK_d);
	if (!ctrl || !key) fail("missing Ctrl+D keycodes");
	XRaiseWindow(display, mainwin);
	XSetInputFocus(display, mainwin, RevertToParent, CurrentTime);
	XSync(display, False);
	XTestFakeKeyEvent(display, ctrl, True, CurrentTime);
	XTestFakeKeyEvent(display, key, True, CurrentTime);
	XTestFakeKeyEvent(display, key, False, CurrentTime);
	XTestFakeKeyEvent(display, ctrl, False, CurrentTime);
	XSync(display, False);
}

static uint64_t on_pixels(Window window, int factor)
{
	XImage *image = XGetImage(display, window, 14 * factor, 18 * factor,
				 25 * factor, 12 * factor, AllPlanes, ZPixmap);
	uint64_t hash = UINT64_C(14695981039346656037);
	int x, y;
	if (!image) fail("cannot read EQ On pixels");
	for (y = 0; y < image->height; y++)
		for (x = 0; x < image->width; x++)
			hash = (hash ^ XGetPixel(image, x, y)) * UINT64_C(1099511628211);
	XDestroyImage(image);
	return hash;
}

static uint64_t wait_on_pixels(Window window, int factor, uint64_t old, int changed)
{
	int attempt;
	uint64_t hash = old;
	for (attempt = 0; attempt < 100; attempt++)
	{
		hash = on_pixels(window, factor);
		if ((hash != old) == changed) return hash;
		g_usleep(20000);
	}
	fail("EQ On rendering did not match the click");
	return hash;
}

static void eq_on_boundaries(Window equalizer, int factor)
{
	uint64_t before, selected;
	XRaiseWindow(display, equalizer);
	XSync(display, False);
	g_usleep(150000);
	before = on_pixels(equalizer, factor);
	/* Physical pixels just outside, then exactly on the half-open hitbox. */
	click(equalizer, 14 * factor - 1, 24 * factor);
	if (on_pixels(equalizer, factor) != before) fail("EQ On accepted its left outside edge");
	click(equalizer, 20 * factor, 18 * factor - 1);
	if (on_pixels(equalizer, factor) != before) fail("EQ On accepted its top outside edge");
	click(equalizer, 14 * factor, 18 * factor);
	selected = wait_on_pixels(equalizer, factor, before, 1);
	click(equalizer, 39 * factor - 1, 30 * factor - 1);
	(void) wait_on_pixels(equalizer, factor, selected, 1);
	if (on_pixels(equalizer, factor) != before) fail("EQ On did not toggle back");
}

static void wait_preamp(float expected)
{
	int attempt;
	float actual = 0;
	for (attempt = 0; attempt < 100; attempt++)
	{
		actual = xmms_remote_get_eq_preamp(0);
		if (actual > expected - 0.01f && actual < expected + 0.01f) return;
		g_usleep(20000);
	}
	fprintf(stderr, "expected preamp %.1f, got %.1f\n", expected, actual);
	fail("EQ slider click did not reach remote state");
}

static void eq_slider(Window equalizer, int factor)
{
	xmms_remote_set_eq_preamp(0, 0);
	wait_preamp(0);
	click(equalizer, 28 * factor, 43 * factor);
	wait_preamp(20);
	click(equalizer, 28 * factor, 93 * factor);
	wait_preamp(-20);
	xmms_remote_set_eq_preamp(0, 0);
	wait_preamp(0);
}

static void playlist_visible(Window playlist, int wanted)
{
	XWindowAttributes attr;
	int attempt;
	for (attempt = 0; attempt < 100; attempt++)
	{
		if (!!xmms_remote_is_pl_win(0) == wanted &&
		    XGetWindowAttributes(display, playlist, &attr) &&
		    (attr.map_state == IsViewable) == wanted) return;
		g_usleep(20000);
	}
	fail("main PL hitbox did not change playlist visibility");
}

static void run_case(int device_scale, int linked)
{
	Window mainwin = None, equalizer = None, playlist = None;
	int attempt, ds;
	for (attempt = 0; attempt < 300; attempt++)
	{
		if (xmms_remote_is_running(0) && xmms_remote_is_eq_win(0) && xmms_remote_is_pl_win(0))
		{
			mainwin = find_window("XMMS_Player", device_scale);
			equalizer = find_window("XMMS_Equalizer", device_scale);
			playlist = find_window("XMMS_Playlist", device_scale);
			if (mainwin && equalizer && playlist) break;
		}
		g_usleep(20000);
	}
	if (!mainwin || !equalizer || !playlist) fail("XMMS windows/controlsocket did not become ready");
	if (xmms_remote_get_playlist_length(0) || xmms_remote_is_playing(0))
		fail("private session was not empty and stopped");
	for (ds = 0; ds <= 1; ds++)
	{
		int main_factor = device_scale * (ds ? 2 : 1);
		int eq_factor = device_scale * (ds && linked ? 2 : 1);
		fprintf(stderr, "GUI checks: GDK_SCALE=%d EQ-linked=%d doublesize=%d\n",
			device_scale, linked, ds);
		geometry(mainwin, 275 * main_factor, 116 * main_factor);
		geometry(equalizer, 275 * eq_factor, 116 * eq_factor);
		geometry(playlist, 300 * device_scale, 232 * device_scale);
		eq_on_boundaries(equalizer, eq_factor);
		eq_slider(equalizer, eq_factor);
		click(mainwin, 253 * main_factor, 64 * main_factor);
		playlist_visible(playlist, 0);
		click(mainwin, 253 * main_factor, 64 * main_factor);
		playlist_visible(playlist, 1);
		geometry(playlist, 300 * device_scale, 232 * device_scale);
		toggle_doublesize(mainwin);
	}
	geometry(mainwin, 275 * device_scale, 116 * device_scale);
	geometry(equalizer, 275 * device_scale, 116 * device_scale);
	/* No remote API exposes equalizer_active. Verify the final On state from
	 * the private config after clean shutdown, in addition to pixel assertions. */
	click(equalizer, 26 * device_scale, 24 * device_scale);
	xmms_remote_quit(0);
	printf("GUI case passed: GDK_SCALE=%d, EQ linked=%d\n", device_scale, linked);
}

int main(int argc, char **argv)
{
	int event, error, major, minor;
	if (!isolated())
	{
		fputs("SKIP: use run-gui-test.sh with its private home/display\n", stderr);
		return 77;
	}
	signal(SIGALRM, deadline);
	alarm(45);
	if (argc == 2 && !strcmp(argv[1], "--check-home"))
	{
		DIR *dir;
		struct stat st;
		char *path = g_build_filename(g_get_home_dir(), ".xmms", "Plugins", NULL);
		if (stat(path, &st) || !S_ISDIR(st.st_mode)) fail("test plugin directory is missing");
		dir = opendir(path);
		g_free(path);
		if (dir) { closedir(dir); fail("plugin-directory guard is not active"); }
		dir = opendir(g_get_home_dir());
		if (!dir) fail("preload also blocked the ordinary home directory");
		closedir(dir);
		puts("Private home and plugin guard verified; HOME unchanged.");
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "--verify-config"))
	{
		ConfigFile *config = xmms_cfg_open_default_file();
		gboolean active = FALSE;
		if (!config || !xmms_cfg_read_boolean(config, "xmms", "equalizer_active", &active) || !active)
			fail("final EQ On state was not saved in the private config");
		xmms_cfg_free(config);
		return 0;
	}
	display = XOpenDisplay(NULL);
	if (!display) { fputs("SKIP: private Xvfb display unavailable\n", stderr); return 77; }
	if (!XTestQueryExtension(display, &event, &error, &major, &minor))
	{
		fputs("SKIP: XTEST extension unavailable\n", stderr);
		XCloseDisplay(display);
		return 77;
	}
	if (argc == 2 && !strcmp(argv[1], "--probe")) { XCloseDisplay(display); return 0; }
	if (argc != 3 || (strcmp(argv[1], "1") && strcmp(argv[1], "2")) ||
	    (strcmp(argv[2], "0") && strcmp(argv[2], "1"))) fail("expected device-scale (1/2) and EQ-linked (0/1)");
	run_case(atoi(argv[1]), atoi(argv[2]));
	XCloseDisplay(display);
	return 0;
}
