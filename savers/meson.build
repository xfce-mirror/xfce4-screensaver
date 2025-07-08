savers = [
  'xfce-floaters',
  'xfce-personal-slideshow',
  'xfce-popsquares',
]

foreach saver : savers
  saver_desktop_in_file = configure_file(
    configuration: configuration_data({
      'SAVERDIR': get_option('prefix') / get_option('libexecdir') / 'xfce4-screensaver',
      'FLOATERS_XFCE_IMAGE': get_option('prefix') / get_option('datadir') / 'pixmaps' / 'xfce-logo-white.svg',
    }),
    input: '@0@.desktop.in.in'.format(saver),
    output: '@0@.desktop.in'.format(saver),
    install: false,
  )

  i18n.merge_file(
    input: saver_desktop_in_file,
    output: '@0@.desktop'.format(saver),
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications' / 'screensavers',
  )
endforeach

gs_theme_engine_sources = [
  'gs-theme-engine.c',
  'gs-theme-engine.h',
]

gs_theme_engine_sources += gnome.genmarshal(
  'gs-theme-engine-marshal',
  sources: 'gs-theme-engine-marshal.list',
  prefix: 'gs_theme_engine_marshal',
  internal: true,
  install_header: false,
)

gs_theme_engine = static_library(
  'gs-theme-engine',
  gs_theme_engine_sources,
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    wayland_deps,
    x11_deps,
  ],
  install: false,
)

floaters_sources = [
  'floaters.c',
]

executable(
  'floaters',
  floaters_sources,
  dependencies: [
    glib,
    gtk,
    libm,
    libxfce4util,
    wayland_deps,
    x11_deps,
  ],
  link_with: [
    gs_theme_engine,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libexecdir') / 'xfce4-screensaver',
)

popsquares_sources = [
  'gste-popsquares.c',
  'gste-popsquares.h',
  'popsquares.c',
]

executable(
  'popsquares',
  popsquares_sources,
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    wayland_deps,
    x11_deps,
  ],
  link_with: [
    gs_theme_engine,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libexecdir') / 'xfce4-screensaver',
)

slideshow_sources = [
  'gste-slideshow.c',
  'gste-slideshow.h',
  'slideshow.c',
  'xdg-user-dir-lookup.c',
  'xdg-user-dir-lookup.h',
]

executable(
  'slideshow',
  slideshow_sources,
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    wayland_deps,
    x11_deps,
  ],
  link_with: [
    gs_theme_engine,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libexecdir') / 'xfce4-screensaver',
)
