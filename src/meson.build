i18n.merge_file(
  input: 'xfce4-screensaver.desktop.in',
  output: 'xfce4-screensaver.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'autostart',
)

install_data(
  'xfce4-screensaver-configure.py',
  install_dir: get_option('prefix') / get_option('libexecdir'),
  install_mode: 'rwxr-xr-x',
)

c_args = [
  '-DSAVERDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir') / 'xfce4-screensaver'),
  '-DPAM_SERVICE_NAME="@0@"'.format('xfce4-screensaver'),
  '-DCONFIGURE_COMMAND="@0@"'.format(get_option('prefix') / get_option('libexecdir') / 'xfce4-screensaver-configure.py'),
]

xfce4_screensaver_command_sources = [
  'xfce4-screensaver-command.c',
  xfce_revision_h,
]

executable(
  'xfce4-screensaver-command',
  xfce4_screensaver_command_sources,
  c_args: c_args,
  dependencies: [
    glib,
    gio,
    libxfce4util,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

auth_sources = [
  'gs-auth.h',
]
if get_option('authentication-scheme') != 'none'
  auth_sources += [
    'gs-auth-@0@.c'.format(get_option('authentication-scheme')),
  ]
endif

test_passwd_sources = [
  'setuid.c',
  'setuid.h',
  'subprocs.c',
  'subprocs.h',
  'test-passwd.c',
  auth_sources,
]

executable(
  'test-passwd',
  test_passwd_sources,
  c_args: c_args,
  dependencies: [
    auth_deps,
    glib,
    gtk,
    libxfce4util,
  ],
  install: false,
)

gs_marshal = gnome.genmarshal(
  'gs-marshal',
  sources: 'gs-marshal.list',
  prefix: 'gs_marshal',
  internal: true,
  install_header: false,
)

test_window_sources = [
  'gs-debug.c',
  'gs-debug.h',
  'gs-job.c',
  'gs-job.h',
  'gs-manager.c',
  'gs-manager.h',
  'gs-prefs.c',
  'gs-prefs.h',
  'gs-theme-manager.c',
  'gs-theme-manager.h',
  'gs-window.c',
  'gs-window.h',
  'subprocs.c',
  'subprocs.h',
  'test-window.c',
  'xfce-desktop-utils.c',
  'xfce-desktop-utils.h',
  gs_marshal,
]

if enable_x11
  test_window_sources += [
    'gs-grab.c',
    'gs-grab.h',
  ]
endif
if enable_wayland
  test_window_sources += [
    'gs-session-lock-manager.c',
    'gs-session-lock-manager.h',
    wayland_protocols_generated_sources,
  ]
endif

executable(
  'test-window',
  test_window_sources,
  c_args: c_args,
  dependencies: [
    auth_deps,
    dbus_glib,
    garcon,
    garcon_gtk,
    glib,
    gtk,
    libm,
    libxfce4ui,
    libxfce4util,
    wayland_deps,
    x11_deps,
    xfconf,
  ],
  include_directories: [
    include_directories('..'),
  ],
  install: false,
)

xfce4_screensaver_resources = gnome.compile_resources(
  'xfce4-screensaver-resources',
  'xfce4-screensaver.gresource.xml',
)

xfce4_screensaver_dialog_sources = [
  'gs-debug.c',
  'gs-debug.h',
  'gs-lock-plug.c',
  'gs-lock-plug.h',
  'gs-prefs.c',
  'gs-prefs.h',
  'setuid.c',
  'setuid.h',
  'subprocs.c',
  'subprocs.h',
  'xfce4-screensaver-dialog.c',
  'xfce-bg.c',
  'xfce-bg.h',
  'xfce-desktop-utils.c',
  'xfce-desktop-utils.h',
  auth_sources,
  xfce_revision_h,
  xfce4_screensaver_resources,
]

if enable_x11
  xfce4_screensaver_dialog_sources += [
    'xfcekbd-indicator.c',
    'xfcekbd-indicator.h',
  ]
endif

executable(
  'xfce4-screensaver-dialog',
  xfce4_screensaver_dialog_sources,
  c_args: c_args,
  dependencies: [
    auth_deps,
    dbus_glib,
    glib,
    gtk,
    libm,
    libxfce4ui,
    libxfce4util,
    wayland_deps,
    x11_deps,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libexecdir'),
)

xfce4_screensaver_sources = [
  'gs-debug.c',
  'gs-debug.h',
  'gs-job.c',
  'gs-job.h',
  'gs-listener.c',
  'gs-listener-dbus.c',
  'gs-listener-dbus.h',
  'gs-listener.h',
  'gs-manager.c',
  'gs-manager.h',
  'gs-monitor.c',
  'gs-monitor.h',
  'gs-prefs.c',
  'gs-prefs.h',
  'gs-theme-manager.c',
  'gs-theme-manager.h',
  'gs-window.c',
  'gs-window.h',
  'subprocs.c',
  'subprocs.h',
  'xfce4-screensaver.c',
  'xfce4-screensaver.h',
  'xfce-desktop-utils.c',
  'xfce-desktop-utils.h',
  gs_marshal,
  xfce_revision_h,
  xfce4_screensaver_resources,
]

if enable_x11
  xfce4_screensaver_sources += [
    'gs-grab.c',
    'gs-grab.h',
    'gs-listener-x11.c',
    'gs-listener-x11.h',
  ]
endif
if enable_wayland
  xfce4_screensaver_sources += [
    'gs-listener-wayland.c',
    'gs-listener-wayland.h',
    'gs-session-lock-manager.c',
    'gs-session-lock-manager.h',
    wayland_protocols_generated_sources,
  ]
endif

executable(
  'xfce4-screensaver',
  xfce4_screensaver_sources,
  c_args: c_args,
  dependencies: [
    dbus_glib,
    garcon,
    garcon_gtk,
    glib,
    gtk,
    libm,
    libxfce4ui,
    libxfce4util,
    session_manager_deps,
    wayland_deps,
    x11_deps,
    xfconf,
  ],
  include_directories: [
    include_directories('..'),
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

xfce4_screensaver_preferences_sources = [
  'gs-debug.c',
  'gs-debug.h',
  'gs-job.c',
  'gs-job.h',
  'gs-prefs.c',
  'gs-prefs.h',
  'gs-theme-manager.c',
  'gs-theme-manager.h',
  'subprocs.c',
  'subprocs.h',
  'xfce4-screensaver-preferences.c',
  'xfce-desktop-utils.c',
  'xfce-desktop-utils.h',
  xfce4_screensaver_resources,
]

executable(
  'xfce4-screensaver-preferences',
  xfce4_screensaver_preferences_sources,
  c_args: c_args,
  dependencies: [
    garcon,
    garcon_gtk,
    glib,
    gtk,
    libm,
    libxfce4ui,
    libxfce4util,
    wayland_deps,
    x11_deps,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)
