instdir = get_option('includedir') / header_install_subdir

configure_file(input: 'capypdf.h.in',
  output: 'capypdf.h',
  configuration: cdata,
  install: true,
  install_dir: instdir)

install_headers('capypdf.hpp', install_dir: instdir)
