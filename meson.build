project('capypdf', 'cpp', 'c',
  version: '0.7.0',
  default_options : ['cpp_std=c++23', 'default_library=static'])

version = meson.project_version()
varr = version.split('.')
cdata = configuration_data()
cdata.set_quoted('CAPYPDF_VERSION_STR', version)
cdata.set('CAPYPDF_VERSION_MAJOR', varr[0])
cdata.set('CAPYPDF_VERSION_MINOR', varr[1])
cdata.set('CAPYPDF_VERSION_MICRO', varr[2])
soversion = 0

header_install_subdir = 'capypdf-@0@'.format(soversion)

fmt_dep = dependency('fmt')
png_dep = dependency(['libpng', 'PNG'])
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')
jpeg_dep = dependency(['libjpeg', 'JPEG'])
freetype_dep = dependency(['freetype2', 'FreeType'])
tiff_dep = dependency(['libtiff-4', 'TIFF'])
gtk_dep = dependency('gtk4', required: false)

pubinc = include_directories('include')

subdir('include')
subdir('src')
subdir('python')
subdir('test')

pkg_mod = import('pkgconfig')
pkg_mod.generate(capypdf_lib,
  name: 'capypdf',
  filebase: 'capypdf',
  subdirs: header_install_subdir)
