SUBDIRS =			\
	images

NULL =

man_MANS = 				\
	xfce4-screensaver.1		\
	xfce4-screensaver-command.1	\
	xfce4-screensaver-preferences.1	\
	$(NULL)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-screensaver-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DIRECTORY_RULE@

directorydir = $(datadir)/desktop-directories
directory_in_files =                            \
	xfce4-screensaver.directory.in		\
	$(NULL)
directory_DATA = $(directory_in_files:.directory.in=.directory)

menudir = $(sysconfdir)/xdg/menus
menu_DATA =					\
	xfce4-screensavers.menu			\
	$(NULL)

gtkbuilderdir = $(pkgdatadir)
gtkbuilder_DATA = 					\
	xfce4-screensaver-preferences.ui	\
	lock-dialog-default.ui		\
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xfce4-screensaver.pc

dbussessionservicedir = $(DBUS_SESSION_SERVICE_DIR)
dbussessionservice_DATA = org.xfce.ScreenSaver.service

@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@

gsettingsschema_in_files = org.xfce.screensaver.gschema.xml.in org.xfce.session.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)

EXTRA_DIST = 				\
	$(gsettingsschema_in_files)	\
	$(desktop_in_files)		\
	$(directory_in_files)		\
	$(gtkbuilder_DATA)		\
	$(man_MANS)			\
	xfce4-screensaver		\
	xfce4-screensaver.pc.in		\
	org.xfce.ScreenSaver.service.in\
	xscreensaver-config.xsl		\
	migrate-xscreensaver-config.sh	\
	$(NULL)

DISTCLEANFILES = 			\
	$(gsettings_SCHEMAS)	\
	$(desktop_DATA)			\
	$(directory_DATA)		\
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in     \
    $(gsettings_SCHEMAS:.xml=.valid)

install-data-hook:
	@system=`uname`; \
	if test -f /usr/include/security/pam_appl.h; then \
	  if test '!' -d $(DESTDIR)$(PAM_PREFIX)/pam.d; then \
		$(mkinstalldirs) $(DESTDIR)$(PAM_PREFIX)/pam.d; \
		chmod 755 $(DESTDIR)$(PAM_PREFIX)/pam.d; \
	   fi; \
	   if test $$system = Linux; then \
		if test '!' -f $(DESTDIR)$(PAM_PREFIX)/pam.d/xfce4-screensaver; then \
		     $(INSTALL_DATA) xfce4-screensaver $(DESTDIR)$(PAM_PREFIX)/pam.d/xfce4-screensaver; \
		fi; \
		if test "x$(PAM_PREFIX)" != "x/etc/pam.d"; then \
	             echo "***" ; \
	             echo "***  Warning: xfce4-screensaver has been compiled with support for" ; \
	             echo "***           Pluggable Authentication Modules (PAM).  However, you may need to" ; \
	             echo "***           manually install $(PAM_PREFIX)/pam.d/xfce4-screensaver" ; \
	             echo "***           to /etc/pam.d/xfce4-screensaver.  Otherwise, unlocking might" ; \
	             echo "***           not work." ; \
	             echo "***" ; \
	             echo "***           Note: If you are using SuSE/Novell you may have to modify this file." ; \
	             echo "***" ; \
		fi; \
	   fi; \
	   if test $$system = SunOS; then \
	        echo "***" ; \
	        echo "***  Warning: xfce4-screensaver has been compiled with support for" ; \
	        echo "***           Pluggable Authentication Modules (PAM).  However, you" ; \
	        echo "***           need to manually add authentication for xfce4-screensaver in" ; \
	        echo "***           $(PAM_PREFIX)/pam.conf or /etc/pam.conf." ; \
	        echo "***           Otherwise, unlocking might not work." ; \
	        echo "***" ; \
	   fi; \
	fi
