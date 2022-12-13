project('pdfgen', 'cpp', default_options : ['cpp_std=c++20'])

fmt_dep = dependency('fmt')
png_dep = dependency('libpng')
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')

pdflib = static_library('mypdf',
  'pdfgen.cpp',
  'pdfpage.cpp',
  'imageops.cpp',
  'utils.cpp',
  'pdfcolorconverter.cpp',
  dependencies: [fmt_dep, png_dep, zlib_dep, lcms_dep]
)

executable('pdfgen',
  'main.cpp',
  link_with: pdflib,
)

executable('covergen',
  'cover.cpp',
  link_with: pdflib,
)
