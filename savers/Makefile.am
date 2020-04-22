NULL =

AM_CPPFLAGS = \
	-I.                                      \
	-I$(srcdir)                              \
	$(XFCE_SCREENSAVER_SAVER_CFLAGS)         \
	$(DISABLE_DEPRECATED_CFLAGS)             \
	-DLIBEXECDIR=\""$(libexecdir)"\"         \
	-DXFCELOCALEDIR=\""$(datadir)/locale"\"  \
	-DDATADIR=\""$(datadir)"\"               \
	$(WARN_CFLAGS)                           \
	$(NULL)

DESKTOP_IN_IN_FILES = \
	xfce-floaters.desktop.in.in            \
	xfce-personal-slideshow.desktop.in.in  \
	xfce-popsquares.desktop.in.in          \
	$(NULL)

xfce-floaters.desktop.in: xfce-floaters.desktop.in.in Makefile
	$(edit) $< >$@
xfce-personal-slideshow.desktop.in: xfce-personal-slideshow.desktop.in.in Makefile
	$(edit) $< >$@
xfce-popsquares.desktop.in: xfce-popsquares.desktop.in.in Makefile
	$(edit) $< >$@

@INTLTOOL_DESKTOP_RULE@

edit = sed \
	-e 's|@FLOATERS_XFCE_IMAGE[@]|$(FLOATERS_XFCE_IMAGE)|g'  \
	-e 's|@SAVERDIR[@]|$(privlibexecdir)|g'

themesdir = $(datadir)/applications/screensavers
themes_in_files = \
	xfce-floaters.desktop.in            \
	xfce-personal-slideshow.desktop.in  \
	xfce-popsquares.desktop.in          \
	$(NULL)
themes_DATA = $(themes_in_files:.desktop.in=.desktop)

BUILT_SOURCES = \
	gs-theme-engine-marshal.c  \
	gs-theme-engine-marshal.h  \
	$(NULL)

gs-theme-engine-marshal.c: gs-theme-engine-marshal.list
	echo "#include \"gs-theme-engine-marshal.h\"" > $@ && \
	@GLIB_GENMARSHAL@ $< --prefix=gs_theme_engine_marshal --body >> $@

gs-theme-engine-marshal.h: gs-theme-engine-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gs_theme_engine_marshal --header > $@

noinst_LIBRARIES = \
	libgs-theme-engine.a

libgs_theme_engine_a_CPPFLAGS = \
	$(XFCE_SCREENSAVER_SAVER_CFLAGS)  \
	-DDATADIR=\""$(datadir)"\"        \
	$(NULL)

libgs_theme_engine_a_SOURCES = \
	$(BUILT_SOURCES)   \
	gs-theme-window.h  \
	gs-theme-window.c  \
	gs-theme-engine.c  \
	gs-theme-engine.h  \
	$(NULL)

saverdir = $(libexecdir)/xfce4-screensaver
saver_PROGRAMS = \
	floaters    \
	popsquares  \
	slideshow   \
	$(NULL)

floaters_SOURCES = \
	floaters.c  \
	$(NULL)

floaters_LDADD = \
	libgs-theme-engine.a            \
	$(XFCE_SCREENSAVER_SAVER_LIBS)  \
	-lm                             \
	$(NULL)

popsquares_SOURCES = \
	gste-popsquares.c  \
	gste-popsquares.h  \
	popsquares.c       \
	$(NULL)

popsquares_LDADD = \
	libgs-theme-engine.a            \
	$(XFCE_SCREENSAVER_SAVER_LIBS)  \
	$(NULL)

slideshow_SOURCES = \
	gste-slideshow.c       \
	gste-slideshow.h       \
	xdg-user-dir-lookup.c  \
	xdg-user-dir-lookup.h  \
	slideshow.c            \
	$(NULL)

slideshow_LDADD = \
	libgs-theme-engine.a            \
	$(XFCE_SCREENSAVER_SAVER_LIBS)  \
	$(NULL)

EXTRA_DIST = \
	gs-theme-engine-marshal.list  \
	$(DESKTOP_IN_IN_FILES)        \
	$(themes_in_files)            \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)                    \
	xfce-floaters.desktop.in            \
	xfce-popsquares.desktop.in          \
	xfce-personal-slideshow.desktop.in  \
	$(NULL)

DISTCLEANFILES = \
	$(themes_DATA)  \
	$(NULL)

MAINTAINERCLEANFILES = \
	*~                               \
	Makefile.in                      \
	xfce-floaters.desktop            \
	xfce-personal-slideshow.desktop  \
	xfce-popsquares.desktop          \
	$(NULL)
