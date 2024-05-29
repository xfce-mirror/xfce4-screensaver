NULL =

if ENABLE_WAYLAND

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libprotocols\" \
	$(PLATFORM_CPPFLAGS) \
	$(NULL)

noinst_LTLIBRARIES = \
	libprotocols.la

libprotocols_built_sources = \
	ext-session-lock-v1.c \
	ext-session-lock-v1-client.h \
	ext-idle-notify-v1.c \
	ext-idle-notify-v1-client.h \
	$(NULL)

nodist_libprotocols_la_SOURCES = \
	$(libprotocols_built_sources)

libprotocols_la_CFLAGS = \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(NULL)

libprotocols_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS) \
	$(NULL)

libprotocols_la_LIBADD = \
	$(WAYLAND_CLIENT_LIBS) \
	$(NULL)

if HAVE_SESSION_LOCK
%.c: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-session-lock/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-session-lock/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@
else
%.c: %.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: %.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@
endif

if HAVE_IDLE_NOTIFY
%.c: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-idle-notify/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-idle-notify/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@
else
%.c: %.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: %.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@
endif

DISTCLEANFILES = \
	$(libprotocols_built_sources) \
	$(NULL)

BUILT_SOURCES = \
	$(libprotocols_built_sources) \
	$(NULL)

endif # ENABLE_WAYLAND

EXTRA_DIST =

if !HAVE_SESSION_LOCK
EXTRA_DIST += \
	ext-session-lock-v1.xml
endif

if !HAVE_IDLE_NOTIFY
EXTRA_DIST += \
	ext-idle-notify-v1.xml
endif
