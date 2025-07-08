subdir('icons')
subdir('images')

install_man('xfce4-screensaver-command.1')
install_man('xfce4-screensaver-preferences.1')
install_man('xfce4-screensaver.1')

i18n.merge_file(
  input: 'xfce4-screensaver-preferences.desktop.in',
  output: 'xfce4-screensaver-preferences.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

# xgettext doesn't recognize .directory.in files as desktop files so we
# use .desktop.in for the translation.
i18n.merge_file(
  input: 'xfce4-screensaver.desktop.in',
  output: 'xfce4-screensaver.directory',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'desktop-directories',
)

install_data(
  'xfce4-screensavers.menu',
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'menus',
)

configure_file(
  configuration: configuration_data({
    'EXPANDED_BINDIR': get_option('prefix') / get_option('bindir'),
  }),
  input: 'org.xfce.ScreenSaver.service.in',
  output: 'org.xfce.ScreenSaver.service',
  install: true,
  install_dir: dbus.get_variable(
    pkgconfig: 'session_bus_services_dir',
    pkgconfig_define: ['datadir', get_option('prefix') / get_option('datadir')],
  )
)

pam_auth_type = get_option('pam-auth-type')
pam_prefix = get_option('pam-prefix')

if pam_prefix == ''
  pam_prefix = get_option('prefix') / get_option('sysconfdir')
endif

if host_machine.system() == 'linux'
  if pam_auth_type == 'common' or (pam_auth_type == 'auto' and fs.exists('/etc' / 'pam.d' / 'common-auth'))
    install_data(
      'xfce4-screensaver.common-auth',
      install_dir: pam_prefix / 'pam.d',
      rename: 'xfce4-screensaver',
    )
  else
    install_data(
      'xfce4-screensaver.system-auth',
      install_dir: pam_prefix / 'pam.d',
      rename: 'xfce4-screensaver',
    )
  endif

  if get_option('pam-prefix') != '/etc'
    warning('''
      Warning: xfce4-screensaver has been compiled with support for
      Pluggable Authentication Modules (PAM). However, you may need
      to manually install @0@/pam.d/xfce4-screensaver
      to /etc/pam.d/xfce4-screensaver. Otherwise, unlocking might
      not work.
    '''.format(pam_prefix))
  endif
endif
