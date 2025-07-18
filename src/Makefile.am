NULL =

saverdir = $(libexecdir)/xfce4-screensaver
themesdir = $(pkgdatadir)/themes

AM_CPPFLAGS = \
	-I.                                         \
	-I$(srcdir)                                 \
	-DPREFIX=\""$(prefix)"\"                    \
	-DBINDIR=\""$(bindir)"\"                    \
	-DLIBDIR=\""$(libdir)"\"                    \
	-DLIBEXECDIR=\""$(libexecdir)"\"            \
	-DDATADIR=\""$(datadir)"\"                  \
	-DSYSCONFDIR=\""$(sysconfdir)"\"            \
	-DXFCELOCALEDIR=\""$(datadir)/locale"\"     \
	-DLINUXPAMLOCALEDIR=\""$(linux_pam_localedir)"\" \
	-DSAVERDIR=\""$(saverdir)"\"                \
	-DTHEMESDIR=\""$(themesdir)"\"              \
	-DGTKBUILDERDIR=\"$(pkgdatadir)\"           \
	-DPAM_SERVICE_NAME=\""xfce4-screensaver"\"  \
	$(NULL)

bin_PROGRAMS = \
	xfce4-screensaver              \
	xfce4-screensaver-command      \
	xfce4-screensaver-preferences  \
	$(NULL)

bin_SCRIPTS = \
	xfce4-screensaver-configure.py    \
	$(NULL)

libexec_PROGRAMS = \
	xfce4-screensaver-dialog     \
	$(NULL)

noinst_PROGRAMS = \
	test-passwd  \
	test-window  \
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = xfce4-screensaver.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

xfce4-screensaver.desktop: xfce4-screensaver.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

xfce4_screensaver_command_SOURCES = \
	xfce4-screensaver-command.c  \
	$(NULL)

xfce4_screensaver_command_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(NULL)

xfce4_screensaver_command_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(NULL)

AUTH_SOURCES = \
	gs-auth.h                \
	gs-auth-@AUTH_SCHEME@.c  \
	$(NULL)

test_passwd_SOURCES = \
	test-passwd.c    \
	$(AUTH_SOURCES)  \
	setuid.c         \
	setuid.h         \
	subprocs.c       \
	subprocs.h       \
	$(NULL)

test_passwd_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(AUTH_CFLAGS) \
	$(NULL)

test_passwd_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(AUTH_LIBS) \
	$(NULL)

test_window_SOURCES = \
	test-window.c    \
	gs-window.h      \
	gs-window.c      \
	gs-marshal.c     \
	gs-marshal.h     \
	gs-debug.c       \
	gs-debug.h       \
	gs-prefs.c       \
	gs-prefs.h       \
	subprocs.c       \
	subprocs.h       \
	xfce-desktop-utils.c \
	xfce-desktop-utils.h \
	gs-manager.c \
	gs-manager.h \
	gs-theme-manager.c \
	gs-theme-manager.h \
	gs-job.c \
	gs-job.h \
	$(NULL)

test_window_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(GARCON_GTK3_CFLAGS) \
	$(NULL)

test_window_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(GARCON_GTK3_LIBS) \
	$(NULL)

if ENABLE_X11
test_window_SOURCES += \
	gs-grab.c \
	gs-grab.h \
	$(NULL)

test_window_CFLAGS += \
	$(LIBX11_CFLAGS) \
	$(LIBXEXT_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(NULL)

test_window_LDADD += \
	$(LIBX11_LIBS) \
	$(LIBXEXT_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(GARCON_LIBS) \
	$(NULL)
endif

if ENABLE_WAYLAND
test_window_SOURCES += \
	gs-session-lock-manager.c \
	gs-session-lock-manager.h \
	$(NULL)

test_window_CFLAGS += \
	$(LIBWLEMBED_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(NULL)

test_window_LDADD += \
	$(top_builddir)/protocols/libprotocols.la \
	$(LIBWLEMBED_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(WAYLAND_CLIENT_LIBS) \
	$(NULL)
endif

xfce4_screensaver_built_sources = \
	xfce4-screensaver-resources.c  \
	$(NULL)

xfce4_screensaver_dialog_SOURCES = \
	$(xfce4_screensaver_built_sources)  \
	xfce4-screensaver-dialog.c                 \
	gs-lock-plug.c                             \
	gs-lock-plug.h                             \
	gs-debug.c                                 \
	gs-debug.h                                 \
	gs-prefs.c                                 \
	gs-prefs.h                                 \
	setuid.c                                   \
	setuid.h                                   \
	subprocs.c                                 \
	subprocs.h                                 \
	xfce-desktop-utils.c                       \
	xfce-desktop-utils.h                       \
	xfce-bg.c                                  \
	xfce-bg.h                                  \
	$(AUTH_SOURCES)                            \
	$(NULL)

EXTRA_xfce4_screensaver_dialog_SOURCES = \
	gs-auth-helper.c            \
	gs-auth-pam.c               \
	gs-auth-pwent.c             \
	gs-auth-bsdauth.c           \
	$(NULL)

xfce4_screensaver_dialog_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(AUTH_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(NULL)

xfce4_screensaver_dialog_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS) \
	$(AUTH_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(NULL)

if ENABLE_X11
xfce4_screensaver_dialog_SOURCES += \
	xfcekbd-indicator.c \
	xfcekbd-indicator.h \
	$(NULL)

xfce4_screensaver_dialog_CFLAGS += \
	$(LIBX11_CFLAGS) \
	$(LIBXEXT_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(NULL)

xfce4_screensaver_dialog_LDADD += \
	$(LIBX11_LIBS) \
	$(LIBXEXT_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(NULL)
endif

BUILT_SOURCES = \
	gs-marshal.c                 \
	gs-marshal.h                 \
	$(NULL)

gs-marshal.c: gs-marshal.list
	echo "#include \"gs-marshal.h\"" > $@ &&  \
	@GLIB_GENMARSHAL@ $< --prefix=gs_marshal --body >> $@

gs-marshal.h: gs-marshal.list
	@GLIB_GENMARSHAL@ $< --prefix=gs_marshal --header > $@

xfce4_screensaver_SOURCES = \
	xfce4-screensaver.c         \
	xfce4-screensaver.h         \
	gs-monitor.c                \
	gs-monitor.h                \
	gs-listener-dbus.c          \
	gs-listener-dbus.h          \
	gs-listener.c               \
	gs-listener.h               \
	gs-manager.c                \
	gs-manager.h                \
	gs-window.c                 \
	gs-window.h                 \
	gs-prefs.c                  \
	gs-prefs.h                  \
	gs-theme-manager.c          \
	gs-theme-manager.h          \
	gs-job.c                    \
	gs-job.h                    \
	gs-debug.c                  \
	gs-debug.h                  \
	subprocs.c                  \
	subprocs.h                  \
	xfce-desktop-utils.c        \
	xfce-desktop-utils.h        \
	$(BUILT_SOURCES)            \
	$(NULL)

xfce4_screensaver_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(GARCON_GTK3_CFLAGS) \
	$(SYSTEMD_CFLAGS) \
	$(ELOGIND_CFLAGS) \
	$(NULL)

xfce4_screensaver_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS) \
	$(GARCON_LIBS) \
	$(GARCON_GTK3_LIBS) \
	$(SYSTEMD_LIBS) \
	$(ELOGIND_LIBS) \
	$(NULL)

if ENABLE_X11
xfce4_screensaver_SOURCES += \
	gs-listener-x11.c \
	gs-listener-x11.h \
	gs-grab.c \
	gs-grab.h \
	$(NULL)

xfce4_screensaver_CFLAGS += \
	$(LIBX11_CFLAGS) \
	$(LIBXEXT_CFLAGS) \
	$(LIBXSCRNSAVER_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(NULL)

xfce4_screensaver_LDADD += \
	$(LIBX11_LIBS) \
	$(LIBXEXT_LIBS) \
	$(LIBXSCRNSAVER_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(LIBWNCK_LIBS) \
	$(NULL)
endif

if ENABLE_WAYLAND
xfce4_screensaver_SOURCES += \
	gs-session-lock-manager.c \
	gs-session-lock-manager.h \
	gs-listener-wayland.c \
	gs-listener-wayland.h \
	$(NULL)

xfce4_screensaver_CFLAGS += \
	$(LIBWLEMBED_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(LIBXFCE4WINDOWING_CFLAGS) \
	$(NULL)

xfce4_screensaver_LDADD += \
	$(top_builddir)/protocols/libprotocols.la \
	$(LIBWLEMBED_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(WAYLAND_CLIENT_LIBS) \
	$(LIBXFCE4WINDOWING_LIBS) \
	$(NULL)
endif

xfce4_screensaver_LDFLAGS = -export-dynamic

xfce4_screensaver_preferences_SOURCES = \
	$(xfce4_screensaver_built_sources)  \
	xfce4-screensaver-preferences.c                 \
	gs-theme-manager.c                              \
	gs-theme-manager.h                              \
	gs-job.c                                        \
	gs-job.h                                        \
	gs-prefs.c                                      \
	gs-prefs.h                                      \
	gs-debug.c                                      \
	gs-debug.h                                      \
	subprocs.c                                      \
	subprocs.h                                      \
	xfce-desktop-utils.c                            \
	xfce-desktop-utils.h                            \
	$(NULL)

xfce4_screensaver_preferences_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(GARCON_GTK3_CFLAGS) \
	$(GARCON_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(LIBWLEMBED_CFLAGS) \
	$(LIBWLEMBED_GTK3_CFLAGS) \
	$(NULL)

xfce4_screensaver_preferences_LDADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS) \
	$(GARCON_LIBS) \
	$(GARCON_GTK3_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(LIBWLEMBED_LIBS) \
	$(LIBWLEMBED_GTK3_LIBS) \
	$(NULL)

xfce4_screensaver_resources_deps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/xfce4-screensaver.gresource.xml)

xfce4-screensaver-resources.c: $(srcdir)/xfce4-screensaver.gresource.xml $(xfce4_screensaver_resources_deps) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<

DISTCLEANFILES = \
	$(xfce4_screensaver_built_sources)       \
	$(NULL)

BUILT_SOURCES += \
	$(xfce4_screensaver_built_sources)       \
	$(NULL)

EXTRA_DIST = \
	meson.build                       \
	debug-screensaver.sh              \
	gs-marshal.list                   \
	xfce4-screensaver.desktop.in      \
	xfce4-screensaver-configure.py    \
	xfce4-screensaver-dialog.css      \
	xfce4-screensaver-dialog.ui       \
	xfce4-screensaver.gresource.xml   \
	xfce4-screensaver-preferences.ui  \
	$(NULL)

CLEANFILES = \
	$(desktop_DATA)               \
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	*~           \
	Makefile.in

install-exec-hook:
	@if [ "x@NEED_SETUID@" = "xyes" ]; then                                                 \
	    echo "***" ;                                                                        \
	    echo "***  Warning: xfce4-screensaver has been compiled with support for" ;         \
	    if [ "x@AUTH_SCHEME@" = "xbsdauth" ]; then                                          \
	        echo "***           bsd_auth(3) and must be installed as a setuid root" ;       \
	    else                                                                                \
	        echo "***           shadow passwords. If your system actually uses shadow" ;    \
	        echo "***           passwords then it must be installed as a setuid root" ;     \
	    fi;                                                                                 \
	    echo "***           program in order for locking to work. To do this, you" ;        \
	    echo "***           must run:" ;                                                    \
	    echo "***" ;                                                                        \
	    echo "***           chown root $(DESTDIR)$(libexecdir)/xfce4-screensaver-dialog" ;  \
	    echo "***           chmod +s $(DESTDIR)$(libexecdir)/xfce4-screensaver-dialog" ;    \
	    echo "***" ;                                                                        \
	    echo "***           For now, it will be installed non-setuid, which" ;              \
	    echo "***           means that locking might not work." ;                           \
	    echo "***" ;                                                                        \
	fi
