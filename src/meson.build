a4pdf_lib = library('a4pdf',
  'pdfgen.cpp',
  'pdfpagebuilder.cpp',
  'pdfdocument.cpp',
  'imageops.cpp',
  'utils.cpp',
  'pdfcolorconverter.cpp',
  'fontsubsetter.cpp',
  'ft_subsetter.cpp',
  'pdfcapi.cpp',
  include_directories: [pubinc],
  dependencies: [fmt_dep, png_dep, zlib_dep, jpeg_dep, lcms_dep, freetype_dep]
)

a4pdf_dep = declare_dependency(include_directories: pubinc,
  link_with: a4pdf_lib
)

executable('pdfgen',
  'main.cpp',
  dependencies: a4pdf_dep,
)

executable('covergen',
  'cover.cpp',
  dependencies: a4pdf_dep,
)

executable('fontgen',
  'fontgen.cpp',
  dependencies: a4pdf_dep,
)

executable('gstategen',
  'gstategen.cpp',
  dependencies: [a4pdf_dep, fmt_dep]
)

executable('pathgen',
  'pathgen.cpp',
  dependencies: [a4pdf_dep]
)

executable('patterngen',
  'patterngen.cpp',
  dependencies: [a4pdf_dep]
)

executable('labtest', 'labtest.cpp',
  dependencies: [a4pdf_dep]
)

executable('imagetest', 'imagetest.cpp',
  dependencies: [a4pdf_dep]
)

executable('pdfviewer', 'pdfviewer.cpp',
  dependencies: [zlib_dep, gtk_dep])

executable('fontsubset', 'fontsubset.cpp', 'ft_subsetter.cpp',
  dependencies: freetype_dep)

