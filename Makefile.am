ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

NULL =

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--without-elogind  \
	--without-systemd

SUBDIRS = \
	src      \
	savers   \
	data     \
	doc      \
	po       \
	$(NULL)

EXTRA_DIST = \
	autogen.sh           \
	COPYING              \
	COPYING.LIB          \
	COPYING.LGPL         \
	AUTHORS              \
	INSTALL              \
	ChangeLog            \
	NEWS                 \
	$(NULL)

MAINTAINERCLEANFILES = \
	*~             \
	compile        \
	configure      \
	INSTALL        \
	COPYING        \
	install-sh     \
	missing        \
	mkinstalldirs  \
	config.guess   \
	ltmain.sh      \
	config.sub     \
	depcomp        \
	Makefile.in    \
	config.h.*     \
	aclocal.m4     \
	acinclude.m4   \
	$(NULL)

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then        \
	    GIT_DIR="$(top_srcdir)/.git" git log --stat > $@;  \
	fi

dist: ChangeLog

.PHONY: ChangeLog

distuninstallcheck_listfiles = 						\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache
