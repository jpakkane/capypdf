project('a4pdf', 'cpp',
  version: '0.1.0',
  default_options : ['cpp_std=c++20'])

version = meson.project_version()
cdata = configuration_data()
cdata.set_quoted('A4PDF_VERSION_STR', version)

fmt_dep = dependency('fmt')
png_dep = dependency('libpng')
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')
jpeg_dep = dependency('libjpeg')
freetype_dep = dependency('freetype2')
gtk_dep = dependency('gtk4')

pubinc = include_directories('include')

subdir('include')
subdir('src')
subdir('test')
