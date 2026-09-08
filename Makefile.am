SUBDIRS = intl libxmms xmms Output Input Effect General Visualization tests

bin_SCRIPTS = xmms-config

CONFIGURE_DEPENDENCIES = $(srcdir)/VERSION

gtk_compat_headers = $(patsubst $(srcdir)/%,%,$(wildcard $(srcdir)/gtk_compat/*.h))

EXTRA_DIST = xmms.spec xmms.spec.in FAQ xmms.m4 VERSION gtk_compat.h \
	$(gtk_compat_headers) GTK3_PORTING.md LICENSE \
	packaging/build-deb.sh packaging/runtime-deps.sh packaging/check-deb.sh \
	packaging/source-excludes packaging/xmms.desktop \
	packaging/postinst packaging/postrm \
	tests/run-sanitizers.sh .github/workflows/ci.yml

m4datadir = $(datadir)/aclocal
m4data_DATA = xmms.m4

man_MANS = xmms.1

.PHONY: check-sanitizers
check-sanitizers:
	bash "$(srcdir)/tests/run-sanitizers.sh"
