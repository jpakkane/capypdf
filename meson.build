project('pdfgen', 'cpp', default_options : ['cpp_std=c++20'])

fmt_dep = dependency('fmt')

executable('pdfgen', 'main.cpp', 'pdfgen.cpp', 'pdfpage.cpp',
  dependencies: fmt_dep)
