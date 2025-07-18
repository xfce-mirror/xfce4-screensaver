NULL =

AM_CPPFLAGS = \
	-I.                                      \
	-I$(srcdir)                              \
	-DLIBEXECDIR=\""$(libexecdir)"\"         \
	-DXFCELOCALEDIR=\""$(datadir)/locale"\"  \
	-DDATADIR=\""$(datadir)"\"               \
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

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

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

libgs_theme_engine_a_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(NULL)

libgs_theme_engine_a_SOURCES = \
	$(BUILT_SOURCES)   \
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

floaters_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(NULL)

floaters_LDADD = \
	libgs-theme-engine.a \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(NULL)

popsquares_SOURCES = \
	gste-popsquares.c  \
	gste-popsquares.h  \
	popsquares.c       \
	$(NULL)

popsquares_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(NULL)

popsquares_LDADD = \
	libgs-theme-engine.a \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(NULL)

slideshow_SOURCES = \
	gste-slideshow.c       \
	gste-slideshow.h       \
	xdg-user-dir-lookup.c  \
	xdg-user-dir-lookup.h  \
	slideshow.c            \
	$(NULL)

slideshow_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(NULL)

slideshow_LDADD = \
	libgs-theme-engine.a \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(NULL)

EXTRA_DIST = \
	meson.build                   \
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
