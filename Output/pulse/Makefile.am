pulsesources = \
pulse.c pulse.h \
about.c \
audio.c \
configure.c \
init.c

if HAVE_PULSE

pulseltlibs = libpulseout.la
pulsebuildsources = $(pulsesources)

else

pulseltlibs =
pulsebuildsources =

endif

EXTRA_DIST = $(pulsesources)

lib_LTLIBRARIES = $(pulseltlibs)

libdir = $(plugindir)/$(OUTPUT_PLUGIN_DIR)

libpulseout_la_LDFLAGS = @PLUGIN_LDFLAGS@
libpulseout_la_LIBADD = @GTK_LIBS@ @PULSE_LIBS@
libpulseout_la_SOURCES = $(pulsebuildsources)
INCLUDES = @GTK_CFLAGS@ @PULSE_CFLAGS@ -I$(top_builddir)/intl -I$(top_srcdir)
