AM_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir) -I$(top_builddir)/intl @GTK_CFLAGS@
LDADD = @GTK_LIBS@

check_PROGRAMS = test_bmp test_config test_ui_scale test_playback
TESTS = test_bmp test_config test_ui_scale test_playback
test_bmp_SOURCES = test_bmp.c ../xmms/bmp.c
test_config_SOURCES = test_config.c ../libxmms/configfile.c
test_ui_scale_SOURCES = test_ui_scale.c
test_playback_SOURCES = test_playback.c

if HAVE_PULSE
check_PROGRAMS += test_pulse
test_pulse_SOURCES = test_pulse.c ../Output/pulse/audio.c
test_pulse_CPPFLAGS = $(AM_CPPFLAGS) @PULSE_CFLAGS@
test_pulse_LDADD = @GTK_LIBS@ @PULSE_LIBS@
TESTS += run-pulse-test.sh
endif

if HAVE_GUI_TESTS
check_PROGRAMS += test_gui
test_gui_SOURCES = test_gui.c
test_gui_CPPFLAGS = $(AM_CPPFLAGS) @TEST_X11_CFLAGS@
test_gui_LDADD = $(top_builddir)/libxmms/libxmms.la @GTK_LIBS@ @TEST_X11_LIBS@
check_DATA = test_home.so
test_home.so: $(srcdir)/test_home.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -shared -fPIC -o $@ $< -ldl
TESTS += run-gui-test.sh
endif

TESTS_ENVIRONMENT = XMMS_TEST_BUILDDIR='$(abs_builddir)'; export XMMS_TEST_BUILDDIR; \
	XMMS_GUI_BUILD_READY=1; export XMMS_GUI_BUILD_READY;
CLEANFILES = test_home.so
EXTRA_DIST = run-pulse-test.sh run-sanitizers.sh run-gui-test.sh test_home.c
