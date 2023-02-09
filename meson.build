project('a4pdf', 'cpp',
  version: '0.1.0',
  default_options : ['cpp_std=c++20'])

fmt_dep = dependency('fmt')
png_dep = dependency('libpng')
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')
freetype_dep = dependency('freetype2')
gtk_dep = dependency('gtk4')

pubinc = include_directories('include')

subdir('src')
