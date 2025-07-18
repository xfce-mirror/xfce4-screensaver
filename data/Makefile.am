SUBDIRS = \
	icons	\
	images

NULL =

man_MANS = \
	xfce4-screensaver.1              \
	xfce4-screensaver-command.1      \
	xfce4-screensaver-preferences.1  \
	$(NULL)

xfce4-screensaver-preferences.desktop: xfce4-screensaver-preferences.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-screensaver-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

# FIXME: xgettext doesn't recognize .directory.in files as desktop files so we
# use .desktop.in for the translation: is there a proper solution?
xfce4-screensaver.directory: xfce4-screensaver.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

directorydir = $(datadir)/desktop-directories
directory_in_files = \
	xfce4-screensaver.desktop.in  \
	$(NULL)
directory_DATA = $(directory_in_files:.desktop.in=.directory)

menudir = $(sysconfdir)/xdg/menus
menu_DATA = \
	xfce4-screensavers.menu  \
	$(NULL)

dbussessionservicedir = $(DBUS_SESSION_SERVICE_DIR)
dbussessionservice_DATA = org.xfce.ScreenSaver.service

EXTRA_DIST = \
	$(desktop_in_files)              \
	$(directory_in_files)            \
	$(man_MANS)                      \
	xfce4-screensaver.common-auth    \
	xfce4-screensaver.system-auth    \
	xfce4-screensavers.menu          \
	org.xfce.ScreenSaver.service.in  \
	meson.build                      \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)    \
	$(directory_DATA)  \
	$(NULL)

MAINTAINERCLEANFILES = \
	*~           \
	Makefile.in

install-data-hook:
	@system=`uname`;                                                                                                \
	if test -f /usr/include/security/pam_appl.h; then                                                               \
	    if test '!' -d $(DESTDIR)$(PAM_PREFIX)/pam.d; then                                                          \
	        $(mkinstalldirs) $(DESTDIR)$(PAM_PREFIX)/pam.d;                                                         \
	        chmod 755 $(DESTDIR)$(PAM_PREFIX)/pam.d;                                                                \
	    fi;                                                                                                         \
	    if test $$system = Linux; then                                                                              \
	        if test '!' -f $(DESTDIR)$(PAM_PREFIX)/pam.d/xfce4-screensaver; then                                    \
	            if test "x$(PAM_AUTH_TYPE)" = "xsystem"; then                                                       \
	                $(INSTALL_DATA) xfce4-screensaver.system-auth $(DESTDIR)$(PAM_PREFIX)/pam.d/xfce4-screensaver;  \
	            else                                                                                                \
	                $(INSTALL_DATA) xfce4-screensaver.common-auth $(DESTDIR)$(PAM_PREFIX)/pam.d/xfce4-screensaver;  \
	            fi;                                                                                                 \
	        fi;                                                                                                     \
	        if test "x$(PAM_PREFIX)" != "x/etc"; then                                                               \
	            echo "***" ;                                                                                        \
	            echo "***  Warning: xfce4-screensaver has been compiled with support for" ;                         \
	            echo "***           Pluggable Authentication Modules (PAM).  However, you may need to" ;            \
	            echo "***           manually install $(PAM_PREFIX)/pam.d/xfce4-screensaver" ;                       \
	            echo "***           to /etc/pam.d/xfce4-screensaver.  Otherwise, unlocking might" ;                 \
	            echo "***           not work." ;                                                                    \
	            echo "***" ;                                                                                        \
	        fi;                                                                                                     \
	    fi;                                                                                                         \
	    if test $$system = SunOS; then                                                                              \
	        echo "***" ;                                                                                            \
	        echo "***  Warning: xfce4-screensaver has been compiled with support for" ;                             \
	        echo "***           Pluggable Authentication Modules (PAM).  However, you" ;                            \
	        echo "***           need to manually add authentication for xfce4-screensaver in" ;                     \
	        echo "***           $(PAM_PREFIX)/pam.conf or /etc/pam.conf." ;                                         \
	        echo "***           Otherwise, unlocking might not work." ;                                             \
	        echo "***" ;                                                                                            \
	    fi;                                                                                                         \
	fi
