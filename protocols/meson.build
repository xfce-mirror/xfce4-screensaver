wayland_scanner_path = wayland_scanner.get_variable(pkgconfig: 'wayland_scanner')
wayland_scanner_bin = find_program(wayland_scanner_path, required: true)
wayland_protocols_pkgdatadir = wayland_protocols.get_variable(pkgconfig: 'pkgdatadir')

protocols = [
  # FIXME: Stop shipping ext-session-lock-v1.xml in-tree
  # when requiring wayland-protocols 1.25 is acceptable.
  'ext-session-lock-v1',
  # FIXME: Stop shipping ext-idle-notify-v1.xml in-tree
  # when requiring wayland-protocols 1.27 is acceptable.
  'ext-idle-notify-v1',
]

wayland_protocols_generated_sources = []

foreach protocol : protocols
  if fs.exists(wayland_protocols_pkgdatadir / 'staging' / protocol / '@0@.xml'.format(protocol))
    file = wayland_protocols_pkgdatadir / 'staging' / protocol / '@0@.xml'.format(protocol)
  elif fs.exists('@0@.xml'.format(protocol))
    file = '@0@.xml'.format(protocol)
  else
    error('The wayland-protocols you installed does not support @0@'.format(protocol))
  endif

  client_header = custom_target(
    '@0@-client-header'.format(protocol),
    input: file,
    output: '@0@-client.h'.format(protocol),
    command: [wayland_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  private_code = custom_target(
    '@0@-private-code'.format(protocol),
    input: file,
    output: '@0@.c'.format(protocol),
    command: [wayland_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  wayland_protocols_generated_sources += [client_header, private_code]
endforeach
