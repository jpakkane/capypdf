project('pdfgen', 'cpp', default_options : ['cpp_std=c++20'])

fmt_dep = dependency('fmt')
png_dep = dependency('libpng')
zlib_dep = dependency('zlib')
lcms_dep = dependency('lcms2')
freetype_dep = dependency('freetype2')
gtk_dep = dependency('gtk4')

pdflib = library('mypdf',
  'pdfgen.cpp',
  'pdfpage.cpp',
  'pdfdocument.cpp',
  'imageops.cpp',
  'utils.cpp',
  'pdfcolorconverter.cpp',
  'fontsubsetter.cpp',
  'ft_subsetter.cpp',
  'pdfcapi.cpp',
  dependencies: [fmt_dep, png_dep, zlib_dep, lcms_dep, freetype_dep]
)

executable('pdfgen',
  'main.cpp',
  link_with: pdflib,
)

executable('covergen',
  'cover.cpp',
  link_with: pdflib,
)

executable('fontgen',
  'fontgen.cpp',
  link_with: pdflib)

executable('gstategen',
  'gstategen.cpp',
  link_with: pdflib,
  dependencies: [fmt_dep])

executable('pdfviewer', 'pdfviewer.cpp',
  dependencies: [zlib_dep, gtk_dep])

executable('fontsubset', 'fontsubset.cpp', 'ft_subsetter.cpp',
  dependencies: freetype_dep)
