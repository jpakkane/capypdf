project('capypdf', 'cpp', 'c',
  version: '0.16.99',
  meson_version: '>=1.5',
  default_options : ['cpp_std=c++23,c++latest', 'default_library=static'])

version = meson.project_version()
varr = version.split('.')
cdata = configuration_data()
cdata.set_quoted('CAPYPDF_VERSION_STR', version)
cdata.set('CAPYPDF_VERSION_MAJOR', varr[0])
cdata.set('CAPYPDF_VERSION_MINOR', varr[1])
cdata.set('CAPYPDF_VERSION_MICRO', varr[2])
soversion = 0

add_project_arguments('-D_GLIBCXX_ASSERTIONS', '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST',
  language: 'cpp')

header_install_subdir = 'capypdf-@0@'.format(soversion)

png_dep = dependency('libpng')
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')
jpeg_dep = dependency('libjpeg')
freetype_dep = dependency('freetype2')
tiff_dep = dependency('libtiff-4')

pubinc = include_directories('include')

if get_option('fuzzing')
  add_project_arguments('-DCAPYFUZZING', language: 'cpp')
endif

subdir('include')
subdir('src')
if get_option('devtools')
  subdir('devtools')
endif
if not get_option('fuzzing')
  subdir('python')
  subdir('benchmark')
  subdir('test')
endif

pkg_mod = import('pkgconfig')
pkg_mod.generate(capypdf_lib,
  name: 'capypdf',
  filebase: 'capypdf',
  subdirs: header_install_subdir)
