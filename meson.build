project('pdfgen', 'cpp', default_options : ['cpp_std=c++20'])

fmt_dep = dependency('fmt')
png_dep = dependency('libpng')
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')

executable('pdfgen',
  'main.cpp',
  'pdfgen.cpp',
  'pdfpage.cpp',
  'imageops.cpp',
  dependencies: [fmt_dep, png_dep, zlib_dep, lcms_dep]
)
