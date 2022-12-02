project('pdfgen', 'cpp', default_options : ['cpp_std=c++20'])

fmt_dep = dependency('fmt')
png_dep = dependency('libpng')

executable('pdfgen',
  'main.cpp',
  'pdfgen.cpp',
  'pdfpage.cpp',
  'imageops.cpp',
  dependencies: [fmt_dep, png_dep]
)
