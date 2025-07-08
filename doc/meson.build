docdir = get_option('docdir')
if docdir == ''
  docdir = get_option('prefix') / get_option('datadir') / 'doc' / '@0@-@1@'.format(meson.project_name(), meson.project_version())
endif

if xmlto.found()
  xfce4_screensaver_xml = configure_file(
    configuration: configuration_data({
      'VERSION': meson.project_version(),
    }),
    input: 'xfce4-screensaver.xml.in',
    output: 'xfce4-screensaver.xml',
  )

  dbus_interface_xml = configure_file(
    input: 'dbus-interface.xml',
    output: 'dbus-interface.xml',
    copy: true,
  )

  configure_file(
    input: ['config.xsl', xfce4_screensaver_xml, dbus_interface_xml],
    output: 'xfce4-screensaver.html',
    command: [xmlto, '-o', '@OUTDIR@', 'html-nochunks', '-m', '@INPUT0@', '@INPUT1@'],
    install: true,
    install_dir: docdir,
  )

  install_data(
    'docbook.css',
    install_dir: docdir,
  )
endif
