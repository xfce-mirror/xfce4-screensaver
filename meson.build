project(
  'xfce4-screensaver',
  'c',
version : '4.20.0-dev',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.60.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'apps'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2025'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.50.0',
  'gtk': '>= 3.24.0',
  'dbus': '>= 1.9.2',
  'dbus-glib': '>= 0.30',
  'xfce4': '>= 4.16.0',
  'libxfce4ui': '>= 4.18.4',

  'x11': '>= 1.6.7',
  'xext': '>= 1.0.0',
  'xscrnsaver': '>= 1.2.3',
  'libxklavier': '>= 5.2',
  'libwnck': '>= 3.20',

  'libwlembed': '>= 0.0.0',
  'wayland': '>= 1.15',
  'wayland-protocols': '>= 1.20',
  'libxfce4windowing': '>= 4.19.2',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
dbus = dependency('dbus-1', version: dependency_versions['dbus-glib'])
dbus_glib = dependency('dbus-glib-1', version: dependency_versions['dbus-glib'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfce4'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['xfce4'])
garcon = dependency('garcon-1', version: dependency_versions['xfce4'])
garcon_gtk = dependency('garcon-gtk3-1', version: dependency_versions['xfce4'])
libm = cc.find_library('m')

# Feature: 'x11'
x11_deps = []
x11_deps += dependency('gdk-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('gtk+-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('x11', version: dependency_versions['x11'], required: get_option('x11'))
x11_deps += dependency('xext', version: dependency_versions['xext'], required: get_option('x11'))
x11_deps += dependency('xscrnsaver', version: dependency_versions['xscrnsaver'], required: get_option('x11'))
x11_deps += dependency('libxklavier', version: dependency_versions['libxklavier'], required: get_option('x11'))
# FIXME: to be removed when libxfce4windowing is an acceptable requirement
# and reassemble the code from gs-listener-{x11,wayland}.c in gs-listener.c
x11_deps += dependency('libwnck-3.0', version: dependency_versions['libwnck'], required: get_option('x11'))

# Feature: 'wayland'
wayland_deps = []
wayland_deps += dependency('gdk-wayland-3.0', version: dependency_versions['gtk'], required: get_option('wayland'))
wayland_deps += dependency('libwlembed-0', version: dependency_versions['libwlembed'], required: get_option('wayland'))
wayland_deps += dependency('libwlembed-gtk3-0', version: dependency_versions['libwlembed'], required: get_option('wayland'))
wayland_deps += dependency('libxfce4windowing-0', version: dependency_versions['libxfce4windowing'], required: get_option('wayland'))
wayland_deps += dependency('wayland-client', version: dependency_versions['wayland'], required: get_option('wayland'))
wayland_deps += dependency('wayland-protocols', version: dependency_versions['wayland-protocols'], required: get_option('wayland'))
wayland_protocols = wayland_deps[-1]
wayland_scanner = dependency('wayland-scanner', version: dependency_versions['wayland'], required: get_option('wayland'), native: true)

xmlto = find_program('xmlto', required: get_option('docs'))

feature_cflags = []

functions = [
  'getresuid',
  'setpriority',
]
foreach function : functions
  if cc.has_function(function)
    feature_cflags += '-DHAVE_@0@=1'.format(function.to_upper())
  endif
endforeach

if cc.check_header('unistd.h')
  feature_cflags += '-DHAVE_UNISTD_H=1'
endif

enable_x11 = not get_option('x11').disabled()
foreach dep : x11_deps
  enable_x11 = enable_x11 and dep.found()
endforeach

enable_wayland = not get_option('wayland').disabled()
foreach dep : wayland_deps
  enable_wayland = enable_wayland and dep.found()
endforeach

if not enable_x11 and not enable_wayland
  error('At least one of the X11 and Wayland backends must be enabled')
endif

if enable_wayland
  feature_cflags += '-DENABLE_WAYLAND=1'
endif

if enable_x11
  feature_cflags += '-DENABLE_X11=1'

  # Path to xscreensaver hack executables
  xscreensaverhackdir = ''
  if get_option('xscreensaverhackdir') == 'yes' or get_option('xscreensaverhackdir') == ''
    foreach dir : [
      '/usr' / 'X11R6' / 'lib' / 'xscreensaver',
      get_option('prefix') / 'libexec' / 'xscreensaver',
      '/usr' / 'libexec' / 'xscreensaver',
      '/usr' / 'lib' / 'xscreensaver',
      '/usr' / 'lib64' / 'xscreensaver',
    ]
      if fs.exists(dir)
        xscreensaverhackdir = '@0@'.format(dir)
        break
      endif
    endforeach
  elif get_option('xscreensaverhackdir') != 'no'
    xscreensaverhackdir = get_option('xscreensaverhackdir')
  endif

  if xscreensaverhackdir != ''
    message('Found directory containing xscreensaver hacks: @0@'.format(xscreensaverhackdir))
    feature_cflags += '-DXSCREENSAVER_HACK_DIR="@0@"'.format(xscreensaverhackdir)
  else
    warning('Could not find directory containing xscreensaver hacks')
  endif

  if get_option('kbd-layout-indicator')
    feature_cflags += '-DWITH_KBD_LAYOUT_INDICATOR=1'
  endif
endif

# Disable support for locking the display
if get_option('authentication-scheme') == 'none'
  feature_cflags += '-DNO_LOCKING=1'
endif

# Check for bsd_auth(3) (OpenBSD)
if get_option('authentication-scheme') == 'bsdauth'
  if host_machine.system() == 'openbsd' and cc.links(files('build-aux' / 'bsdauth' / 'test-build.c'))
    feature_cflags += '-DHAVE_BSDAUTH=1'
  else
    error('bsd_auth(3) support requested but not available')
  endif
endif

auth_deps = []

# Check for PAM
if get_option('authentication-scheme') == 'pam'
  pam = dependency('pam', required: false)
  if pam.found()
    auth_deps += pam
  else
    auth_deps += cc.find_library('pam')
  endif

  feature_cflags += '-DHAVE_PAM=1'

  # On Linux, sigtimedwait() is in libc; on Solaris, it's in librt.
  if not cc.has_function('sigtimedwait')
    auth_deps += cc.find_library('rt')
  endif

  # Check how to call pam_strerror
  if cc.compiles(files('build-aux' / 'pam' / 'test-pam-strerror-2.c'))
    message('Number of arguments taken by pam_strerror: 2')
    feature_cflags += '-DPAM_STRERROR_TWO_ARGS=1'
    pam_strerror_args = 2
  elif cc.compiles(files('build-aux' / 'pam' / 'test-pam-strerror-1.c'))
    message('Number of arguments taken by pam_strerror: 1')
    pam_strerror_args = 1
  else
    error('Fail to figure out how to call pam_strerror')
  endif

  # Check if Linux-PAM is used
  # OpenPAM has security/pam_types.h instead.
  if pam.found() and cc.check_header('security/_pam_types.h')
    feature_cflags += '-DHAVE_LINUX_PAM=1'
    linux_pam_localedir = get_option('linux-pam-localedir')
    if linux_pam_localedir == ''
      linux_pam_localedir = pam.get_variable(pkgconfig: 'prefix', default_value: get_option('prefix')) / get_option('datadir') / 'locale'
    endif
    feature_cflags += '-DLINUXPAMLOCALEDIR="@0@"'.format(linux_pam_localedir)
  endif

  if cc.has_function('pam_fail_delay', dependencies: auth_deps)
    feature_cflags += '-DHAVE_PAM_FAIL_DELAY=1'
  endif
endif

# Check for the nine billion variants of shadow passwords
if get_option('authentication-scheme') == 'pwent'
  if get_option('shadow')
    # Check for Sun "adjunct" passwords
    if cc.compiles(files('build-aux' / 'pwent' / 'test-sun-shadow-passwd.c'))
      message('pwent: adjunct shadow password detected')
      feature_cflags += '-DHAVE_ADJUNCT_PASSWD=1'
    # Check for DEC and SCO so-called "enhanced" security
    elif cc.compiles(files('build-aux' / 'pwent' / 'test-dec-shadow-passwd.c'))
      message('pwent: enhanced shadow password detected')
      feature_cflags += '-DHAVE_ENHANCED_PASSWD=1'
      # On SCO, getprpwnam() is in -lprot (which uses nap() from -lx)
      # (I'm told it needs -lcurses too, but I don't understand why)
      # But on DEC, it's in -lsecurity
      foreach dep : ['prot', 'curses', 'x', 'security']
        auth_deps += cc.find_library(dep, required: false)
      endforeach
    # Check for HP's entry in the "Not Invented Here" Sweepstakes
    elif cc.compiles(files('build-aux' / 'pwent' / 'test-hp-shadow-passwd.c'))
      message('pwent: HPUX shadow password detected')
      feature_cflags += '-DHAVE_HPUX_PASSWD=1'
      # on HPUX, bigcrypt is in -lsec
      auth_deps += cc.find_library('sec', required: false)
    # Check for "traditional" shadow passwords
    elif cc.compiles(files('build-aux' / 'pwent' / 'test-generic-shadow-passwd.c'))
      message('pwent: traditional shadow password detected')
      feature_cflags += '-DHAVE_SHADOW_PASSWD=1'
      # On some systems (UnixWare 2.1), getspnam() is in -lgen instead of -lc
      if not cc.has_function('getspnam')
        auth_deps += cc.find_library('gen')
      endif
    endif
  endif

  # Check for other libraries needed for non-shadow passwords
  crypt = dependency('libcrypt', 'libxcrypt', required: false)
  if not crypt.found()
    crypt = cc.find_library('crypt')
  endif
  auth_deps += crypt
  if cc.has_function('crypt', dependencies: crypt)
    feature_cflags += '-DHAVE_LIBCRYPT=1'
  endif
endif

# Check for external password helper
# On some BSD systems, they fork an external program that takes
# the password on stdin, and returns true if that password is a
# valid one. Then only that smaller program needs to be setuid.
if get_option('authentication-scheme') == 'helper'
  if get_option('passwd-helper') == ''
    error('You must provide full pathname of helper via the passwd-helper meson option')
  endif
  feature_cflags += '-DPASSWD_HELPER_PROGRAM="@0@"'.format(get_option('passwd-helper'))
endif

session_manager_deps = []

# Consolekit / systemd / elogind
if get_option('session-manager') == 'systemd'
  session_manager_deps += dependency('libsystemd')
  feature_cflags += '-DWITH_SYSTEMD=1'
elif get_option('session-manager') == 'elogind'
  session_manager_deps += dependency('libelogind')
  feature_cflags += '-DWITH_ELOGIND=1'
elif get_option('session-manager') == 'consolekit'
  feature_cflags += '-DWITH_CONSOLE_KIT=1'
endif

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_URL="https://docs.xfce.org/@0@/@1@/start"'.format(project_namespace, meson.project_name()),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DXFCELOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

if enable_wayland
  subdir('protocols')
endif
subdir('src')
subdir('savers')
subdir('data')
subdir('doc')
subdir('po')
