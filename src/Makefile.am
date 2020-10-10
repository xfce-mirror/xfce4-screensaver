NULL =

saverdir = $(libexecdir)/xfce4-screensaver
themesdir = $(pkgdatadir)/themes

AM_CPPFLAGS = \
	-I.                                         \
	-I$(srcdir)                                 \
	-DXFCEMENU_I_KNOW_THIS_IS_UNSTABLE          \
	$(XFCE_SCREENSAVER_CFLAGS)                  \
	$(XFCE_SCREENSAVER_DIALOG_CFLAGS)           \
	$(XFCE_SCREENSAVER_CAPPLET_CFLAGS)          \
	$(DISABLE_DEPRECATED_CFLAGS)                \
	-DPREFIX=\""$(prefix)"\"                    \
	-DBINDIR=\""$(bindir)"\"                    \
	-DLIBDIR=\""$(libdir)"\"                    \
	-DLIBEXECDIR=\""$(libexecdir)"\"            \
	-DDATADIR=\""$(datadir)"\"                  \
	-DSYSCONFDIR=\""$(sysconfdir)"\"            \
	-DXFCELOCALEDIR=\""$(datadir)/locale"\"     \
	-DSAVERDIR=\""$(saverdir)"\"                \
	-DTHEMESDIR=\""$(themesdir)"\"              \
	-DGTKBUILDERDIR=\"$(pkgdatadir)\"           \
	-DPAM_SERVICE_NAME=\""xfce4-screensaver"\"  \
	$(WARN_CFLAGS)                              \
	$(AUTH_CFLAGS)                              \
	$(DEBUG_CFLAGS)                             \
	$(DBUS_CFLAGS)                              \
	$(LIBXFCEKBDUI_CFLAGS)                      \
	$(LIBXKLAVIER_CFLAGS)                       \
	$(SYSTEMD_CFLAGS)                           \
	$(ELOGIND_CFLAGS)                           \
	$(WNCK_LIBS)								\
	$(NULL)

bin_PROGRAMS = \
	xfce4-screensaver              \
	xfce4-screensaver-command      \
	xfce4-screensaver-preferences  \
	$(NULL)

bin_SCRIPTS = \
	xfce4-screensaver-configure    \
	$(NULL)

libexec_PROGRAMS = \
	xfce4-screensaver-dialog     \
	xfce4-screensaver-gl-helper  \
	$(NULL)

noinst_PROGRAMS = \
	test-passwd  \
	test-window  \
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = xfce4-screensaver.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

xfce4_screensaver_command_SOURCES = \
	xfce4-screensaver-command.c  \
	$(NULL)

xfce4_screensaver_command_LDADD = \
	$(XFCE_SCREENSAVER_COMMAND_LIBS)  \
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

test_passwd_LDADD = \
	$(XFCE_SCREENSAVER_DIALOG_LIBS)  \
	$(AUTH_LIBS)                     \
	$(NULL)

test_window_SOURCES = \
	test-window.c    \
	gs-window.h      \
	gs-window-x11.c  \
	gs-grab-x11.c    \
	gs-grab.h        \
	gs-marshal.c     \
	gs-marshal.h     \
	gs-debug.c       \
	gs-debug.h       \
	gs-prefs.c       \
	gs-prefs.h       \
	subprocs.c       \
	subprocs.h       \
	$(NULL)

test_window_LDADD = \
	$(XFCE_SCREENSAVER_LIBS)  \
	$(SAVER_LIBS)             \
	$(NULL)

xfce4_screensaver_dialog_built_sources = \
	xfce4-screensaver-dialog-css.h  \
	xfce4-screensaver-dialog-ui.h

xfce4_screensaver_dialog_SOURCES = \
	$(xfce4_screensaver_dialog_built_sources)  \
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
	xfcekbd-indicator.c                        \
	xfcekbd-indicator.h                        \
	xfce-bg.c                                  \
	xfce-bg.h                                  \
	$(AUTH_SOURCES)                            \
	$(NULL)

EXTRA_xfce4_screensaver_dialog_SOURCES = \
	gs-auth-helper.c            \
	gs-auth-pam.c               \
	gs-auth-pwent.c             \
	gs-auth-bsdauth.c           \
	xfce-desktop-utils.c        \
	xfce-desktop-utils.h        \
	xfcekbd-indicator.c         \
	xfcekbd-indicator.h         \
	$(NULL)

xfce4_screensaver_dialog_LDADD = \
	$(XFCE_SCREENSAVER_DIALOG_LIBS)  \
	$(XFCE_SCREENSAVER_LIBS)         \
	$(SAVER_LIBS)                    \
	$(AUTH_LIBS)                     \
	$(LIBXFCEKBDUI_LIBS)             \
	$(NULL)

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
	gs-listener-x11.c           \
	gs-listener-x11.h           \
	gs-manager.c                \
	gs-manager.h                \
	gs-window-x11.c             \
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
	gs-grab-x11.c               \
	gs-grab.h                   \
	xfce-bg.c                   \
	xfce-bg.h                   \
	xfce-desktop-utils.c        \
	xfce-desktop-utils.h        \
	xfcekbd-indicator.c         \
	xfcekbd-indicator.h         \
	$(BUILT_SOURCES)            \
	$(NULL)

xfce4_screensaver_gl_helper_LDADD = \
	$(XFCE_SCREENSAVER_LIBS)  \
	$(GL_LIBS)                \
	$(NULL)

xfce4_screensaver_gl_helper_SOURCES = \
	xfce4-screensaver-gl-helper.c  \
	gs-visual-gl.c                 \
	gs-visual-gl.h                 \
	$(NULL)

xfce4_screensaver_LDADD = \
	$(XFCE_SCREENSAVER_LIBS)  \
	$(SAVER_LIBS)             \
	$(SYSTEMD_LIBS)           \
	$(ELOGIND_LIBS)           \
	$(WNCK_LIBS)			  \
	$(NULL)

xfce4_screensaver_LDFLAGS = -export-dynamic

xfce4_screensaver_preferences_built_sources = \
	xfce4-screensaver-preferences-ui.h

xfce4_screensaver_preferences_SOURCES = \
	$(xfce4_screensaver_preferences_built_sources)  \
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
	xfcekbd-indicator.c                             \
	xfcekbd-indicator.h                             \
	$(NULL)

xfce4_screensaver_preferences_LDADD = \
	$(XFCE_SCREENSAVER_CAPPLET_LIBS)  \
	$(NULL)

if MAINTAINER_MODE

xfce4-screensaver-dialog-css.h: $(srcdir)/xfce4-screensaver-dialog.css Makefile
	$(AM_V_GEN) xdt-csource --static --name=xfce4_screensaver_dialog_css $< >$@

xfce4-screensaver-dialog-ui.h: $(srcdir)/xfce4-screensaver-dialog.ui Makefile
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=xfce4_screensaver_dialog_ui $< >$@

xfce4-screensaver-preferences-ui.h: $(srcdir)/xfce4-screensaver-preferences.ui Makefile
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=xfce4_screensaver_preferences_ui $< >$@

DISTCLEANFILES = \
	$(xfce4_screensaver_dialog_built_sources)       \
	$(xfce4_screensaver_preferences_built_sources)

BUILT_SOURCES += \
	$(xfce4_screensaver_dialog_built_sources)       \
	$(xfce4_screensaver_preferences_built_sources)

endif

EXTRA_DIST = \
	debug-screensaver.sh              \
	gs-marshal.list                   \
	xfce4-screensaver.desktop.in      \
	xfce4-screensaver-configure       \
	xfce4-screensaver-dialog.css      \
	xfce4-screensaver-dialog.ui       \
	xfce4-screensaver-preferences.ui  \
	$(NULL)

GLIB_GENMARSHAL = $(shell pkg-config --variable=glib_genmarshal glib-2.0)

CLEANFILES = \
	$(desktop_DATA)               \
	xfce4-screensaver.desktop.in  \
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
